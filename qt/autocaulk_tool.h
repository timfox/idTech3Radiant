#pragma once

#include <QString>

class QTreeWidgetItem;

// Autocaulk tool - automatically apply nodraw texture to unseen faces
class AutocaulkTool {
public:
	AutocaulkTool();
	
	// Apply autocaulk to selected brushes or all brushes
	void applyToSelection(const QVector<QTreeWidgetItem*>& brushItems);
	void applyToAll();
	
	// Settings
	void setNodrawTexture(const QString& texture) { m_nodrawTexture = texture; }
	QString nodrawTexture() const { return m_nodrawTexture; }
	
	// Statistics
	int facesCaulked() const { return m_facesCaulked; }
	int brushesProcessed() const { return m_brushesProcessed; }
	
	// Reset statistics
	void resetStats() { m_facesCaulked = 0; m_brushesProcessed = 0; }

private:
	QString m_nodrawTexture{QStringLiteral("common/nodraw")};
	int m_facesCaulked{0};
	int m_brushesProcessed{0};
	
	// Face visibility checking
	bool isFaceVisible(int brushIndex, int faceIndex) const;
	bool isFaceFacingOutward(int brushIndex, int faceIndex) const;
	
	// Apply caulk to a single brush
	void applyToBrush(QTreeWidgetItem* brushItem);
};
