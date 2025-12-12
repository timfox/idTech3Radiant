#include "vertex_tool.h"

#include <QVector>
#include <QMap>

#include <QPainter>
#include <QColor>
#include <cmath>
#include <algorithm>

VertexTool::VertexTool() {
}

void VertexTool::selectVertex(int brushIndex, int vertexIndex, const QVector3D& position) {
	m_selectedVertices.clear();
	SelectedVertex v;
	v.brushIndex = brushIndex;
	v.vertexIndex = vertexIndex;
	v.position = position;
	v.originalPosition = position;
	m_selectedVertices.append(v);
}

void VertexTool::addVertexToSelection(int brushIndex, int vertexIndex, const QVector3D& position) {
	// Check if already selected
	if (findVertex(brushIndex, vertexIndex)) {
		return; // Already selected
	}
	
	SelectedVertex v;
	v.brushIndex = brushIndex;
	v.vertexIndex = vertexIndex;
	v.position = position;
	v.originalPosition = position;
	m_selectedVertices.append(v);
}

void VertexTool::clearSelection() {
	m_selectedVertices.clear();
}

VertexTool::SelectedVertex* VertexTool::findVertex(int brushIndex, int vertexIndex) {
	for (SelectedVertex& v : m_selectedVertices) {
		if (v.brushIndex == brushIndex && v.vertexIndex == vertexIndex) {
			return &v;
		}
	}
	return nullptr;
}

void VertexTool::moveVertex(const QVector3D& delta) {
	if (!hasSelection()) return;
	
	// Move all selected vertices by delta
	for (SelectedVertex& v : m_selectedVertices) {
		v.position += delta;
	}
	
	// In full implementation, would update brush geometry
	// and trigger realtime update if enabled
}

void VertexTool::moveVertexTo(int vertexIndex, const QVector3D& newPosition) {
	if (vertexIndex < 0 || vertexIndex >= m_selectedVertices.size()) return;
	
	m_selectedVertices[vertexIndex].position = newPosition;
	
	// In full implementation, would update brush geometry
	// and trigger realtime update if enabled
}

void VertexTool::snapToGrid(float gridSize) {
	if (!hasSelection() || gridSize <= 0.0f) return;
	
	// Snap all selected vertices to grid
	for (SelectedVertex& v : m_selectedVertices) {
		QVector3D snapped = v.position;
		snapped.setX(std::round(snapped.x() / gridSize) * gridSize);
		snapped.setY(std::round(snapped.y() / gridSize) * gridSize);
		snapped.setZ(std::round(snapped.z() / gridSize) * gridSize);
		v.position = snapped;
	}
}

void VertexTool::saveState() {
	UndoState state;
	state.vertices = m_selectedVertices;
	m_undoStack.append(state);
	
	// Limit undo stack size
	if (m_undoStack.size() > 50) {
		m_undoStack.removeFirst();
	}
}

void VertexTool::restoreState() {
	if (m_undoStack.isEmpty()) return;
	
	UndoState state = m_undoStack.takeLast();
	m_selectedVertices = state.vertices;
	
	// In full implementation, would restore brush geometry
}

bool VertexTool::isShapeValid() const {
	if (m_selectedVertices.isEmpty()) return true;
	
	// Check all brushes with selected vertices
	QVector<int> brushIndices;
	for (const SelectedVertex& v : m_selectedVertices) {
		if (!brushIndices.contains(v.brushIndex)) {
			brushIndices.append(v.brushIndex);
		}
	}
	
	for (int brushIndex : brushIndices) {
		if (!validateBrush(brushIndex)) {
			return false;
		}
	}
	
	return true;
}

QVector<int> VertexTool::getInvalidFaces() const {
	QVector<int> invalid;
	
	if (m_selectedVertices.isEmpty()) return invalid;
	
	// In full implementation, would check each face for validity
	// For now, return empty (all valid)
	return invalid;
}

bool VertexTool::isVertexValid(int brushIndex, int vertexIndex) const {
	Q_UNUSED(brushIndex);
	Q_UNUSED(vertexIndex);
	
	// In full implementation, would check if moving this vertex
	// would create an invalid brush shape
	return true;
}

bool VertexTool::validateBrush(int brushIndex) const {
	Q_UNUSED(brushIndex);
	// In full implementation, would validate:
	// - All faces are planar
	// - Brush is convex
	// - No degenerate faces
	// - All vertices are valid
	
	// For now, check if any selected vertices belong to this brush
	for (const SelectedVertex& v : m_selectedVertices) {
		if (v.brushIndex == brushIndex) {
			// Simplified: assume valid if vertex positions are reasonable
			if (v.position.x() < -100000.0f || v.position.x() > 100000.0f ||
			    v.position.y() < -100000.0f || v.position.y() > 100000.0f ||
			    v.position.z() < -100000.0f || v.position.z() > 100000.0f) {
				return false; // Out of bounds
			}
		}
	}
	
	return true;
}

void VertexTool::removeSelectedVertices() {
	if (m_selectedVertices.isEmpty()) return;
	
	// Group vertices by brush
	QMap<int, QVector<int>> brushVertices;
	for (int i = 0; i < m_selectedVertices.size(); ++i) {
		int brushIdx = m_selectedVertices[i].brushIndex;
		brushVertices[brushIdx].append(i);
	}
	
	// Remove vertices from each brush
	// In full implementation, would:
	// 1. Remove vertex from brush geometry
	// 2. Update face indices
	// 3. Validate resulting brush
	// 4. Remove brush if it becomes invalid
	
	m_selectedVertices.clear();
}

void VertexTool::insertVertex(int brushIndex, int edgeIndex, const QVector3D& position) {
	Q_UNUSED(brushIndex);
	Q_UNUSED(edgeIndex);
	Q_UNUSED(position);
	
	// In full implementation, would:
	// 1. Find edge by index
	// 2. Insert vertex at position along edge
	// 3. Split faces that use this edge
	// 4. Update brush geometry
	// 5. Validate resulting brush
}

bool VertexTool::canRemoveVertex(int brushIndex, int vertexIndex) const {
	Q_UNUSED(brushIndex);
	Q_UNUSED(vertexIndex);
	
	// In full implementation, would check:
	// - Vertex is not required for brush validity
	// - Removing it won't create degenerate faces
	// - Brush will remain convex
	
	return true; // Simplified
}

bool VertexTool::canInsertVertex(int brushIndex, int edgeIndex) const {
	Q_UNUSED(brushIndex);
	Q_UNUSED(edgeIndex);
	
	// In full implementation, would check:
	// - Edge exists
	// - Position is valid
	// - Resulting brush will be valid
	
	return true; // Simplified
}
