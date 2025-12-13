#include "snapping_system.h"
#include "vk_viewport.h"

#include <QtMath>
#include <QDebug>
#include <limits>

SnappingSystem& SnappingSystem::instance() {
    static SnappingSystem instance;
    return instance;
}

SnappingSystem::SnappingSystem() {
    // Default snap modes: Grid enabled, others disabled
    m_snapModes[static_cast<int>(SnapMode::Grid)] = true;
}

SnappingSystem::~SnappingSystem() {
}

void SnappingSystem::setSnapMode(SnapMode mode, bool enabled) {
    m_snapModes[static_cast<int>(mode)] = enabled;
}

bool SnappingSystem::isSnapModeEnabled(SnapMode mode) const {
    return m_snapModes[static_cast<int>(mode)];
}

void SnappingSystem::setSnapThreshold(float threshold) {
    m_snapThreshold = threshold;
}

QVector3D SnappingSystem::snapPosition(const QVector3D& inputPos, const QVector3D& cursorPos) {
    if (cursorPos.isNull()) {
        // If no cursor position, just apply grid snapping
        if (isSnapModeEnabled(SnapMode::Grid) && VkViewportWidget::gridSize() > 0.0f) {
            return snapToGrid(inputPos, VkViewportWidget::gridSize());
        }
        return inputPos;
    }

    // Find the best snap target
    SnapTarget bestTarget = findBestSnapTarget(inputPos, cursorPos);

    if (bestTarget.distance <= m_snapThreshold) {
        return bestTarget.position;
    }

    // No snap target found, return original position
    return inputPos;
}

SnapTarget SnappingSystem::findBestSnapTarget(const QVector3D& inputPos, const QVector3D& cursorPos) {
    SnapTarget bestTarget;
    bestTarget.distance = std::numeric_limits<float>::max();
    bestTarget.position = inputPos;

    // Check all enabled snap modes
    if (isSnapModeEnabled(SnapMode::Point)) {
        SnapTarget pointTarget = findNearestPoint(cursorPos, m_snapThreshold);
        if (pointTarget.distance < bestTarget.distance) {
            bestTarget = pointTarget;
        }
    }

    if (isSnapModeEnabled(SnapMode::Edge)) {
        SnapTarget edgeTarget = findNearestEdge(cursorPos, m_snapThreshold);
        if (edgeTarget.distance < bestTarget.distance) {
            bestTarget = edgeTarget;
        }
    }

    if (isSnapModeEnabled(SnapMode::Face)) {
        SnapTarget faceTarget = findNearestFace(cursorPos, m_snapThreshold);
        if (faceTarget.distance < bestTarget.distance) {
            bestTarget = faceTarget;
        }
    }

    if (isSnapModeEnabled(SnapMode::Perpendicular)) {
        SnapTarget perpTarget = findPerpendicularSnap(cursorPos, m_snapThreshold);
        if (perpTarget.distance < bestTarget.distance) {
            bestTarget = perpTarget;
        }
    }

    // Grid snapping (always check last as fallback)
    if (isSnapModeEnabled(SnapMode::Grid) && VkViewportWidget::gridSize() > 0.0f) {
        QVector3D gridPos = snapToGrid(inputPos, VkViewportWidget::gridSize());
        float gridDistance = (gridPos - cursorPos).length();
        if (gridDistance < bestTarget.distance && gridDistance <= m_snapThreshold) {
            bestTarget.position = gridPos;
            bestTarget.distance = gridDistance;
            bestTarget.mode = SnapMode::Grid;
            bestTarget.description = "Grid";
        }
    }

    return bestTarget;
}

void SnappingSystem::registerPoint(const QVector3D& point) {
    if (!m_points.contains(point)) {
        m_points.append(point);
    }
}

void SnappingSystem::registerEdge(const QVector3D& start, const QVector3D& end) {
    QPair<QVector3D, QVector3D> edge = {start, end};
    if (!m_edges.contains(edge)) {
        m_edges.append(edge);
    }
}

void SnappingSystem::registerFace(const QVector<QVector3D>& vertices) {
    if (!m_faces.contains(vertices)) {
        m_faces.append(vertices);
    }
}

void SnappingSystem::clearSnapTargets() {
    m_points.clear();
    m_edges.clear();
    m_faces.clear();
}

QVector3D SnappingSystem::snapToGrid(const QVector3D& pos, float gridSize) {
    if (gridSize <= 0.0f) return pos;

    QVector3D snapped;
    snapped.setX(std::round(pos.x() / gridSize) * gridSize);
    snapped.setY(std::round(pos.y() / gridSize) * gridSize);
    snapped.setZ(std::round(pos.z() / gridSize) * gridSize);
    return snapped;
}

bool SnappingSystem::isGridSnapEnabled() {
    return instance().isSnapModeEnabled(SnapMode::Grid);
}

SnapTarget SnappingSystem::findNearestPoint(const QVector3D& pos, float maxDistance) {
    SnapTarget result;
    result.distance = maxDistance;
    result.position = pos;
    result.mode = SnapMode::Point;

    for (const QVector3D& point : m_points) {
        float distance = (point - pos).length();
        if (distance < result.distance) {
            result.distance = distance;
            result.position = point;
            result.description = "Point";
        }
    }

    return result;
}

SnapTarget SnappingSystem::findNearestEdge(const QVector3D& pos, float maxDistance) {
    SnapTarget result;
    result.distance = maxDistance;
    result.position = pos;
    result.mode = SnapMode::Edge;

    for (const auto& edge : m_edges) {
        QVector3D closestPoint = closestPointOnLine(pos, edge.first, edge.second);
        float distance = (closestPoint - pos).length();
        if (distance < result.distance) {
            result.distance = distance;
            result.position = closestPoint;
            result.description = "Edge";
        }
    }

    return result;
}

SnapTarget SnappingSystem::findNearestFace(const QVector3D& pos, float maxDistance) {
    SnapTarget result;
    result.distance = maxDistance;
    result.position = pos;
    result.mode = SnapMode::Face;

    for (const auto& face : m_faces) {
        if (face.size() < 3) continue; // Need at least 3 points for a face

        QVector3D closestPoint = closestPointOnPlane(pos, face);
        float distance = (closestPoint - pos).length();
        if (distance < result.distance) {
            result.distance = distance;
            result.position = closestPoint;
            result.description = "Face";
        }
    }

    return result;
}

SnapTarget SnappingSystem::findPerpendicularSnap(const QVector3D& pos, float maxDistance) {
    // Perpendicular snapping to existing edges
    // This finds points where the cursor forms perpendicular lines with existing edges
    SnapTarget result;
    result.distance = maxDistance;
    result.position = pos;
    result.mode = SnapMode::Perpendicular;

    // TODO: Implement perpendicular snapping algorithm
    // This would find points where lines from the cursor are perpendicular to edges
    // For now, return original position

    return result;
}

float SnappingSystem::pointToLineDistance(const QVector3D& point, const QVector3D& lineStart, const QVector3D& lineEnd) {
    QVector3D lineDir = (lineEnd - lineStart).normalized();
    QVector3D toPoint = point - lineStart;
    float projection = QVector3D::dotProduct(toPoint, lineDir);
    QVector3D closest = lineStart + lineDir * qBound(0.0f, projection, (lineEnd - lineStart).length());
    return (point - closest).length();
}

QVector3D SnappingSystem::closestPointOnLine(const QVector3D& point, const QVector3D& lineStart, const QVector3D& lineEnd) {
    QVector3D lineDir = (lineEnd - lineStart).normalized();
    QVector3D toPoint = point - lineStart;
    float projection = QVector3D::dotProduct(toPoint, lineDir);
    projection = qBound(0.0f, projection, (lineEnd - lineStart).length());
    return lineStart + lineDir * projection;
}

float SnappingSystem::pointToPlaneDistance(const QVector3D& point, const QVector<QVector3D>& face) {
    if (face.size() < 3) return std::numeric_limits<float>::max();

    // Calculate plane normal
    QVector3D v1 = face[1] - face[0];
    QVector3D v2 = face[2] - face[0];
    QVector3D normal = QVector3D::crossProduct(v1, v2).normalized();

    // Plane equation: ax + by + cz + d = 0
    float d = -QVector3D::dotProduct(normal, face[0]);
    float distance = qAbs(QVector3D::dotProduct(normal, point) + d) / normal.length();

    return distance;
}

QVector3D SnappingSystem::closestPointOnPlane(const QVector3D& point, const QVector<QVector3D>& face) {
    if (face.size() < 3) return point;

    // Calculate plane normal
    QVector3D v1 = face[1] - face[0];
    QVector3D v2 = face[2] - face[0];
    QVector3D normal = QVector3D::crossProduct(v1, v2).normalized();

    // Plane equation: ax + by + cz + d = 0
    float d = -QVector3D::dotProduct(normal, face[0]);

    // Project point onto plane
    float t = -(QVector3D::dotProduct(normal, point) + d) / QVector3D::dotProduct(normal, normal);
    return point + normal * t;
}
