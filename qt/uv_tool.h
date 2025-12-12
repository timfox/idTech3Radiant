#pragma once

#include <QVector3D>
#include <QVector2D>
#include <QPoint>
#include <QVector>

// UV Tool for editing texture alignment of selected faces
class UVTool {
public:
	UVTool();
	
	// Face selection
	void selectFace(int brushIndex, int faceIndex);
	void clearSelection();
	bool hasSelection() const { return m_selectedBrushIndex >= 0 && m_selectedFaceIndex >= 0; }
	
	// UV manipulation
	void translateUV(const QVector2D& delta);
	void rotateUV(float angle);
	void scaleUV(const QVector2D& scale);
	void flipUV(bool horizontal, bool vertical);
	
	// Texture alignment
	void alignToView();
	void alignToFace();
	void fitToFace();
	
	// Grid alignment
	void snapToGrid(float gridSize);
	
	// Get current UV coordinates
	QVector<QVector2D> getUVCoordinates() const;
	void setUVCoordinates(const QVector<QVector2D>& uvs);
	
	// Texture lock
	void setTextureLock(bool enabled) { m_textureLock = enabled; }
	bool textureLock() const { return m_textureLock; }
	
	// Undo support
	void saveState();
	void restoreState();
	bool canUndo() const { return !m_undoStack.isEmpty(); }

private:
	int m_selectedBrushIndex{-1};
	int m_selectedFaceIndex{-1};
	QVector<QVector2D> m_uvCoordinates;
	bool m_textureLock{false};
	
	// Undo stack
	struct UndoState {
		QVector<QVector2D> uvs;
	};
	QVector<UndoState> m_undoStack;
	
	// In full implementation, would store face geometry and texture info
};
