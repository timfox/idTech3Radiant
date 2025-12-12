#pragma once

#include <QString>
#include <QWidget>
#include <QElapsedTimer>
#include <QVector3D>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

#ifdef RADIANT_USE_ENGINE_RENDERER_VK
#include "tr_public.h"
#endif

// Forward declaration - gizmo.h included in .cpp
class Gizmo;

// Vulkan renderer host placeholder for Qt Radiant.
// Future work: wire the real engine renderer when RADIANT_USE_ENGINE_RENDERER_VK is enabled.
class VkViewportWidget final : public QWidget {
	Q_OBJECT
public:
	enum class ViewType {
		Perspective,
		Top,
		Front,
		Side
	};

	// Selection modes
	enum class SelectionMode {
		Object,  // Select whole objects/brushes
		Face,    // Select individual faces
		Edge,    // Select edges
		Vertex   // Select vertices
	};

	explicit VkViewportWidget(const QString& rendererPathHint = QString(),
	                          ViewType view = ViewType::Perspective,
	                          QWidget* parent = nullptr);

	bool loadRenderer(const QString& path);
	bool loadMap(const QString& path);
	QString status() const { return m_status; }
	QString rendererPath() const { return m_rendererPath; }
	QString viewName() const;
	bool hasSelection() const { return m_hasSelection; }
	QVector3D selectionPosition() const { return m_selection; }
	QVector<QVector3D> selectedObjects() const { return m_selectedObjects; }
	void clearSelection();
	void addToSelection(const QVector3D& pos);
	void setSelectionMode(int mode) { m_selectionMode = static_cast<SelectionMode>(mode); }
	SelectionMode selectionMode() const { return m_selectionMode; }
	
	// Gizmo support
	void setGizmoMode(int mode); // 0=None, 1=Box, 2=Gizmo
	int gizmoMode() const { return static_cast<int>(m_gizmoMode); }
	void setGizmoOperation(int op); // 0=Translate, 1=Rotate, 2=Scale
	void toggleGizmoSpace(); // Toggle Global/Local
	
	// Clipping tool support
	void setClippingTool(class ClippingTool* tool) { m_clippingTool = tool; }
	class ClippingTool* clippingTool() const { return m_clippingTool; }
	
	// Polygon tool support
	void setPolygonTool(class PolygonTool* tool) { m_polygonTool = tool; }
	class PolygonTool* polygonTool() const { return m_polygonTool; }
	
	// Vertex tool support
	void setVertexTool(class VertexTool* tool) { m_vertexTool = tool; }
	class VertexTool* vertexTool() const { return m_vertexTool; }
	
	// Texture paint tool support
	void setTexturePaintTool(class TexturePaintTool* tool) { m_texturePaintTool = tool; }
	class TexturePaintTool* texturePaintTool() const { return m_texturePaintTool; }
	
	// Extrusion tool support
	void setExtrusionTool(class ExtrusionTool* tool) { m_extrusionTool = tool; }
	class ExtrusionTool* extrusionTool() const { return m_extrusionTool; }
	void setExtrusionToolActive(bool active) { m_extrusionToolActive = active; }
	bool isExtrusionToolActive() const { return m_extrusionToolActive; }
	
	// Material/texture support
	void setCurrentMaterial(const QString& material) { m_currentMaterial = material; }
	QString currentMaterial() const { return m_currentMaterial; }

Q_SIGNALS:
	void selectionChanged(const QVector3D& pos);
	void gizmoOperationChanged(int operation); // 0=Translate, 1=Rotate, 2=Scale
	void lightPowerChanged(float delta); // Light power adjustment

protected:
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void showEvent(QShowEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
public:
	void focusOnSelection(); // Focus camera on selected object (Tab key)
	void resetCamera(); // Reset camera to default position

private:
	QString locateDefaultRenderer() const;
	void updateCameraText();
	void buildViewMatrices(QMatrix4x4& viewMatrix, QMatrix4x4& projMatrix) const;
	QVector3D screenToWorld(const QPoint& screenPos) const;

	QString m_rendererPath;
	QString m_status;
	bool m_loaded{false};
	QString m_mapPath;
	bool m_hasSelection{false};
	QVector3D m_selection{0.0f, 0.0f, 0.0f};

	// Simple camera state for orbit/fly preview
	QPoint m_lastPos;
	float m_yaw{0.0f};
	float m_pitch{20.0f};
	float m_distance{6.0f};
	float m_panX{0.0f};
	float m_panY{0.0f};
	QString m_cameraInfo;
	ViewType m_viewType{ViewType::Perspective};
	
	// WASD camera movement
	bool m_cameraKeys[4]{false, false, false, false}; // W, A, S, D
	bool m_cameraShift{false}; // Shift for faster movement
	QTimer* m_cameraUpdateTimer{nullptr};
	void updateCameraMovement();

	// Selection/manipulation state
	bool m_isDragging{false};
	QPoint m_dragStartPos;
	QVector3D m_dragStartSelection;
	bool m_dragSelecting{false}; // Marquee selection
	QRect m_dragSelectRect;
	QVector<QVector3D> m_selectedObjects; // Multi-object selection
	bool m_pendingMarqueeStart{false}; // Waiting to decide if this becomes a marquee
	qint64 m_dragStartTime{0}; // Timestamp for drag start
	
	// Gizmo (using int to avoid include dependency)
	class Gizmo* m_gizmo = nullptr;
	int m_gizmoMode{1}; // 0=None, 1=Box, 2=Gizmo
	
	// Clipping tool
	class ClippingTool* m_clippingTool = nullptr;
	
	// Polygon tool
	class PolygonTool* m_polygonTool = nullptr;
	
	// Vertex tool
	class VertexTool* m_vertexTool = nullptr;
	
	// Texture paint tool
	class TexturePaintTool* m_texturePaintTool = nullptr;
	
	// Extrusion tool
	class ExtrusionTool* m_extrusionTool = nullptr;
	bool m_extrusionToolActive{false};
	QString m_currentMaterial;

	QElapsedTimer m_frameTimer;
	qint64 m_lastFrameMs{0};

#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	refexport_t* m_re{nullptr};
#endif

	SelectionMode m_selectionMode{SelectionMode::Object};

	// Grid settings (static for all viewports)
	static float s_gridSize;
	static bool s_microgridEnabled;
	
public:
	static void setGridSize(float size);
	static float gridSize() { return s_gridSize; }
	static void setMicrogridEnabled(bool enabled);
	static bool isMicrogridEnabled() { return s_microgridEnabled; }
};
