#pragma once

#include "renderable.h"

class Renderer;

extern bool g_navmeshOverlayEnabled;
extern bool g_physicsPlacementOverlayEnabled;

bool NavMeshOverlay_isEnabled();
void NavMeshOverlay_toggle();
void NavMeshOverlay_setEnabled( bool enabled );
void NavMeshOverlay_render( Renderer& renderer );

void NavMesh_rebuild();

bool PhysicsPlacementOverlay_isEnabled();
void PhysicsPlacementOverlay_toggle();
void PhysicsPlacementOverlay_setEnabled( bool enabled );
void PhysicsPlacementOverlay_render( Renderer& renderer );
void PhysicsPlacement_settleSelection();
