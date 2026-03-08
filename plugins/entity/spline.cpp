/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

///\file
///\brief Spline entity (misc_spline) with Bezier and Catmull-Rom curves, control points, and handles.

#include "spline.h"

#include "cullable.h"
#include "renderable.h"
#include "editable.h"

#include "selectionlib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "entitylib.h"
#include "render.h"
#include "eclasslib.h"
#include "stream/stringstream.h"

#include "targetable.h"
#include "origin.h"
#include "angles.h"
#include "filters.h"
#include "namedentity.h"
#include "keyobservers.h"
#include "namekeys.h"

#include "curve.h"
#include "entity.h"

inline void PointVertexArray_testSelect( PointVertex* first, std::size_t count, SelectionTest& test, SelectionIntersection& best ){
	test.TestLineStrip(
	    VertexPointer(
	        reinterpret_cast<VertexPointer::pointer>( &first->vertex ),
	        sizeof( PointVertex )
	    ),
	    IndexPointer::index_type( count ),
	    best
	);
}

class SplineEntity :
	public Bounded,
	public Snappable
{
	EntityKeyValues m_entity;
	KeyObserverMap m_keyObservers;
	MatrixTransform m_transform;

	OriginKey m_originKey;
	Vector3 m_origin;
	AnglesKey m_anglesKey;
	Vector3 m_angles;

	ClassnameFilter m_filter;
	NamedEntity m_named;
	NameKeys m_nameKeys;

	AABB m_aabb_local;
	mutable AABB m_curveBounds;

	RenderableNamedEntity m_renderName;

public:
	BezierChainSpline m_curveBezier;
	SignalHandlerId m_curveBezierChanged;
	CatmullRomSpline m_curveCatmullRom;
	SignalHandlerId m_curveCatmullRomChanged;

private:
	Callback<void()> m_transformChanged;
	Callback<void()> m_evaluateTransform;

	void construct(){
		m_keyObservers.insert( "classname", ClassnameFilter::ClassnameChangedCaller( m_filter ) );
		m_keyObservers.insert( Static<KeyIsName>::instance().m_nameKey, NamedEntity::IdentifierChangedCaller( m_named ) );
		m_keyObservers.insert( "angle", m_anglesKey.getAngleChangedCallback() );
		m_keyObservers.insert( "angles", m_anglesKey.getAnglesChangedCallback() );
		m_keyObservers.insert( "origin", OriginKey::OriginChangedCaller( m_originKey ) );
		m_keyObservers.insert( curve_Bezier, BezierChainSpline::CurveChangedCaller( m_curveBezier ) );
		m_keyObservers.insert( curve_CatmullRomSpline, CatmullRomSpline::CurveChangedCaller( m_curveCatmullRom ) );

		m_nameKeys.setKeyIsName( keyIsNameQuake3 );
		m_entity.attach( m_keyObservers );
	}
	void destroy(){
		m_entity.detach( m_keyObservers );
	}

	void updateTransform(){
		m_transform.localToParent() = g_matrix4_identity;
		matrix4_translate_by_vec3( m_transform.localToParent(), m_origin );
		matrix4_multiply_by_matrix4( m_transform.localToParent(), matrix4_rotation_for_euler_xyz_degrees_quantised( m_angles ) );
		m_transformChanged();
	}

public:
	SplineEntity( EntityClass* eclass, scene::Node& node, const Callback<void()>& transformChanged, const Callback<void()>& boundsChanged, const Callback<void()>& evaluateTransform ) :
		m_entity( eclass ),
		m_originKey( OriginChangedCaller( *this ) ),
		m_origin( ORIGINKEY_IDENTITY ),
		m_anglesKey( AnglesChangedCaller( *this ), m_entity ),
		m_angles( ANGLESKEY_IDENTITY ),
		m_filter( m_entity, node ),
		m_named( m_entity ),
		m_nameKeys( m_entity ),
		m_aabb_local( Vector3( -8, -8, -8 ), Vector3( 8, 8, 8 ) ),
		m_renderName( m_named, g_vector3_identity ),
		m_curveBezier( boundsChanged ),
		m_curveCatmullRom( boundsChanged ),
		m_transformChanged( transformChanged ),
		m_evaluateTransform( evaluateTransform ){
		construct();
	}
	SplineEntity( const SplineEntity& other, scene::Node& node, const Callback<void()>& transformChanged, const Callback<void()>& boundsChanged, const Callback<void()>& evaluateTransform ) :
		m_entity( other.m_entity ),
		m_originKey( OriginChangedCaller( *this ) ),
		m_origin( ORIGINKEY_IDENTITY ),
		m_anglesKey( AnglesChangedCaller( *this ), m_entity ),
		m_angles( ANGLESKEY_IDENTITY ),
		m_filter( m_entity, node ),
		m_named( m_entity ),
		m_nameKeys( m_entity ),
		m_aabb_local( other.m_aabb_local ),
		m_renderName( m_named, g_vector3_identity ),
		m_curveBezier( boundsChanged ),
		m_curveCatmullRom( boundsChanged ),
		m_transformChanged( transformChanged ),
		m_evaluateTransform( evaluateTransform ){
		construct();
	}
	~SplineEntity(){
		destroy();
	}

	typedef MemberCaller<SplineEntity, void(), &SplineEntity::updateTransform> UpdateTransformCaller;
	void originChanged(){
		m_origin = m_originKey.m_origin;
		updateTransform();
	}
	typedef MemberCaller<SplineEntity, void(), &SplineEntity::originChanged> OriginChangedCaller;
	void anglesChanged(){
		m_angles = m_anglesKey.m_angles;
		updateTransform();
	}
	typedef MemberCaller<SplineEntity, void(), &SplineEntity::anglesChanged> AnglesChangedCaller;

	InstanceCounter m_instanceCounter;
	void instanceAttach( const scene::Path& path ){
		if ( ++m_instanceCounter.m_count == 1 ) {
			m_filter.instanceAttach();
			m_entity.instanceAttach( path_find_mapfile( path.begin(), path.end() ) );
		}
	}
	void instanceDetach( const scene::Path& path ){
		if ( --m_instanceCounter.m_count == 0 ) {
			m_entity.instanceDetach( path_find_mapfile( path.begin(), path.end() ) );
			m_filter.instanceDetach();
		}
	}

	EntityKeyValues& getEntity(){
		return m_entity;
	}
	const EntityKeyValues& getEntity() const {
		return m_entity;
	}

	Namespaced& getNamespaced(){
		return m_nameKeys;
	}
	Nameable& getNameable(){
		return m_named;
	}
	TransformNode& getTransformNode(){
		return m_transform;
	}

	const AABB& localAABB() const override {
		m_curveBounds = m_aabb_local;
		aabb_extend_by_aabb_safe( m_curveBounds, m_curveBezier.m_bounds );
		aabb_extend_by_aabb_safe( m_curveBounds, m_curveCatmullRom.m_bounds );
		return m_curveBounds;
	}

	void renderSolid( Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected, bool childSelected, const AABB& childBounds ) const {
		renderer.SetState( m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly );
		renderer.SetState( m_entity.getEntityClass().m_state_wire, Renderer::eFullMaterials );

		if ( !m_curveBezier.m_renderCurve.m_vertices.empty() ) {
			renderer.addRenderable( m_curveBezier.m_renderCurve, localToWorld );
		}
		if ( !m_curveCatmullRom.m_renderCurve.m_vertices.empty() ) {
			renderer.addRenderable( m_curveCatmullRom.m_renderCurve, localToWorld );
		}

		if ( m_renderName.excluded_not()
		  && ( selected || childSelected || ( g_showNames && ( volume.fill() || aabb_fits_view( childBounds, volume.GetModelview(), volume.GetViewport(), g_showNamesRatio ) ) ) ) ) {
			m_renderName.render( renderer, volume, localToWorld, selected, childSelected );
		}
	}

	void renderWireframe( Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected, bool childSelected, const AABB& childBounds ) const {
		renderSolid( renderer, volume, localToWorld, selected, childSelected, childBounds );
	}

	void testSelect( Selector& selector, SelectionTest& test, SelectionIntersection& best ){
		PointVertexArray_testSelect( &m_curveBezier.m_renderCurve.m_vertices[0], m_curveBezier.m_renderCurve.m_vertices.size(), test, best );
		PointVertexArray_testSelect( &m_curveCatmullRom.m_renderCurve.m_vertices[0], m_curveCatmullRom.m_renderCurve.m_vertices.size(), test, best );
	}

	void translate( const Vector3& translation ){
		m_origin = origin_translated( m_origin, translation );
	}
	void rotate( const Quaternion& rotation ){
		m_angles = angles_rotated( m_angles, rotation );
	}
	void snapto( float snap ) override {
		m_originKey.m_origin = origin_snapped( m_originKey.m_origin, snap );
		m_originKey.write( &m_entity );
	}
	void revertTransform(){
		m_origin = m_originKey.m_origin;
		m_angles = m_anglesKey.m_angles;
		m_curveBezier.m_controlPointsTransformed = m_curveBezier.m_controlPoints;
		m_curveCatmullRom.m_controlPointsTransformed = m_curveCatmullRom.m_controlPoints;
	}
	void freezeTransform(){
		m_originKey.m_origin = m_origin;
		m_originKey.write( &m_entity );
		m_anglesKey.m_angles = m_angles;
		m_anglesKey.write( &m_entity );
		m_curveBezier.m_controlPoints = m_curveBezier.m_controlPointsTransformed;
		ControlPoints_write( m_curveBezier.m_controlPoints, curve_Bezier, m_entity );
		m_curveCatmullRom.m_controlPoints = m_curveCatmullRom.m_controlPointsTransformed;
		ControlPoints_write( m_curveCatmullRom.m_controlPoints, curve_CatmullRomSpline, m_entity );
	}
	void transformChanged(){
		revertTransform();
		m_evaluateTransform();
		updateTransform();
		m_curveBezier.curveChanged();
		m_curveCatmullRom.curveChanged();
	}
	typedef MemberCaller<SplineEntity, void(), &SplineEntity::transformChanged> TransformChangedCaller;
};

class SplineEntityInstance :
	public TargetableInstance,
	public TransformModifier,
	public Renderable,
	public SelectionTestable,
	public ComponentSelectionTestable,
	public ComponentEditable,
	public ComponentSnappable
{
	class TypeCasts
	{
		InstanceTypeCastTable m_casts;
	public:
		TypeCasts(){
			m_casts = TargetableInstance::StaticTypeCasts::instance().get();
			InstanceContainedCast<SplineEntityInstance, Bounded>::install( m_casts );
			InstanceStaticCast<SplineEntityInstance, Renderable>::install( m_casts );
			InstanceStaticCast<SplineEntityInstance, SelectionTestable>::install( m_casts );
			InstanceStaticCast<SplineEntityInstance, ComponentSelectionTestable>::install( m_casts );
			InstanceStaticCast<SplineEntityInstance, ComponentEditable>::install( m_casts );
			InstanceStaticCast<SplineEntityInstance, ComponentSnappable>::install( m_casts );
			InstanceStaticCast<SplineEntityInstance, Transformable>::install( m_casts );
			InstanceIdentityCast<SplineEntityInstance>::install( m_casts );
		}
		InstanceTypeCastTable& get(){
			return m_casts;
		}
	};

	SplineEntity& m_contained;
	CurveEdit m_curveBezier;
	CurveEdit m_curveCatmullRom;
	mutable AABB m_aabb_component;

public:
	typedef LazyStatic<TypeCasts> StaticTypeCasts;

	Bounded& get( NullType<Bounded> ){
		return m_contained;
	}

	STRING_CONSTANT( Name, "SplineEntityInstance" );

	SplineEntityInstance( const scene::Path& path, scene::Instance* parent, SplineEntity& contained ) :
		TargetableInstance( path, parent, this, StaticTypeCasts::instance().get(), contained.getEntity(), *this ),
		TransformModifier( SplineEntity::TransformChangedCaller( contained ), ApplyTransformCaller( *this ) ),
		m_contained( contained ),
		m_curveBezier( m_contained.m_curveBezier.m_controlPointsTransformed, SelectionChangedComponentCaller( *this ) ),
		m_curveCatmullRom( m_contained.m_curveCatmullRom.m_controlPointsTransformed, SelectionChangedComponentCaller( *this ) ){
		m_contained.instanceAttach( Instance::path() );
		m_contained.m_curveBezierChanged = m_contained.m_curveBezier.connect( CurveEdit::CurveChangedCaller( m_curveBezier ) );
		m_contained.m_curveCatmullRomChanged = m_contained.m_curveCatmullRom.connect( CurveEdit::CurveChangedCaller( m_curveCatmullRom ) );
	}
	~SplineEntityInstance(){
		m_contained.m_curveCatmullRom.disconnect( m_contained.m_curveCatmullRomChanged );
		m_contained.m_curveBezier.disconnect( m_contained.m_curveBezierChanged );
		m_contained.instanceDetach( Instance::path() );
	}

	void renderSolid( Renderer& renderer, const VolumeTest& volume ) const override {
		m_contained.renderSolid( renderer, volume, Instance::localToWorld(), getSelectable().isSelected(), Instance::childSelected(), Instance::childBounds() );

		m_curveBezier.renderComponentsSelected( renderer, volume, localToWorld() );
		m_curveCatmullRom.renderComponentsSelected( renderer, volume, localToWorld() );
	}
	void renderWireframe( Renderer& renderer, const VolumeTest& volume ) const override {
		m_contained.renderWireframe( renderer, volume, Instance::localToWorld(), getSelectable().isSelected(), Instance::childSelected(), Instance::childBounds() );

		m_curveBezier.renderComponentsSelected( renderer, volume, localToWorld() );
		m_curveCatmullRom.renderComponentsSelected( renderer, volume, localToWorld() );
	}
	void renderComponents( Renderer& renderer, const VolumeTest& volume ) const override {
		if ( GlobalSelectionSystem().ComponentMode() == SelectionSystem::eVertex ) {
			m_curveBezier.renderComponents( renderer, volume, localToWorld() );
			m_curveCatmullRom.renderComponents( renderer, volume, localToWorld() );
		}
	}

	void testSelect( Selector& selector, SelectionTest& test ) override {
		test.BeginMesh( localToWorld() );
		SelectionIntersection best;

		m_contained.testSelect( selector, test, best );

		if ( best.valid() ) {
			Selector_add( selector, getSelectable(), best );
		}
	}

	bool isSelectedComponents() const override {
		return m_curveBezier.isSelected() || m_curveCatmullRom.isSelected();
	}
	void setSelectedComponents( bool selected, SelectionSystem::EComponentMode mode ) override {
		if ( mode == SelectionSystem::eVertex ) {
			m_curveBezier.setSelected( selected );
			m_curveCatmullRom.setSelected( selected );
		}
	}
	void testSelectComponents( Selector& selector, SelectionTest& test, SelectionSystem::EComponentMode mode ) override {
		if ( mode == SelectionSystem::eVertex ) {
			test.BeginMesh( localToWorld() );
			m_curveBezier.testSelect( selector, test );
			m_curveCatmullRom.testSelect( selector, test );
		}
	}
	void gatherComponentsHighlight( std::vector<std::vector<Vector3>>& polygons, SelectionIntersection& intersection, SelectionTest& test, SelectionSystem::EComponentMode mode ) const override {
	}

	void transformComponents( const Matrix4& matrix ){
		if ( m_curveBezier.isSelected() ) {
			m_curveBezier.transform( matrix );
		}
		if ( m_curveCatmullRom.isSelected() ) {
			m_curveCatmullRom.transform( matrix );
		}
	}

	const AABB& getSelectedComponentsBounds() const override {
		m_aabb_component = AABB();
		m_curveBezier.forEachSelected( [&]( const Vector3& point ){
			aabb_extend_by_point_safe( m_aabb_component, point );
		});
		m_curveCatmullRom.forEachSelected( [&]( const Vector3& point ){
			aabb_extend_by_point_safe( m_aabb_component, point );
		});
		return m_aabb_component;
	}
	void gatherSelectedComponents( const Vector3Callback& callback ) const override {
	}

	void snapComponents( float snap ) override {
		if ( m_curveBezier.isSelected() ) {
			m_curveBezier.snapto( snap );
			m_curveBezier.write( curve_Bezier, m_contained.getEntity() );
		}
		if ( m_curveCatmullRom.isSelected() ) {
			m_curveCatmullRom.snapto( snap );
			m_curveCatmullRom.write( curve_CatmullRomSpline, m_contained.getEntity() );
		}
	}

	void evaluateTransform(){
		if ( getType() == TRANSFORM_PRIMITIVE ) {
			m_contained.translate( getTranslation() );
			m_contained.rotate( getRotation() );
		}
		else
		{
			transformComponents( calculateTransform() );
		}
	}
	void applyTransform(){
		m_contained.revertTransform();
		evaluateTransform();
		m_contained.freezeTransform();
	}
	typedef MemberCaller<SplineEntityInstance, void(), &SplineEntityInstance::applyTransform> ApplyTransformCaller;

	void selectionChangedComponent( const Selectable& selectable ){
		GlobalSelectionSystem().getObserver( SelectionSystem::eComponent )( selectable );
		GlobalSelectionSystem().onComponentSelection( *this, selectable );
	}
	typedef MemberCaller<SplineEntityInstance, void(const Selectable&), &SplineEntityInstance::selectionChangedComponent> SelectionChangedComponentCaller;
};

class SplineEntityNode final :
	public scene::Node::Symbiot,
	public scene::Instantiable,
	public scene::Cloneable
{
	class TypeCasts
	{
		NodeTypeCastTable m_casts;
	public:
		TypeCasts(){
			NodeStaticCast<SplineEntityNode, scene::Instantiable>::install( m_casts );
			NodeStaticCast<SplineEntityNode, scene::Cloneable>::install( m_casts );
			NodeContainedCast<SplineEntityNode, Snappable>::install( m_casts );
			NodeContainedCast<SplineEntityNode, TransformNode>::install( m_casts );
			NodeContainedCast<SplineEntityNode, Entity>::install( m_casts );
			NodeContainedCast<SplineEntityNode, Nameable>::install( m_casts );
			NodeContainedCast<SplineEntityNode, Namespaced>::install( m_casts );
		}
		NodeTypeCastTable& get(){
			return m_casts;
		}
	};

	scene::Node m_node;
	InstanceSet m_instances;
	SplineEntity m_contained;

public:
	typedef LazyStatic<TypeCasts> StaticTypeCasts;

	Snappable& get( NullType<Snappable> ){
		return m_contained;
	}
	TransformNode& get( NullType<TransformNode> ){
		return m_contained.getTransformNode();
	}
	Entity& get( NullType<Entity> ){
		return m_contained.getEntity();
	}
	Nameable& get( NullType<Nameable> ){
		return m_contained.getNameable();
	}
	Namespaced& get( NullType<Namespaced> ){
		return m_contained.getNamespaced();
	}

	SplineEntityNode( EntityClass* eclass ) :
		m_node( this, this, StaticTypeCasts::instance().get(), GlobalSceneGraph().currentLayer() ),
		m_contained( eclass, m_node, InstanceSet::TransformChangedCaller( m_instances ), InstanceSet::BoundsChangedCaller( m_instances ), InstanceSetEvaluateTransform<SplineEntityInstance>::Caller( m_instances ) ){
	}
	SplineEntityNode( const SplineEntityNode& other ) :
		scene::Node::Symbiot( other ),
		scene::Instantiable( other ),
		scene::Cloneable( other ),
		m_node( this, this, StaticTypeCasts::instance().get(), other.m_node.m_layer ),
		m_contained( other.m_contained, m_node, InstanceSet::TransformChangedCaller( m_instances ), InstanceSet::BoundsChangedCaller( m_instances ), InstanceSetEvaluateTransform<SplineEntityInstance>::Caller( m_instances ) ){
	}
	void release() override {
		delete this;
	}
	scene::Node& node(){
		return m_node;
	}

	scene::Node& clone() const override {
		return ( new SplineEntityNode( *this ) )->node();
	}

	scene::Instance* create( const scene::Path& path, scene::Instance* parent ) override {
		return new SplineEntityInstance( path, parent, m_contained );
	}
	void forEachInstance( const scene::Instantiable::Visitor& visitor ) override {
		m_instances.forEachInstance( visitor );
	}
	void insert( scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance ) override {
		m_instances.insert( observer, path, instance );
	}
	scene::Instance* erase( scene::Instantiable::Observer* observer, const scene::Path& path ) override {
		return m_instances.erase( observer, path );
	}
};

void SplineEntity_construct(){
	CurveEdit::Type::instance().m_controlsShader = GlobalShaderCache().capture( "$POINT" );
	CurveEdit::Type::instance().m_selectedShader = GlobalShaderCache().capture( "$SELPOINT" );
}

void SplineEntity_destroy(){
	GlobalShaderCache().release( "$SELPOINT" );
	GlobalShaderCache().release( "$POINT" );
}

scene::Node& New_SplineEntity( EntityClass* eclass ){
	return ( new SplineEntityNode( eclass ) )->node();
}
