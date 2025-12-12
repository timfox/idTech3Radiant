#pragma once

#include <QVector3D>
#include <QString>
#include <QPoint>
#include <QVector>

// Texture painting by drag tool
class TexturePaintTool {
public:
	TexturePaintTool();
	
	// Start painting
	void startPaint(const QVector3D& position, const QString& texture);
	void updatePaint(const QVector3D& position);
	void endPaint();
	
	// Settings
	void setTexture(const QString& texture) { m_texture = texture; }
	QString texture() const { return m_texture; }
	
	void setBrushSize(float size) { m_brushSize = size; }
	float brushSize() const { return m_brushSize; }
	
	void setPaintMode(int mode) { m_paintMode = mode; } // 0=Replace, 1=Blend
	int paintMode() const { return m_paintMode; }
	
	// Status
	bool isPainting() const { return m_isPainting; }
	QVector3D lastPaintPosition() const { return m_lastPaintPosition; }

private:
	QString m_texture;
	float m_brushSize{32.0f};
	int m_paintMode{0}; // 0=Replace, 1=Blend
	bool m_isPainting{false};
	QVector3D m_lastPaintPosition;
	QVector3D m_paintStartPosition;
	
	// Paint helpers
	void applyTextureToFaces(const QVector3D& position, float radius);
	QVector<int> getFacesInRadius(const QVector3D& position, float radius) const;
};
