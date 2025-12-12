#pragma once

#include <QVector3D>
#include <QMatrix4x4>

class QPainter;

enum class GizmoMode {
	None,
	Box,  // Old workflow
	Gizmo
};

enum class GizmoOperation {
	Translate,
	Rotate,
	Scale
};

enum class GizmoSpace {
	Global,
	Local
};

class Gizmo {
public:
	Gizmo();
	
	// Mode and operation
	void setMode(GizmoMode mode) { m_mode = mode; }
	GizmoMode mode() const { return m_mode; }
	void cycleMode(); // Cycles: None -> Box -> Gizmo -> None
	
	void setOperation(GizmoOperation op) { m_operation = op; }
	GizmoOperation operation() const { return m_operation; }
	void cycleOperation(); // Cycles: Translate -> Rotate -> Scale -> Translate
	
	void setSpace(GizmoSpace space) { m_space = space; }
	GizmoSpace space() const { return m_space; }
	void toggleSpace(); // Toggles between Global and Local
	
	// Axis locking
	void setLockedAxes(bool x, bool y, bool z);
	bool isAxisLocked(int axis) const; // 0=X, 1=Y, 2=Z
	
	// Position and orientation
	void setPosition(const QVector3D& pos) { m_position = pos; }
	QVector3D position() const { return m_position; }
	
	void setRotation(const QMatrix4x4& rot) { m_rotation = rot; }
	QMatrix4x4 rotation() const { return m_rotation; }
	
	// Interaction
	bool hitTest(const QVector3D& rayOrigin, const QVector3D& rayDir, int& hitAxis, float& hitDistance) const;
	QVector3D projectToAxis(const QVector3D& worldPos, int axis) const;
	
	// Rendering
	void render(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const;
	
	// Size
	void setSize(float size) { m_size = size; }
	float size() const { return m_size; }

private:
	GizmoMode m_mode{GizmoMode::Box};
	GizmoOperation m_operation{GizmoOperation::Translate};
	GizmoSpace m_space{GizmoSpace::Global};
	
	QVector3D m_position{0.0f, 0.0f, 0.0f};
	QMatrix4x4 m_rotation; // Identity by default
	float m_size{1.0f};
	
	bool m_lockedAxes[3]{false, false, false};
	
	void renderTranslateGizmo(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const;
	void renderRotateGizmo(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const;
	void renderScaleGizmo(QPainter& painter, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const;
	
	QVector3D screenToWorld(const QPoint& screenPos, const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const;
	float calculateScreenSize(const QMatrix4x4& viewMatrix, const QMatrix4x4& projMatrix, int viewportWidth, int viewportHeight) const;
};
