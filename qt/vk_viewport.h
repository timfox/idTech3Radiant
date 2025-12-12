#pragma once

#include <QString>
#include <QWidget>
#include <QElapsedTimer>
#include <QVector3D>

#ifdef RADIANT_USE_ENGINE_RENDERER_VK
#include "tr_public.h"
#endif

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

Q_SIGNALS:
	void selectionChanged(const QVector3D& pos);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;

private:
	QString locateDefaultRenderer() const;
	void updateCameraText();

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

	// Selection/manipulation state
	bool m_isDragging{false};
	QPoint m_dragStartPos;
	QVector3D m_dragStartSelection;

	QElapsedTimer m_frameTimer;
	qint64 m_lastFrameMs{0};

#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	refexport_t* m_re{nullptr};
#endif
};
