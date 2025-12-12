#pragma once

#include <QVector3D>

class QTreeWidgetItem;

// CSG Tool (Shell Modifier) - create shell from selected brushes
class CSGTool {
public:
	CSGTool();
	
	// Create shell from selected brushes
	bool createShell(const QVector<QTreeWidgetItem*>& brushItems, float thickness);
	bool createShellFromBrush(int brushIndex, float thickness);
	
	// Settings
	void setThickness(float thickness) { m_thickness = thickness; }
	float thickness() const { return m_thickness; }
	
	void setInward(bool inward) { m_inward = inward; }
	bool inward() const { return m_inward; }
	
	// Preview
	void setPreviewThickness(float thickness) { m_previewThickness = thickness; }
	float previewThickness() const { return m_previewThickness; }
	bool hasPreview() const { return m_hasPreview; }
	void clearPreview() { m_hasPreview = false; }

private:
	float m_thickness{8.0f};
	bool m_inward{false}; // true = inward shell, false = outward shell
	float m_previewThickness{0.0f};
	bool m_hasPreview{false};
	
	// Shell creation helpers
	QVector3D getFaceNormal(int brushIndex, int faceIndex) const;
	bool validateShell(int brushIndex, float thickness) const;
};
