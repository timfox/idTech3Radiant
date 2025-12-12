#pragma once

#include <QVector3D>
#include <QVector>
#include <QPoint>

class VertexTool {
public:
	VertexTool();
	
	// Vertex selection
	void selectVertex(int brushIndex, int vertexIndex, const QVector3D& position);
	void addVertexToSelection(int brushIndex, int vertexIndex, const QVector3D& position);
	void clearSelection();
	bool hasSelection() const { return !m_selectedVertices.isEmpty(); }
	QVector3D selectedPosition() const { return m_selectedVertices.isEmpty() ? QVector3D() : m_selectedVertices[0].position; }
	
	// Multi-vertex selection
	struct SelectedVertex {
		int brushIndex;
		int vertexIndex;
		QVector3D position;
		QVector3D originalPosition;
	};
	QVector<SelectedVertex> selectedVertices() const { return m_selectedVertices; }
	int selectedVertexCount() const { return m_selectedVertices.size(); }
	
	// Vertex manipulation
	void moveVertex(const QVector3D& delta); // Move all selected vertices by delta
	void moveVertexTo(int vertexIndex, const QVector3D& newPosition);
	void snapToGrid(float gridSize);
	
	// Vertex editing - remove and insert
	void removeSelectedVertices();
	void insertVertex(int brushIndex, int edgeIndex, const QVector3D& position); // Insert vertex on edge
	bool canRemoveVertex(int brushIndex, int vertexIndex) const;
	bool canInsertVertex(int brushIndex, int edgeIndex) const;
	
	// Validation
	bool isShapeValid() const;
	QVector<int> getInvalidFaces() const; // Returns indices of invalid faces
	bool isVertexValid(int brushIndex, int vertexIndex) const;
	
	// Realtime updates
	void enableRealtime(bool enabled) { m_realtimeUpdates = enabled; }
	bool realtimeUpdates() const { return m_realtimeUpdates; }
	
	// Undo support
	void saveState();
	void restoreState();
	bool canUndo() const { return !m_undoStack.isEmpty(); }

private:
	QVector<SelectedVertex> m_selectedVertices;
	bool m_realtimeUpdates{true};
	
	// Undo stack
	struct UndoState {
		QVector<SelectedVertex> vertices;
	};
	QVector<UndoState> m_undoStack;
	
	// In full implementation, would store brush geometry
	QVector<QVector<QVector3D>> m_brushVertices;
	
	bool validateBrush(int brushIndex) const;
	SelectedVertex* findVertex(int brushIndex, int vertexIndex);
};
