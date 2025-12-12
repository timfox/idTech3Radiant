#include "clipping_tool.h"

#include <QPainter>
#include <QVector>

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QMatrix4x4>
#include <QPolygon>
#include <cmath>
#include <limits>

ClippingTool::ClippingTool() {
	m_planeNormal = QVector3D(0.0f, 0.0f, 1.0f);
	m_planePoint = QVector3D(0.0f, 0.0f, 0.0f);
}

void ClippingTool::toggleMode() {
	m_threePointMode = !m_threePointMode;
	if (m_threePointMode && m_points.size() > 3) {
		// Keep only first 3 points
		m_points.resize(3);
	} else if (!m_threePointMode && m_points.size() > 2) {
		// Keep only first 2 points
		m_points.resize(2);
	}
	calculatePlane();
}

void ClippingTool::addPoint(const QVector3D& point) {
	if (m_threePointMode) {
		if (m_points.size() >= 3) {
			m_points.clear();
		}
	} else {
		if (m_points.size() >= 2) {
			m_points.clear();
		}
	}
	m_points.append(point);
	calculatePlane();
}

void ClippingTool::clearPoints() {
	m_points.clear();
	m_planeNormal = QVector3D(0.0f, 0.0f, 1.0f);
	m_planePoint = QVector3D(0.0f, 0.0f, 0.0f);
}

QVector3D ClippingTool::point(int index) const {
	if (index < 0 || index >= m_points.size()) {
		return QVector3D();
	}
	return m_points[index];
}

bool ClippingTool::isValid() const {
	if (m_threePointMode) {
		return m_points.size() == 3;
	} else {
		return m_points.size() == 2;
	}
}

QVector3D ClippingTool::planeNormal() const {
	return m_planeNormal;
}

QVector3D ClippingTool::planePoint() const {
	return m_planePoint;
}

void ClippingTool::calculatePlane() {
	if (!isValid()) {
		return;
	}
	
	if (m_threePointMode && m_points.size() == 3) {
		// Calculate plane from 3 points
		QVector3D v1 = m_points[1] - m_points[0];
		QVector3D v2 = m_points[2] - m_points[0];
		m_planeNormal = QVector3D::crossProduct(v1, v2).normalized();
		if (m_planeNormal.length() < 0.001f) {
			// Points are collinear, use default
			m_planeNormal = QVector3D(0.0f, 0.0f, 1.0f);
		}
		m_planePoint = m_points[0];
	} else if (!m_threePointMode && m_points.size() == 2) {
		// For 2-point mode, create a plane perpendicular to view
		// In full implementation, would use view direction
			Q_UNUSED((m_points[1] - m_points[0]).normalized()); // Would use dir for plane calculation
		// Default to Z-up plane
		m_planeNormal = QVector3D(0.0f, 0.0f, 1.0f);
		m_planePoint = (m_points[0] + m_points[1]) * 0.5f;
	}
}

void ClippingTool::render(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const {
	if (!isValid()) {
		// Render points only
		renderHandles(painter, viewMatrix, projMatrix, viewportWidth, viewportHeight);
		return;
	}
	
	// Render clip plane
	renderPlane(painter, viewMatrix, projMatrix, viewportWidth, viewportHeight);
	
	// Render handles
	renderHandles(painter, viewMatrix, projMatrix, viewportWidth, viewportHeight);
	
	// Render objects that will be clipped (highlighted in red)
	painter.setPen(QPen(QColor(255, 0, 0, 200), 2));
	painter.setBrush(QBrush(QColor(255, 0, 0, 50)));
	
	for (const QVector3D& objPos : m_objectsToClip) {
		QVector4D obj4(objPos.x(), objPos.y(), objPos.z(), 1.0f);
		QVector4D viewObj = viewMatrix * obj4;
		QVector4D clipObj = projMatrix * viewObj;
		
		if (clipObj.w() <= 0.0f) continue;
		
		float invW = 1.0f / clipObj.w();
		QPoint screenPos(
			static_cast<int>((clipObj.x() * invW + 1.0f) * 0.5f * viewportWidth),
			static_cast<int>((1.0f - clipObj.y() * invW) * 0.5f * viewportHeight)
		);
		
		// Draw highlight circle
		painter.drawEllipse(screenPos, 8, 8);
	}
}

void ClippingTool::renderPlane(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const {
	// Render clip plane as a semi-transparent quad
	const float planeSize = 512.0f; // Size of plane visualization
	
	// Calculate two vectors in the plane
	QVector3D right, up;
	if (std::abs(m_planeNormal.x()) < 0.9f) {
		right = QVector3D::crossProduct(m_planeNormal, QVector3D(1.0f, 0.0f, 0.0f)).normalized();
	} else {
		right = QVector3D::crossProduct(m_planeNormal, QVector3D(0.0f, 1.0f, 0.0f)).normalized();
	}
	up = QVector3D::crossProduct(right, m_planeNormal).normalized();
	
	// Create plane corners
	QVector3D corners[4] = {
		m_planePoint - right * planeSize - up * planeSize,
		m_planePoint + right * planeSize - up * planeSize,
		m_planePoint + right * planeSize + up * planeSize,
		m_planePoint - right * planeSize + up * planeSize
	};
	
	// Project corners to screen
	QPoint screenCorners[4];
	bool allVisible = true;
	for (int i = 0; i < 4; ++i) {
		QVector4D corner4(corners[i].x(), corners[i].y(), corners[i].z(), 1.0f);
		QVector4D viewCorner = viewMatrix * corner4;
		QVector4D clipCorner = projMatrix * viewCorner;
		
		if (clipCorner.w() <= 0.0f) {
			allVisible = false;
			break;
		}
		
		float invW = 1.0f / clipCorner.w();
		screenCorners[i] = QPoint(
			static_cast<int>((clipCorner.x() * invW + 1.0f) * 0.5f * viewportWidth),
			static_cast<int>((1.0f - clipCorner.y() * invW) * 0.5f * viewportHeight)
		);
	}
	
	if (allVisible) {
		// Draw plane quad
		painter.setPen(QPen(QColor(100, 200, 255, 150), 1));
		painter.setBrush(QBrush(QColor(100, 200, 255, 50)));
		QPolygon planePoly;
		for (int i = 0; i < 4; ++i) {
			planePoly << screenCorners[i];
		}
		painter.drawPolygon(planePoly);
		
		// Draw plane outline
		painter.setPen(QPen(QColor(100, 200, 255, 200), 2));
		painter.setBrush(Qt::NoBrush);
		painter.drawPolygon(planePoly);
	}
	
	// Draw plane normal arrow
	QVector3D normalEnd = m_planePoint + m_planeNormal * 32.0f;
	QVector4D start4(m_planePoint.x(), m_planePoint.y(), m_planePoint.z(), 1.0f);
	QVector4D end4(normalEnd.x(), normalEnd.y(), normalEnd.z(), 1.0f);
	
	QVector4D viewStart = viewMatrix * start4;
	QVector4D viewEnd = viewMatrix * end4;
	QVector4D clipStart = projMatrix * viewStart;
	QVector4D clipEnd = projMatrix * viewEnd;
	
	if (clipStart.w() > 0.0f && clipEnd.w() > 0.0f) {
		float invWStart = 1.0f / clipStart.w();
		float invWEnd = 1.0f / clipEnd.w();
		QPoint startPos(
			static_cast<int>((clipStart.x() * invWStart + 1.0f) * 0.5f * viewportWidth),
			static_cast<int>((1.0f - clipStart.y() * invWStart) * 0.5f * viewportHeight)
		);
		QPoint endPos(
			static_cast<int>((clipEnd.x() * invWEnd + 1.0f) * 0.5f * viewportWidth),
			static_cast<int>((1.0f - clipEnd.y() * invWEnd) * 0.5f * viewportHeight)
		);
		
		painter.setPen(QPen(QColor(255, 255, 0), 2));
		painter.drawLine(startPos, endPos);
	}
}

void ClippingTool::renderHandles(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const {
	// Render handles for each point
	for (int i = 0; i < m_points.size(); ++i) {
		QVector4D point4(m_points[i].x(), m_points[i].y(), m_points[i].z(), 1.0f);
		QVector4D viewPoint = viewMatrix * point4;
		QVector4D clipPoint = projMatrix * viewPoint;
		
		if (clipPoint.w() <= 0.0f) continue;
		
		float invW = 1.0f / clipPoint.w();
		QPoint screenPos(
			static_cast<int>((clipPoint.x() * invW + 1.0f) * 0.5f * viewportWidth),
			static_cast<int>((1.0f - clipPoint.y() * invW) * 0.5f * viewportHeight)
		);
		
		// Draw handle
		QColor handleColor = (i < (m_threePointMode ? 3 : 2)) ? QColor(100, 200, 255) : QColor(200, 200, 200);
		painter.setPen(QPen(handleColor, 2));
		painter.setBrush(QBrush(handleColor));
		painter.drawEllipse(screenPos, 6, 6);
		
		// Draw point number
		painter.setPen(Qt::white);
		painter.drawText(screenPos + QPoint(8, -8), QString::number(i + 1));
	}
}

bool ClippingTool::hitTestHandle(const QVector3D& rayOrigin, const QVector3D& rayDir, int& handleIndex, float& hitDistance) const {
	const float handleRadius = 8.0f; // Screen space radius
	float minDistance = std::numeric_limits<float>::max();
	int bestHandle = -1;
	
	for (int i = 0; i < m_points.size(); ++i) {
		// Ray-sphere intersection test
		QVector3D toPoint = m_points[i] - rayOrigin;
		float proj = QVector3D::dotProduct(toPoint, rayDir);
		
		if (proj < 0.0f) continue; // Behind ray origin
		
		QVector3D closestPoint = rayOrigin + rayDir * proj;
		float distSq = (closestPoint - m_points[i]).lengthSquared();
		
		if (distSq < handleRadius * handleRadius && proj < minDistance) {
			minDistance = proj;
			bestHandle = i;
		}
	}
	
	if (bestHandle >= 0) {
		handleIndex = bestHandle;
		hitDistance = minDistance;
		return true;
	}
	
	return false;
}

void ClippingTool::moveHandle(int handleIndex, const QVector3D& newPosition) {
	if (handleIndex < 0 || handleIndex >= m_points.size()) return;
	
	m_points[handleIndex] = newPosition;
	calculatePlane();
}
