#pragma once

#include <QVector3D>

class QTreeWidgetItem;

// Patch thicken tool - thicken selected patches
class PatchThickenTool {
public:
	PatchThickenTool();
	
	// Thicken selected patches
	bool thickenPatches(const QVector<QTreeWidgetItem*>& patchItems, float thickness);
	bool thickenPatch(int patchIndex, float thickness);
	
	// Settings
	void setThickness(float thickness) { m_thickness = thickness; }
	float thickness() const { return m_thickness; }
	
	// Preview
	void setPreviewThickness(float thickness) { m_previewThickness = thickness; }
	float previewThickness() const { return m_previewThickness; }
	bool hasPreview() const { return m_hasPreview; }
	void clearPreview() { m_hasPreview = false; }

private:
	float m_thickness{8.0f};
	float m_previewThickness{0.0f};
	bool m_hasPreview{false};
	
	// Thickening helpers
	QVector3D getPatchNormal(int patchIndex) const;
	bool validateThickening(int patchIndex, float thickness) const;
};
