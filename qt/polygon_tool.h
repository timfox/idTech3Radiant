#pragma once

#include <QVector3D>
#include <QVector>
#include <QPoint>

QT_BEGIN_NAMESPACE
class QPainter;
QT_END_NAMESPACE

class PolygonTool {
public:
	PolygonTool();
	
	// Point management
	void addPoint(const QVector3D& point);
	void clearPoints();
	int pointCount() const { return m_points.size(); }
	QVector3D point(int index) const;
	
	// Validation
	bool isValid() const; // At least 3 points forming convex shape
	bool isConvex() const;
	
	// Conversion
	// In full implementation, would convert to brush with proper face texturing
	QVector<QVector3D> getTriangulatedFaces() const;
	
	// Visual feedback
	void setCurrentPoint(const QVector3D& point) { m_currentPoint = point; }
	QVector3D currentPoint() const { return m_currentPoint; }
	
	// Rendering
	void render(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const;
	
	// Real-time preview
	bool showPreview() const { return m_showPreview; }
	void setShowPreview(bool show) { m_showPreview = show; }

private:
	QVector<QVector3D> m_points;
	QVector3D m_currentPoint;
	bool m_showPreview{true};
	
	bool checkConvexity() const;
};
