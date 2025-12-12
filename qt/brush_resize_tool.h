#pragma once

#include <QVector3D>
#include <QVector>

class QTreeWidgetItem;

// Brush resize tool - reduce selected faces to most wanted ones
class BrushResizeTool {
public:
	BrushResizeTool();
	
	// Resize selected brushes
	bool resizeBrushes(const QVector<QTreeWidgetItem*>& brushItems);
	bool resizeBrush(int brushIndex);
	
	// Settings
	void setReduceToMinimal(bool minimal) { m_reduceToMinimal = minimal; }
	bool reduceToMinimal() const { return m_reduceToMinimal; }
	
	void setPreserveFaces(const QVector<int>& faceIndices) { m_preserveFaces = faceIndices; }
	QVector<int> preserveFaces() const { return m_preserveFaces; }

private:
	bool m_reduceToMinimal{true};
	QVector<int> m_preserveFaces;
	
	// Resize helpers
	QVector<int> getMostWantedFaces(int brushIndex) const;
	bool validateResize(int brushIndex) const;
};
