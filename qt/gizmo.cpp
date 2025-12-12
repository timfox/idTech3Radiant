#include "gizmo.h"

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QPolygon>
#include <cmath>
#include <limits>

Gizmo::Gizmo() {
	m_rotation.setToIdentity();
}

void Gizmo::cycleMode() {
	switch (m_mode) {
	case GizmoMode::None:
		m_mode = GizmoMode::Box;
		break;
	case GizmoMode::Box:
		m_mode = GizmoMode::Gizmo;
		break;
	case GizmoMode::Gizmo:
		m_mode = GizmoMode::None;
		break;
	}
}

void Gizmo::cycleOperation() {
	switch (m_operation) {
	case GizmoOperation::Translate:
		m_operation = GizmoOperation::Rotate;
		break;
	case GizmoOperation::Rotate:
		m_operation = GizmoOperation::Scale;
		break;
	case GizmoOperation::Scale:
		m_operation = GizmoOperation::Translate;
		break;
	}
}

void Gizmo::toggleSpace() {
	m_space = (m_space == GizmoSpace::Global) ? GizmoSpace::Local : GizmoSpace::Global;
}

void Gizmo::setLockedAxes(bool x, bool y, bool z) {
	m_lockedAxes[0] = x;
	m_lockedAxes[1] = y;
	m_lockedAxes[2] = z;
}

bool Gizmo::isAxisLocked(int axis) const {
	if (axis < 0 || axis > 2) return false;
	return m_lockedAxes[axis];
}

bool Gizmo::hitTest(const QVector3D& rayOrigin, const QVector3D& rayDir, int& hitAxis, float& hitDistance) const {
	if (m_mode != GizmoMode::Gizmo) return false;
	
	// Validate inputs
	if (rayDir.lengthSquared() < 0.001f) return false; // Invalid ray direction
	if (m_size <= 0.0f) return false; // Invalid gizmo size
	
	// Hit test against gizmo axes
	// For translate/scale: test against axis lines
	// For rotate: test against rotation circles
	const float hitThreshold = m_size * 0.1f; // 10% of gizmo size
	float minDistance = std::numeric_limits<float>::max();
	int bestAxis = -1;
	
	QVector3D axes[3] = {
		QVector3D(1.0f, 0.0f, 0.0f),
		QVector3D(0.0f, 1.0f, 0.0f),
		QVector3D(0.0f, 0.0f, 1.0f)
	};
	
	if (m_operation == GizmoOperation::Translate || m_operation == GizmoOperation::Scale) {
		// Test against axis lines
		for (int i = 0; i < 3; ++i) {
			if (m_lockedAxes[i]) continue;
			
			QVector3D axisStart = m_position;
			QVector3D axisEnd = m_position + axes[i] * m_size;
			
			// Line-line intersection test
			QVector3D lineDir = (axisEnd - axisStart).normalized();
			QVector3D toStart = axisStart - rayOrigin;
			QVector3D cross = QVector3D::crossProduct(rayDir, lineDir);
			float crossLen = cross.length();
			
			if (crossLen < 0.001f) continue; // Parallel lines
			
			float t = QVector3D::dotProduct(QVector3D::crossProduct(toStart, lineDir), cross) / (crossLen * crossLen);
			float s = QVector3D::dotProduct(QVector3D::crossProduct(toStart, rayDir), cross) / (crossLen * crossLen);
			
			if (s >= 0.0f && s <= m_size && t > 0.0f) {
				QVector3D closestPoint = axisStart + lineDir * s;
				QVector3D rayPoint = rayOrigin + rayDir * t;
				float dist = (closestPoint - rayPoint).length();
				
				if (dist < hitThreshold && t < minDistance) {
					minDistance = t;
					bestAxis = i;
				}
			}
		}
	} else if (m_operation == GizmoOperation::Rotate) {
		// Test against rotation circles (simplified - test distance from circle plane)
		for (int i = 0; i < 3; ++i) {
			if (m_lockedAxes[i]) continue;
			
			// Get plane normal (the axis we're rotating around)
			QVector3D planeNormal = axes[i];
			
			// Project ray onto plane
			float distToPlane = QVector3D::dotProduct(m_position - rayOrigin, planeNormal) / QVector3D::dotProduct(rayDir, planeNormal);
			if (distToPlane < 0.0f) continue;
			
			QVector3D hitPoint = rayOrigin + rayDir * distToPlane;
			QVector3D toCenter = hitPoint - m_position;
			float distFromAxis = QVector3D::dotProduct(toCenter, planeNormal);
			toCenter = toCenter - planeNormal * distFromAxis; // Remove component along axis
			float distFromCenter = toCenter.length();
			float circleRadius = m_size;
			
			if (std::abs(distFromCenter - circleRadius) < hitThreshold && distToPlane < minDistance) {
				minDistance = distToPlane;
				bestAxis = i;
			}
		}
	}
	
	if (bestAxis >= 0) {
		hitAxis = bestAxis;
		hitDistance = minDistance;
		return true;
	}
	
	return false;
}

QVector3D Gizmo::projectToAxis(const QVector3D& worldPos, int axis) const {
	QVector3D result = worldPos;
	
	// Project to axis (0=X, 1=Y, 2=Z)
	if (axis == 0) {
		result.setY(m_position.y());
		result.setZ(m_position.z());
	} else if (axis == 1) {
		result.setX(m_position.x());
		result.setZ(m_position.z());
	} else if (axis == 2) {
		result.setX(m_position.x());
		result.setY(m_position.y());
	}
	
	return result;
}

void Gizmo::render(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const {
	if (m_mode != GizmoMode::Gizmo) return;
	
	switch (m_operation) {
	case GizmoOperation::Translate:
		renderTranslateGizmo(painter, viewMatrix, projMatrix, viewportWidth, viewportHeight);
		break;
	case GizmoOperation::Rotate:
		renderRotateGizmo(painter, viewMatrix, projMatrix, viewportWidth, viewportHeight);
		break;
	case GizmoOperation::Scale:
		renderScaleGizmo(painter, viewMatrix, projMatrix, viewportWidth, viewportHeight);
		break;
	}
}

void Gizmo::renderTranslateGizmo(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const {
	// Render Maya-style translation gizmo (arrows for X, Y, Z axes)
	// Maya uses brighter, more saturated colors with better visual feedback

	QColor colors[3] = {QColor(255, 0, 0), QColor(0, 255, 0), QColor(0, 0, 255)}; // Maya-style bright colors
	QVector3D axes[3] = {
		QVector3D(1.0f, 0.0f, 0.0f),
		QVector3D(0.0f, 1.0f, 0.0f),
		QVector3D(0.0f, 0.0f, 1.0f)
	};

	// Transform axes to local space if needed
	QMatrix4x4 transform = m_rotation;
	if (m_space == GizmoSpace::Local) {
		// Axes are already in local space via rotation matrix
	}

	// Calculate screen space size based on distance (Maya-style scaling)
	float screenSize = calculateScreenSize(viewMatrix, projMatrix, viewportWidth, viewportHeight);
	
	// Project position to screen
	QVector4D pos4(m_position.x(), m_position.y(), m_position.z(), 1.0f);
	QVector4D viewPos = viewMatrix * pos4;
	QVector4D clipPos = projMatrix * viewPos;
	
	if (clipPos.w() <= 0.0f) return; // Behind camera
	
	float invW = 1.0f / clipPos.w();
	QPoint centerPos(
		static_cast<int>((clipPos.x() * invW + 1.0f) * 0.5f * viewportWidth),
		static_cast<int>((1.0f - clipPos.y() * invW) * 0.5f * viewportHeight)
	);
	
	for (int i = 0; i < 3; ++i) {
		if (m_lockedAxes[i]) {
			// Draw locked axis in gray
			QVector3D axis = transform.map(axes[i]);
			QVector3D axisEnd = m_position + axis * m_size;
			
			QVector4D end4(axisEnd.x(), axisEnd.y(), axisEnd.z(), 1.0f);
			QVector4D viewEnd = viewMatrix * end4;
			QVector4D clipEnd = projMatrix * viewEnd;
			
			if (clipEnd.w() <= 0.0f) continue;
			
			float invWEnd = 1.0f / clipEnd.w();
			QPoint endPos(
				static_cast<int>((clipEnd.x() * invWEnd + 1.0f) * 0.5f * viewportWidth),
				static_cast<int>((1.0f - clipEnd.y() * invWEnd) * 0.5f * viewportHeight)
			);
			
			painter.setPen(QPen(QColor(100, 100, 100), 2, Qt::DashLine));
			painter.drawLine(centerPos, endPos);
			continue;
		}
		
		QVector3D axis = transform.map(axes[i]);
		QVector3D axisEnd = m_position + axis * m_size;
		
		QVector4D end4(axisEnd.x(), axisEnd.y(), axisEnd.z(), 1.0f);
		QVector4D viewEnd = viewMatrix * end4;
		QVector4D clipEnd = projMatrix * viewEnd;
		
		if (clipEnd.w() <= 0.0f) continue;
		
		float invWEnd = 1.0f / clipEnd.w();
		QPoint endPos(
			static_cast<int>((clipEnd.x() * invWEnd + 1.0f) * 0.5f * viewportWidth),
			static_cast<int>((1.0f - clipEnd.y() * invWEnd) * 0.5f * viewportHeight)
		);
		
		// Draw axis line with screen-size based thickness
		float lineThickness = std::max(screenSize * 0.02f, 2.0f);
		painter.setPen(QPen(colors[i], static_cast<int>(lineThickness)));
		painter.drawLine(centerPos, endPos);
		
		// Draw Maya-style cone arrow head
		QPoint dir = endPos - centerPos;
		float dirLen = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
		if (dirLen > 0.0f) {
			QPoint normalized(dir.x() / dirLen, dir.y() / dirLen);

			// Calculate cone size based on screen size
			float coneLength = std::max(screenSize * 0.15f, 8.0f);
			float coneWidth = std::max(screenSize * 0.08f, 4.0f);

			QPoint coneBase = endPos - normalized * coneLength;
			QPoint coneLeft(coneBase.x() - normalized.y() * coneWidth, coneBase.y() + normalized.x() * coneWidth);
			QPoint coneRight(coneBase.x() + normalized.y() * coneWidth, coneBase.y() - normalized.x() * coneWidth);

			QPolygon cone;
			cone << endPos << coneLeft << coneRight;
			painter.setBrush(QBrush(colors[i]));
			painter.drawPolygon(cone);

			// Draw cone outline for better visibility
			painter.setPen(QPen(colors[i].darker(150), 1));
			painter.drawLine(endPos, coneLeft);
			painter.drawLine(endPos, coneRight);
			painter.drawLine(coneLeft, coneRight);
		}
	}

	// Draw center sphere (Maya-style - yellow/orange center)
	float centerSize = std::max(screenSize * 0.04f, 3.0f);
	QColor centerColor(255, 200, 0); // Maya-style center color
	painter.setPen(QPen(centerColor.darker(200), 1));
	painter.setBrush(QBrush(centerColor));
	painter.drawEllipse(centerPos, static_cast<int>(centerSize), static_cast<int>(centerSize));
}

void Gizmo::renderRotateGizmo(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const {
	// Render rotation gizmo (circles for X, Y, Z axes)
	QColor colors[3] = {QColor(255, 80, 80), QColor(80, 255, 80), QColor(80, 120, 255)};
	QVector3D axes[3] = {
		QVector3D(1.0f, 0.0f, 0.0f),
		QVector3D(0.0f, 1.0f, 0.0f),
		QVector3D(0.0f, 0.0f, 1.0f)
	};
	
	QMatrix4x4 transform = m_rotation;
	
	// Project position to screen
	QVector4D pos4(m_position.x(), m_position.y(), m_position.z(), 1.0f);
	QVector4D viewPos = viewMatrix * pos4;
	QVector4D clipPos = projMatrix * viewPos;
	
	if (clipPos.w() <= 0.0f) return;
	
	float invW = 1.0f / clipPos.w();
	QPoint centerPos(
		static_cast<int>((clipPos.x() * invW + 1.0f) * 0.5f * viewportWidth),
		static_cast<int>((1.0f - clipPos.y() * invW) * 0.5f * viewportHeight)
	);
	
	for (int i = 0; i < 3; ++i) {
		if (m_lockedAxes[i]) {
			// Draw locked axis in gray (dashed circle)
			painter.setPen(QPen(QColor(100, 100, 100), 2, Qt::DashLine));
			painter.setBrush(Qt::NoBrush);
			int radius = static_cast<int>(m_size * 30);
			painter.drawEllipse(centerPos, radius, radius);
			continue;
		}
		
		QVector3D axis = transform.map(axes[i]);
		
		// Calculate circle radius in screen space
		QVector3D circlePoint = m_position + axis * m_size;
		QVector4D circle4(circlePoint.x(), circlePoint.y(), circlePoint.z(), 1.0f);
		QVector4D viewCircle = viewMatrix * circle4;
		QVector4D clipCircle = projMatrix * viewCircle;
		
		if (clipCircle.w() <= 0.0f) continue;
		
		float invWCircle = 1.0f / clipCircle.w();
		QPoint circleScreenPos(
			static_cast<int>((clipCircle.x() * invWCircle + 1.0f) * 0.5f * viewportWidth),
			static_cast<int>((1.0f - clipCircle.y() * invWCircle) * 0.5f * viewportHeight)
		);
		
		int radius = static_cast<int>((circleScreenPos - centerPos).manhattanLength());
		
		// Draw circle for rotation axis
		painter.setPen(QPen(colors[i], 2));
		painter.setBrush(Qt::NoBrush);
		painter.drawEllipse(centerPos, radius, radius);
		
		// Draw tick marks for rotation reference
		const int numTicks = 8;
		for (int t = 0; t < numTicks; ++t) {
			float angle = (t * 2.0f * M_PI) / numTicks;
			QVector3D tickDir(std::cos(angle), std::sin(angle), 0.0f);
			// Rotate tick direction to align with axis plane
			// Simplified - would use proper rotation in full implementation
			QPoint tickEnd(
				centerPos.x() + static_cast<int>(tickDir.x() * radius),
				centerPos.y() + static_cast<int>(tickDir.y() * radius)
			);
			painter.drawLine(centerPos, tickEnd);
		}
	}
	
	// Draw center sphere
	painter.setPen(QPen(Qt::white, 2));
	painter.setBrush(QBrush(Qt::white));
	painter.drawEllipse(centerPos, 4, 4);
}

void Gizmo::renderScaleGizmo(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const {
	// Render scale gizmo (boxes at ends of axes)
	QColor colors[3] = {QColor(255, 80, 80), QColor(80, 255, 80), QColor(80, 120, 255)};
	QVector3D axes[3] = {
		QVector3D(1.0f, 0.0f, 0.0f),
		QVector3D(0.0f, 1.0f, 0.0f),
		QVector3D(0.0f, 0.0f, 1.0f)
	};
	
	QMatrix4x4 transform = m_rotation;
	
	// Project position to screen
	QVector4D pos4(m_position.x(), m_position.y(), m_position.z(), 1.0f);
	QVector4D viewPos = viewMatrix * pos4;
	QVector4D clipPos = projMatrix * viewPos;
	
	if (clipPos.w() <= 0.0f) return;
	
	float invW = 1.0f / clipPos.w();
	QPoint centerPos(
		static_cast<int>((clipPos.x() * invW + 1.0f) * 0.5f * viewportWidth),
		static_cast<int>((1.0f - clipPos.y() * invW) * 0.5f * viewportHeight)
	);
	
	for (int i = 0; i < 3; ++i) {
		if (m_lockedAxes[i]) {
			// Draw locked axis in gray
			QVector3D axis = transform.map(axes[i]);
			QVector3D axisEnd = m_position + axis * m_size;
			
			QVector4D end4(axisEnd.x(), axisEnd.y(), axisEnd.z(), 1.0f);
			QVector4D viewEnd = viewMatrix * end4;
			QVector4D clipEnd = projMatrix * viewEnd;
			
			if (clipEnd.w() <= 0.0f) continue;
			
			float invWEnd = 1.0f / clipEnd.w();
			QPoint endPos(
				static_cast<int>((clipEnd.x() * invWEnd + 1.0f) * 0.5f * viewportWidth),
				static_cast<int>((1.0f - clipEnd.y() * invWEnd) * 0.5f * viewportHeight)
			);
			
			painter.setPen(QPen(QColor(100, 100, 100), 2, Qt::DashLine));
			painter.drawLine(centerPos, endPos);
			painter.setBrush(QBrush(QColor(100, 100, 100)));
			painter.drawRect(endPos.x() - 4, endPos.y() - 4, 8, 8);
			continue;
		}
		
		QVector3D axis = transform.map(axes[i]);
		QVector3D axisEnd = m_position + axis * m_size;
		
		QVector4D end4(axisEnd.x(), axisEnd.y(), axisEnd.z(), 1.0f);
		QVector4D viewEnd = viewMatrix * end4;
		QVector4D clipEnd = projMatrix * viewEnd;
		
		if (clipEnd.w() <= 0.0f) continue;
		
		float invWEnd = 1.0f / clipEnd.w();
		QPoint endPos(
			static_cast<int>((clipEnd.x() * invWEnd + 1.0f) * 0.5f * viewportWidth),
			static_cast<int>((1.0f - clipEnd.y() * invWEnd) * 0.5f * viewportHeight)
		);
		
		// Draw axis line
		painter.setPen(QPen(colors[i], 2));
		painter.drawLine(centerPos, endPos);
		
		// Draw box at end
		painter.setBrush(QBrush(colors[i]));
		painter.setPen(QPen(colors[i].darker(), 1));
		painter.drawRect(endPos.x() - 6, endPos.y() - 6, 12, 12);
	}
	
	// Draw center box (uniform scale handle)
	painter.setPen(QPen(Qt::white, 2));
	painter.setBrush(QBrush(Qt::white));
	painter.drawRect(centerPos.x() - 5, centerPos.y() - 5, 10, 10);
}

float Gizmo::calculateScreenSize(const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const {
	// Calculate screen-space size for gizmo scaling (Maya-style)
	// This ensures gizmos appear consistent size regardless of distance

	QVector4D pos4(m_position.x(), m_position.y(), m_position.z(), 1.0f);
	QVector4D viewPos = viewMatrix * pos4;
	QVector4D clipPos = projMatrix * viewPos;

	if (clipPos.w() <= 0.0f) return 50.0f; // Default size if behind camera

	float invW = 1.0f / clipPos.w();
	QPoint screenPos(
		static_cast<int>((clipPos.x() * invW + 1.0f) * 0.5f * viewportWidth),
		static_cast<int>((1.0f - clipPos.y() * invW) * 0.5f * viewportHeight)
	);

	// Calculate a point one unit away and see how many pixels that is
	QVector3D offsetPos = m_position + QVector3D(1.0f, 0.0f, 0.0f);
	QVector4D offset4(offsetPos.x(), offsetPos.y(), offsetPos.z(), 1.0f);
	QVector4D offsetView = viewMatrix * offset4;
	QVector4D offsetClip = projMatrix * offsetView;

	if (offsetClip.w() <= 0.0f) return 50.0f;

	float offsetInvW = 1.0f / offsetClip.w();
	QPoint offsetScreenPos(
		static_cast<int>((offsetClip.x() * offsetInvW + 1.0f) * 0.5f * viewportWidth),
		static_cast<int>((1.0f - offsetClip.y() * offsetInvW) * 0.5f * viewportHeight)
	);

	float pixelDistance = QPoint(offsetScreenPos - screenPos).manhattanLength();
	return std::max(pixelDistance * 0.8f, 20.0f); // Scale factor for good visibility
}

QVector3D Gizmo::screenToWorld(const QPoint& screenPos, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const {
	// Simplified screen to world conversion
	// In full implementation, would use proper unprojection
	Q_UNUSED(viewMatrix);
	Q_UNUSED(projMatrix);
	Q_UNUSED(viewportWidth);
	Q_UNUSED(viewportHeight);
	return QVector3D(static_cast<float>(screenPos.x()), static_cast<float>(screenPos.y()), 0.0f);
}
