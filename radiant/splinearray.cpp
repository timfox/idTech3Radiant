/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
any prior sources.
===========================================================================
*/

#include "splinearray.h"

#include "debugging/debugging.h"
#include "ientity.h"
#include "iselection.h"
#include "iundo.h"
#include "scenelib.h"
#include "entitylib.h"
#include "stream/stringstream.h"

#include "entity.h"
#include "map.h"
#include "grid.h"
#include "mainframe.h"

#include "../plugins/entity/curve.h"
#include "math/curve.h"

#include <vector>
#include <cmath>

namespace
{

Vector3 tangentToAngles( const Vector3& tangent ){
	if ( vector3_length_squared( tangent ) < 1e-10 ) {
		return Vector3( 0, 0, 0 );
	}
	Vector3 dir = vector3_normalised( tangent );
	float yaw = atan2( -dir.x(), dir.y() ) * 180.0 / 3.14159265f;
	float pitch = asin( dir.z() ) * 180.0 / 3.14159265f;
	return Vector3( pitch, 0, yaw );
}

} // namespace

void SplineArray_Selected(){
	if ( GlobalSelectionSystem().Mode() != SelectionSystem::ePrimitive ) {
		return;
	}
	if ( GlobalSelectionSystem().countSelected() < 2 ) {
		globalWarningStream() << "Spline Array: Select a misc_spline entity and at least one other entity to clone along the spline.\n";
		return;
	}

	Entity* splineEntity = nullptr;
	scene::Instance* splineInstance = nullptr;
	std::vector<scene::Node*> toClone;

	class CollectSplineAndTargets : public SelectionSystem::Visitor
	{
		Entity*& m_splineEntity;
		scene::Instance*& m_splineInstance;
		std::vector<scene::Node*>& m_toClone;
	public:
		CollectSplineAndTargets( Entity*& splineEntity, scene::Instance*& splineInstance, std::vector<scene::Node*>& toClone )
			: m_splineEntity( splineEntity ), m_splineInstance( splineInstance ), m_toClone( toClone ){
		}
		void visit( scene::Instance& instance ) const override {
			Entity* entity = Node_getEntity( instance.path().top() );
			if ( entity == nullptr ) return;
			if ( string_equal( entity->getClassName(), "misc_spline" ) ) {
				m_splineEntity = entity;
				m_splineInstance = &instance;
			}
			else
			{
				m_toClone.push_back( &instance.path().top().get() );
			}
		}
	};

	GlobalSelectionSystem().foreachSelected( CollectSplineAndTargets( splineEntity, splineInstance, toClone ) );

	if ( splineEntity == nullptr ) {
		globalWarningStream() << "Spline Array: No misc_spline entity selected. Add a misc_spline and select it with the entities to array.\n";
		return;
	}
	if ( toClone.empty() ) {
		globalWarningStream() << "Spline Array: Select at least one entity (besides the spline) to clone along the path.\n";
		return;
	}

	ControlPoints controlPoints;
	bool useBezier = false;
	const char* curveBezier = splineEntity->getKeyValue( "curve_Bezier" );
	if ( !string_empty( curveBezier ) && ControlPoints_parse( controlPoints, curveBezier ) ) {
		useBezier = true;
	}
	else
	{
		controlPoints.resize( 0 );
		const char* curveCatmull = splineEntity->getKeyValue( "curve_CatmullRomSpline" );
		if ( string_empty( curveCatmull ) || !ControlPoints_parse( controlPoints, curveCatmull ) ) {
			globalWarningStream() << "Spline Array: misc_spline has no curve_Bezier or curve_CatmullRomSpline data. Add control points in Vertex mode.\n";
			return;
		}
	}
	const std::size_t count = 10;
	const bool alignToTangent = true;
	const Vector3 offset( 0, 0, 0 );

	UndoableCommand undo( "splineArray" );

	scene::Node* parent = &splineInstance->path().parent().get();
	scene::Traversable* traversable = Node_getTraversable( *parent );
	if ( traversable == nullptr ) {
		globalWarningStream() << "Spline Array: Could not find parent traversable.\n";
		return;
	}

	const Matrix4& splineLocalToWorld = splineInstance->localToWorld();

	for ( std::size_t i = 0; i < count; ++i ) {
		const double t = ( count > 1 ) ? ( double( i ) / double( count - 1 ) ) : 0.0;
		Vector3 pos;
		Vector3 tangent;
		if ( useBezier && controlPoints.size() >= 4 ) {
			pos = BezierChain_evaluate( controlPoints, t );
			tangent = BezierChain_derivative( controlPoints, t );
		}
		else if ( controlPoints.size() >= 2 ) {
			pos = CatmullRom_evaluate( controlPoints, t );
			tangent = CatmullRom_derivative( controlPoints, t );
		}
		else
		{
			continue;
		}

		pos = matrix4_transformed_point( splineLocalToWorld, pos + offset );
		tangent = matrix4_transformed_direction( splineLocalToWorld, tangent );

		for ( scene::Node* node : toClone ) {
			scene::Node& clone = Node_Clone( *node );
			Entity* cloneEntity = Node_getEntity( clone );
			if ( cloneEntity != nullptr ) {
				char originBuf[64];
				sprintf( originBuf, "%g %g %g", pos.x(), pos.y(), pos.z() );
				cloneEntity->setKeyValue( "origin", originBuf );
				if ( alignToTangent && vector3_length_squared( tangent ) > 1e-10 ) {
					Vector3 angles = tangentToAngles( tangent );
					char anglesBuf[64];
					sprintf( anglesBuf, "%g %g %g", angles.x(), angles.y(), angles.z() );
					cloneEntity->setKeyValue( "angles", anglesBuf );
					cloneEntity->setKeyValue( "angle", "" );
				}
			}
			Map_gatherNamespaced( NodeSmartReference( clone ) );
			traversable->insert( clone );
		}
	}

	SceneChangeNotify();
}
