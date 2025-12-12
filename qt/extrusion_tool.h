#pragma once

#include <QVector3D>

class QTreeWidgetItem;

// Brush face extrusion tool
class ExtrusionTool {
public:
	ExtrusionTool();
	
	// Extrude selected face(s)
	bool extrudeFace(int brushIndex, int faceIndex, float distance);
	bool extrudeSelectedFaces(const QVector<QTreeWidgetItem*>& brushItems, float distance);
	
	// Settings
	void setExtrudeDistance(float distance) { m_extrudeDistance = distance; }
	float extrudeDistance() const { return m_extrudeDistance; }
	
	// Preview
	void setPreviewDistance(float distance) { m_previewDistance = distance; }
	float previewDistance() const { return m_previewDistance; }
	bool hasPreview() const { return m_hasPreview; }
	void clearPreview() { m_hasPreview = false; }

private:
	float m_extrudeDistance{64.0f};
	float m_previewDistance{0.0f};
	bool m_hasPreview{false};
	
	// Extrusion helpers
	QVector3D getFaceNormal(int brushIndex, int faceIndex) const;
	QVector<QVector3D> getFaceVertices(int brushIndex, int faceIndex) const;
	bool validateExtrusion(int brushIndex, int faceIndex, float distance) const;
};
