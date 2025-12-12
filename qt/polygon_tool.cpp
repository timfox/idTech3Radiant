#include "polygon_tool.h"

#include <QPainter>
#include <QVector>

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QMatrix4x4>
#include <QPointF>
#include <QPolygon>
#include <cmath>
#include <algorithm>

PolygonTool::PolygonTool() {
}

void PolygonTool::addPoint(const QVector3D& point) {
	// Validate point (reasonable bounds)
	if (point.x() < -100000.0f || point.x() > 100000.0f ||
	    point.y() < -100000.0f || point.y() > 100000.0f ||
	    point.z() < -100000.0f || point.z() > 100000.0f) {
		return; // Point out of bounds
	}
	
	// Check for duplicate points (within tolerance)
	const float tolerance = 0.1f;
	for (const QVector3D& existing : m_points) {
		if ((existing - point).lengthSquared() < tolerance * tolerance) {
			return; // Duplicate point
		}
	}
	
	m_points.append(point);
}

void PolygonTool::clearPoints() {
	m_points.clear();
	m_currentPoint = QVector3D();
}

QVector3D PolygonTool::point(int index) const {
	if (index < 0 || index >= m_points.size()) {
		return QVector3D();
	}
	return m_points[index];
}

bool PolygonTool::isValid() const {
	return m_points.size() >= 3 && isConvex();
}

bool PolygonTool::isConvex() const {
	if (m_points.size() < 3) return false;
	
	// Simplified convexity check - in full implementation would be more robust
	// For now, assume points form a convex shape if they're in a reasonable order
	return checkConvexity();
}

bool PolygonTool::checkConvexity() const {
	if (m_points.size() < 3) return false;
	
	// Check coplanarity
	float z = m_points[0].z();
	for (int i = 1; i < m_points.size(); ++i) {
		if (std::abs(m_points[i].z() - z) > 0.01f) {
			return false; // Not coplanar
		}
	}
	
	// Check convexity using cross product method
	// For a convex polygon, all cross products should have the same sign
	if (m_points.size() < 3) return true;
	
	// Project to 2D (use X-Y plane)
	QVector<QPointF> points2D;
	for (const QVector3D& p : m_points) {
		points2D.append(QPointF(p.x(), p.y()));
	}
	
	// Check if polygon is convex by checking cross products
	float lastCross = 0.0f;
	for (int i = 0; i < points2D.size(); ++i) {
		int next = (i + 1) % points2D.size();
		int nextNext = (i + 2) % points2D.size();
		
		QPointF v1 = points2D[next] - points2D[i];
		QPointF v2 = points2D[nextNext] - points2D[next];
		
		float cross = v1.x() * v2.y() - v1.y() * v2.x();
		
		if (i > 0) {
			// Check if sign changed
			if ((lastCross > 0.0f && cross < 0.0f) || (lastCross < 0.0f && cross > 0.0f)) {
				return false; // Not convex
			}
		}
		
		lastCross = cross;
	}
	
	return true;
}

QVector<QVector3D> PolygonTool::getTriangulatedFaces() const {
	QVector<QVector3D> triangles;
	
	if (m_points.size() < 3) return triangles;
	
	// Simple fan triangulation from first point
	for (int i = 1; i < m_points.size() - 1; ++i) {
		triangles.append(m_points[0]);
		triangles.append(m_points[i]);
		triangles.append(m_points[i + 1]);
	}
	
	return triangles;
}

void PolygonTool::render(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const {
	if (m_points.isEmpty() && m_currentPoint.isNull()) return;
	
	// Project points to screen
	QVector<QPoint> screenPoints;
	
	for (const QVector3D& p : m_points) {
		QVector4D p4(p.x(), p.y(), p.z(), 1.0f);
		QVector4D viewP = viewMatrix * p4;
		QVector4D clipP = projMatrix * viewP;
		
		if (clipP.w() <= 0.0f) continue;
		
		float invW = 1.0f / clipP.w();
		screenPoints.append(QPoint(
			static_cast<int>((clipP.x() * invW + 1.0f) * 0.5f * viewportWidth),
			static_cast<int>((1.0f - clipP.y() * invW) * 0.5f * viewportHeight)
		));
	}
	
	// Draw polygon preview
	if (screenPoints.size() >= 2) {
		// Draw lines between points
		QColor lineColor = isValid() ? QColor(100, 200, 255) : QColor(255, 100, 100);
		painter.setPen(QPen(lineColor, 2));
		painter.setBrush(Qt::NoBrush);
		
		for (int i = 0; i < screenPoints.size(); ++i) {
			int next = (i + 1) % screenPoints.size();
			painter.drawLine(screenPoints[i], screenPoints[next]);
		}
		
		// Fill polygon if valid
		if (isValid() && screenPoints.size() >= 3) {
			QPolygon poly;
			for (const QPoint& p : screenPoints) {
				poly << p;
			}
			painter.setBrush(QBrush(QColor(100, 200, 255, 50)));
			painter.drawPolygon(poly);
		}
	}
	
	// Draw points
	for (int i = 0; i < screenPoints.size(); ++i) {
		QColor pointColor = isValid() ? QColor(100, 200, 255) : QColor(255, 100, 100);
		painter.setPen(QPen(pointColor, 2));
		painter.setBrush(QBrush(pointColor));
		painter.drawEllipse(screenPoints[i], 4, 4);
		
		// Draw point number
		painter.setPen(Qt::white);
		painter.drawText(screenPoints[i] + QPoint(6, -6), QString::number(i + 1));
	}
	
	// Draw current point preview
	if (!m_currentPoint.isNull() && m_showPreview) {
		QVector4D p4(m_currentPoint.x(), m_currentPoint.y(), m_currentPoint.z(), 1.0f);
		QVector4D viewP = viewMatrix * p4;
		QVector4D clipP = projMatrix * viewP;
		
		if (clipP.w() > 0.0f) {
			float invW = 1.0f / clipP.w();
			QPoint screenPos(
				static_cast<int>((clipP.x() * invW + 1.0f) * 0.5f * viewportWidth),
				static_cast<int>((1.0f - clipP.y() * invW) * 0.5f * viewportHeight)
			);
			
			// Draw preview point
			painter.setPen(QPen(QColor(255, 255, 0), 2));
			painter.setBrush(QBrush(QColor(255, 255, 0, 100)));
			painter.drawEllipse(screenPos, 5, 5);
			
			// Draw line from last point to preview
			if (!screenPoints.isEmpty()) {
				painter.setPen(QPen(QColor(255, 255, 0, 150), 1, Qt::DashLine));
				painter.drawLine(screenPoints.last(), screenPos);
			}
		}
	}
}
