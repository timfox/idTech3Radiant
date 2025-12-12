#include "uv_tool.h"

#include <QVector>

#include <cmath>

UVTool::UVTool() {
}

void UVTool::selectFace(int brushIndex, int faceIndex) {
	m_selectedBrushIndex = brushIndex;
	m_selectedFaceIndex = faceIndex;
	
	// In full implementation, would load UV coordinates from face
	// For now, initialize with default coordinates
	m_uvCoordinates.clear();
	m_uvCoordinates.append(QVector2D(0.0f, 0.0f));
	m_uvCoordinates.append(QVector2D(1.0f, 0.0f));
	m_uvCoordinates.append(QVector2D(1.0f, 1.0f));
	m_uvCoordinates.append(QVector2D(0.0f, 1.0f));
}

void UVTool::clearSelection() {
	m_selectedBrushIndex = -1;
	m_selectedFaceIndex = -1;
	m_uvCoordinates.clear();
}

void UVTool::translateUV(const QVector2D& delta) {
	if (!hasSelection()) return;
	
	for (QVector2D& uv : m_uvCoordinates) {
		uv += delta;
	}
}

void UVTool::rotateUV(float angle) {
	if (!hasSelection()) return;
	
	// Rotate around center
	QVector2D center(0.0f, 0.0f);
	for (const QVector2D& uv : m_uvCoordinates) {
		center += uv;
	}
	center /= static_cast<float>(m_uvCoordinates.size());
	
	const float cosA = std::cos(angle);
	const float sinA = std::sin(angle);
	
	for (QVector2D& uv : m_uvCoordinates) {
		QVector2D relative = uv - center;
		uv = center + QVector2D(
			relative.x() * cosA - relative.y() * sinA,
			relative.x() * sinA + relative.y() * cosA
		);
	}
}

void UVTool::scaleUV(const QVector2D& scale) {
	if (!hasSelection()) return;
	
	// Scale around center
	QVector2D center(0.0f, 0.0f);
	for (const QVector2D& uv : m_uvCoordinates) {
		center += uv;
	}
	center /= static_cast<float>(m_uvCoordinates.size());
	
	for (QVector2D& uv : m_uvCoordinates) {
		QVector2D relative = uv - center;
		uv = center + QVector2D(relative.x() * scale.x(), relative.y() * scale.y());
	}
}

void UVTool::flipUV(bool horizontal, bool vertical) {
	if (!hasSelection()) return;
	
	QVector2D center(0.0f, 0.0f);
	for (const QVector2D& uv : m_uvCoordinates) {
		center += uv;
	}
	center /= static_cast<float>(m_uvCoordinates.size());
	
	for (QVector2D& uv : m_uvCoordinates) {
		QVector2D relative = uv - center;
		if (horizontal) relative.setX(-relative.x());
		if (vertical) relative.setY(-relative.y());
		uv = center + relative;
	}
}

void UVTool::alignToView() {
	// In full implementation, would align UVs to current view direction
	// For now, just reset to default
	selectFace(m_selectedBrushIndex, m_selectedFaceIndex);
}

void UVTool::alignToFace() {
	// In full implementation, would align UVs to face normal
	// For now, just reset to default
	selectFace(m_selectedBrushIndex, m_selectedFaceIndex);
}

void UVTool::fitToFace() {
	// In full implementation, would fit UVs to cover entire face
	// For now, scale to unit square
	scaleUV(QVector2D(1.0f, 1.0f));
}

void UVTool::snapToGrid(float gridSize) {
	if (!hasSelection() || gridSize <= 0.0f) return;
	
	for (QVector2D& uv : m_uvCoordinates) {
		uv.setX(std::round(uv.x() / gridSize) * gridSize);
		uv.setY(std::round(uv.y() / gridSize) * gridSize);
	}
}

QVector<QVector2D> UVTool::getUVCoordinates() const {
	return m_uvCoordinates;
}

void UVTool::setUVCoordinates(const QVector<QVector2D>& uvs) {
	if (!hasSelection()) return;
	m_uvCoordinates = uvs;
}

void UVTool::saveState() {
	UndoState state;
	state.uvs = m_uvCoordinates;
	m_undoStack.append(state);
	
	// Limit undo stack size
	if (m_undoStack.size() > 50) {
		m_undoStack.removeFirst();
	}
}

void UVTool::restoreState() {
	if (m_undoStack.isEmpty()) return;
	
	UndoState state = m_undoStack.takeLast();
	m_uvCoordinates = state.uvs;
}
