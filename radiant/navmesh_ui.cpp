#include "navmesh_ui.h"

#include "navmeshsystem.h"
#include "camwindow.h"
#include "entitylib.h"
#include "ientity.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "iundo.h"
#include "mainframe.h"
#include "renderer.h"
#include "igl.h"
#include "math/matrix.h"
#include "debugging/debugging.h"
#include "DetourNavMesh.h"
#include "scenelib.h"

#include <array>
#include <limits>
#include <vector>

namespace
{
CamWnd* getCamWnd(){
	return GlobalCamera_getCamWnd();
}

void refreshCamera(){
	if ( CamWnd* camwnd = getCamWnd() ) {
		CamWnd_Update( *camwnd );
	}
}

	class NavMeshRenderable final : public OpenGLRenderable
	{
	public:
		using Segment = std::array<float, 3>;

		NavMeshRenderable( const dtNavMesh* mesh, const std::array<float, 4>& colour, float lineWidth ) :
			m_colour( colour ),
			m_lineWidth( lineWidth )
		{
			if ( mesh == 0 ) {
				return;
			}
			for ( int tileIndex = 0; tileIndex < mesh->getMaxTiles(); ++tileIndex )
			{
				const dtMeshTile* tile = mesh->getTile( tileIndex );
				if ( tile == 0 || tile->header == 0 ) {
					continue;
				}
				const float* verts = tile->verts;
				const dtPoly* polys = tile->polys;
				for ( int polyIndex = 0; polyIndex < tile->header->polyCount; ++polyIndex )
				{
					const dtPoly* poly = &polys[polyIndex];
					if ( poly->vertCount < 3 ) {
						continue;
					}
					for ( int vert = 0; vert < poly->vertCount; ++vert )
					{
						const int idx0 = poly->verts[vert];
						const int idx1 = poly->verts[( vert + 1 ) % poly->vertCount];
						const float* v0 = &verts[idx0 * 3];
						const float* v1 = &verts[idx1 * 3];
						m_segments.push_back( Segment{ { v0[0], v0[1], v0[2] } } );
						m_segments.push_back( Segment{ { v1[0], v1[1], v1[2] } } );
					}
				}
			}
		}

		bool empty() const {
			return m_segments.empty();
		}

	void render( RenderStateFlags ) const override {
			if ( m_segments.empty() ) {
				return;
			}
			gl().glEnable( GL_BLEND );
			gl().glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			gl().glLineWidth( m_lineWidth );
			gl().glColor4fv( m_colour.data() );
			gl().glBegin( GL_LINES );
			for ( const Segment& segment : m_segments )
			{
				gl().glVertex3fv( segment.data() );
			}
			gl().glEnd();
			gl().glLineWidth( 1.0f );
			gl().glColor4f( 1, 1, 1, 1 );
		}

	private:
		std::vector<Segment> m_segments;
		std::array<float, 4> m_colour;
		float m_lineWidth;
	};

	struct PhysicsPlacementPreview
	{
		AABB currentBounds;
		AABB settledBounds;
		Vector3 currentOrigin;
		Vector3 settledOrigin;
		bool hasLanding = false;
		scene::Instance* instance = nullptr;
	};

	bool PhysicsPlacement_isDynamicEntity( const Entity* entity ){
		if ( entity == nullptr ) {
			return false;
		}

		const char* classname = entity->getClassName();
		return string_equal_prefix_nocase( classname, "misc_phys_" )
			&& !classname_equal( classname, "misc_phys_sensor" );
	}

	bool PhysicsPlacement_isSupportEntity( const Entity* entity ){
		if ( entity == nullptr ) {
			return true;
		}

		return !classname_equal( entity->getClassName(), "misc_phys_sensor" );
	}

	bool PhysicsPlacement_hasUsableBounds( const AABB& bounds ){
		return aabb_valid( bounds )
			&& bounds.extents.x() > 0
			&& bounds.extents.y() > 0
			&& bounds.extents.z() >= 0;
	}

	double PhysicsPlacement_minZ( const AABB& bounds ){
		return bounds.origin.z() - bounds.extents.z();
	}

	double PhysicsPlacement_maxZ( const AABB& bounds ){
		return bounds.origin.z() + bounds.extents.z();
	}

	bool PhysicsPlacement_overlapsXY( const AABB& lhs, const AABB& rhs ){
		return fabs( lhs.origin.x() - rhs.origin.x() ) <= ( lhs.extents.x() + rhs.extents.x() )
			&& fabs( lhs.origin.y() - rhs.origin.y() ) <= ( lhs.extents.y() + rhs.extents.y() );
	}

	std::vector<AABB> PhysicsPlacement_collectSupportBounds(){
		std::vector<AABB> supports;

		class SupportWalker final : public scene::Graph::Walker
		{
			std::vector<AABB>& m_supports;
		public:
			explicit SupportWalker( std::vector<AABB>& supports ) : m_supports( supports ){
			}

			bool pre( const scene::Path& path, scene::Instance& instance ) const override {
				if ( Selectable* selectable = Instance_getSelectable( instance ); selectable != nullptr && selectable->isSelected() ) {
					return true;
				}

				if ( Entity* entity = Node_getEntity( path.top() ) ) {
					if ( !PhysicsPlacement_isSupportEntity( entity ) ) {
						return true;
					}
				}

				const AABB bounds = instance.worldAABB();
				if ( PhysicsPlacement_hasUsableBounds( bounds ) ) {
					m_supports.push_back( bounds );
				}
				return true;
			}
		};

		GlobalSceneGraph().traverse( SupportWalker( supports ) );
		return supports;
	}

	std::vector<PhysicsPlacementPreview> PhysicsPlacement_collectSelectionPreview(){
		const std::vector<AABB> supports = PhysicsPlacement_collectSupportBounds();
		std::vector<PhysicsPlacementPreview> previews;

		class PreviewVisitor final : public SelectionSystem::Visitor
		{
			const std::vector<AABB>& m_supports;
			std::vector<PhysicsPlacementPreview>& m_previews;
		public:
			PreviewVisitor( const std::vector<AABB>& supports, std::vector<PhysicsPlacementPreview>& previews )
				: m_supports( supports ), m_previews( previews ){
			}

			void visit( scene::Instance& instance ) const override {
				Entity* entity = Node_getEntity( instance.path().top() );
				if ( !PhysicsPlacement_isDynamicEntity( entity ) ) {
					return;
				}

				const AABB currentBounds = instance.worldAABB();
				if ( !PhysicsPlacement_hasUsableBounds( currentBounds ) ) {
					return;
				}

				const double currentMinZ = PhysicsPlacement_minZ( currentBounds );
				double bestSupportTop = -std::numeric_limits<double>::infinity();
				bool foundSupport = false;
				for ( const AABB& support : m_supports )
				{
					if ( !PhysicsPlacement_overlapsXY( currentBounds, support ) ) {
						continue;
					}

					const double supportTop = PhysicsPlacement_maxZ( support );
					if ( supportTop > currentMinZ + 1.0 ) {
						continue;
					}

					if ( !foundSupport || supportTop > bestSupportTop ) {
						bestSupportTop = supportTop;
						foundSupport = true;
					}
				}

				PhysicsPlacementPreview preview;
				preview.currentBounds = currentBounds;
				preview.settledBounds = currentBounds;
				preview.currentOrigin = currentBounds.origin;
				preview.settledOrigin = currentBounds.origin;
				preview.hasLanding = foundSupport;
				preview.instance = &instance;
				if ( foundSupport ) {
					const double deltaZ = bestSupportTop - currentMinZ;
					preview.settledOrigin[2] += deltaZ;
					preview.settledBounds.origin[2] += deltaZ;
				}
				m_previews.push_back( preview );
			}
		};

		GlobalSelectionSystem().foreachSelected( PreviewVisitor( supports, previews ) );
		return previews;
	}

	class PhysicsPlacementRenderable final : public OpenGLRenderable
	{
		std::vector<PhysicsPlacementPreview> m_previews;
	public:
		explicit PhysicsPlacementRenderable( std::vector<PhysicsPlacementPreview> previews )
			: m_previews( std::move( previews ) ){
		}

		bool empty() const {
			return m_previews.empty();
		}

		void render( RenderStateFlags ) const override {
			if ( m_previews.empty() ) {
				return;
			}

			gl().glEnable( GL_BLEND );
			gl().glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			gl().glLineWidth( 1.7f );

			for ( const PhysicsPlacementPreview& preview : m_previews )
			{
				gl().glColor4f( 0.98f, 0.66f, 0.18f, 0.75f );
				aabb_draw_wire( preview.currentBounds );

				if ( preview.hasLanding ) {
					gl().glColor4f( 0.18f, 0.9f, 0.5f, 0.78f );
					aabb_draw_wire( preview.settledBounds );

					const Vector3 currentBottom( preview.currentOrigin.x(), preview.currentOrigin.y(), PhysicsPlacement_minZ( preview.currentBounds ) );
					const Vector3 settledBottom( preview.settledOrigin.x(), preview.settledOrigin.y(), PhysicsPlacement_minZ( preview.settledBounds ) );
					gl().glBegin( GL_LINES );
					gl().glVertex3f( currentBottom.x(), currentBottom.y(), currentBottom.z() );
					gl().glVertex3f( settledBottom.x(), settledBottom.y(), settledBottom.z() );
					gl().glEnd();
				}
			}

			gl().glLineWidth( 1.0f );
			gl().glColor4f( 1, 1, 1, 1 );
		}
	};
}

bool g_navmeshOverlayEnabled = false;
bool g_physicsPlacementOverlayEnabled = false;

bool NavMeshOverlay_isEnabled(){
	return g_navmeshOverlayEnabled;
}

void NavMeshOverlay_setEnabled( bool enabled ){
	if ( g_navmeshOverlayEnabled == enabled ) {
		return;
	}
	g_navmeshOverlayEnabled = enabled;
	if ( enabled && GlobalNavMeshSystem().navMesh() == 0 ) {
		GlobalNavMeshSystem().rebuild();
	}
	refreshCamera();
}

void NavMeshOverlay_toggle(){
	NavMeshOverlay_setEnabled( !g_navmeshOverlayEnabled );
}

void NavMeshOverlay_render( Renderer& renderer ){
	if ( !g_navmeshOverlayEnabled ) {
		return;
	}
	const dtNavMesh* mesh = GlobalNavMeshSystem().navMesh();
	if ( mesh == 0 ) {
		return;
	}
	const std::array<float, 4> colour{ 0.0f, 0.78f, 0.92f, 0.55f };
	NavMeshRenderable renderable( mesh, colour, 1.4f );
	if ( renderable.empty() ) {
		return;
	}
	renderer.PushState();
	renderer.addRenderable( renderable, g_matrix4_identity );
	renderer.PopState();
}

void NavMesh_rebuild(){
	if ( GlobalNavMeshSystem().rebuild() ) {
		globalOutputStream() << "Navmesh rebuilt in " << GlobalNavMeshSystem().buildTimeMs() << " ms\n";
	}
	else
	{
		globalWarningStream() << "Navmesh rebuild failed or produced no valid surface.\n";
	}
	refreshCamera();
}

bool PhysicsPlacementOverlay_isEnabled(){
	return g_physicsPlacementOverlayEnabled;
}

void PhysicsPlacementOverlay_setEnabled( bool enabled ){
	if ( g_physicsPlacementOverlayEnabled == enabled ) {
		return;
	}
	g_physicsPlacementOverlayEnabled = enabled;
	refreshCamera();
}

void PhysicsPlacementOverlay_toggle(){
	PhysicsPlacementOverlay_setEnabled( !g_physicsPlacementOverlayEnabled );
}

void PhysicsPlacementOverlay_render( Renderer& renderer ){
	if ( !g_physicsPlacementOverlayEnabled ) {
		return;
	}

	PhysicsPlacementRenderable renderable( PhysicsPlacement_collectSelectionPreview() );
	if ( renderable.empty() ) {
		return;
	}

	renderer.PushState();
	renderer.addRenderable( renderable, g_matrix4_identity );
	renderer.PopState();
}

void PhysicsPlacement_settleSelection(){
	std::vector<PhysicsPlacementPreview> previews = PhysicsPlacement_collectSelectionPreview();
	if ( previews.empty() ) {
		Sys_Status( "Select rigid-body physics props to settle." );
		return;
	}

	int moved = 0;
	UndoableCommand undo( "physicsPlacementSettleSelection" );
	for ( const PhysicsPlacementPreview& preview : previews )
	{
		if ( !preview.hasLanding || preview.instance == nullptr ) {
			continue;
		}

		if ( Transformable* transform = Instance_getTransformable( *preview.instance ) ) {
			transform->setType( TRANSFORM_PRIMITIVE );
			transform->setTranslation( preview.settledOrigin );
			transform->freezeTransform();
			++moved;
		}
	}

	if ( moved == 0 ) {
		Sys_Status( "No supporting surface found below the selected rigid bodies." );
		return;
	}

	SceneChangeNotify();
	refreshCamera();
	globalOutputStream() << "Settled " << moved << " rigid-body prop"
		<< ( moved == 1 ? "" : "s" ) << " for placement preview.\n";
	Sys_Status( moved == 1 ? "Settled 1 rigid-body prop." : "Settled rigid-body props." );
}
