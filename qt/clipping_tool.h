#pragma once

#include <QVector3D>
#include <QPoint>
#include <QVector>
#include <QRect>

QT_BEGIN_NAMESPACE
class QPainter;
QT_END_NAMESPACE

class ClippingTool {
public:
	ClippingTool();
	
	// Clipping mode
	void setThreePointMode(bool enabled) { m_threePointMode = enabled; }
	bool isThreePointMode() const { return m_threePointMode; }
	void toggleMode(); // Toggle between 2-point and 3-point
	
	// Clip points
	void addPoint(const QVector3D& point);
	void clearPoints();
	int pointCount() const { return m_points.size(); }
	QVector3D point(int index) const;
	
	// Clip plane calculation
	bool isValid() const;
	QVector3D planeNormal() const;
	QVector3D planePoint() const;
	
	// Entity clipping
	void setClipEntities(bool enabled) { m_clipEntities = enabled; }
	bool clipEntities() const { return m_clipEntities; }
	
	// Visual feedback
	QRect getScreenBounds() const { return m_screenBounds; }
	void setScreenBounds(const QRect& bounds) { m_screenBounds = bounds; }
	
	// Rendering
	void render(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const;
	
	// Object highlighting
	QVector<QVector3D> getObjectsToClip() const { return m_objectsToClip; }
	void setObjectsToClip(const QVector<QVector3D>& objects) { m_objectsToClip = objects; }
	
	// Handle manipulation
	bool hitTestHandle(const QVector3D& rayOrigin, const QVector3D& rayDir, int& handleIndex, float& hitDistance) const;
	void moveHandle(int handleIndex, const QVector3D& newPosition);

private:
	bool m_threePointMode{false};
	QVector<QVector3D> m_points;
	bool m_clipEntities{false};
	QRect m_screenBounds;
	QVector<QVector3D> m_objectsToClip; // Objects that will be clipped (highlighted in red)
	
	void calculatePlane();
	QVector3D m_planeNormal;
	QVector3D m_planePoint;
	
	void renderPlane(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const;
	void renderHandles(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const;
};
