#pragma once

#include <QVector3D>
#include <QVector>
#include <QPair>

enum class SnapMode {
    Grid,       // Snap to grid points
    Point,      // Snap to vertices/points
    Edge,       // Snap to edges
    Face,       // Snap to faces/surfaces
    Perpendicular // Snap to perpendicular lines
};

struct SnapTarget {
    QVector3D position;
    SnapMode mode;
    float distance; // Distance from cursor to snap point
    QString description; // For UI feedback
};

class SnappingSystem {
public:
    static SnappingSystem& instance();

    // Snap modes
    void setSnapMode(SnapMode mode, bool enabled);
    bool isSnapModeEnabled(SnapMode mode) const;
    void setSnapThreshold(float threshold); // Maximum snap distance

    // Snapping functions
    QVector3D snapPosition(const QVector3D& inputPos, const QVector3D& cursorPos = QVector3D());
    SnapTarget findBestSnapTarget(const QVector3D& inputPos, const QVector3D& cursorPos = QVector3D());

    // Register snap targets (called by tools/geometry)
    void registerPoint(const QVector3D& point);
    void registerEdge(const QVector3D& start, const QVector3D& end);
    void registerFace(const QVector<QVector3D>& vertices);
    void clearSnapTargets();

    // Grid snapping (existing functionality)
    static QVector3D snapToGrid(const QVector3D& pos, float gridSize);
    static bool isGridSnapEnabled();

private:
    SnappingSystem();
    ~SnappingSystem();

    // Snap mode states
    bool m_snapModes[5] = {true, false, false, false, false}; // Grid enabled by default
    float m_snapThreshold = 0.5f; // Maximum snap distance

    // Registered snap targets
    QVector<QVector3D> m_points;
    QVector<QPair<QVector3D, QVector3D>> m_edges;
    QVector<QVector<QVector3D>> m_faces;

    // Helper functions
    SnapTarget findNearestPoint(const QVector3D& pos, float maxDistance);
    SnapTarget findNearestEdge(const QVector3D& pos, float maxDistance);
    SnapTarget findNearestFace(const QVector3D& pos, float maxDistance);
    SnapTarget findPerpendicularSnap(const QVector3D& pos, float maxDistance);

    // Geometry helpers
    static float pointToLineDistance(const QVector3D& point, const QVector3D& lineStart, const QVector3D& lineEnd);
    static QVector3D closestPointOnLine(const QVector3D& point, const QVector3D& lineStart, const QVector3D& lineEnd);
    static float pointToPlaneDistance(const QVector3D& point, const QVector<QVector3D>& face);
    static QVector3D closestPointOnPlane(const QVector3D& point, const QVector<QVector3D>& face);
};