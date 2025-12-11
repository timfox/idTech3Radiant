#pragma once

#include <QString>
#include <QWidget>
#include <QElapsedTimer>

// Vulkan renderer host placeholder for Qt Radiant.
// Future work: wire the real engine renderer when RADIANT_USE_ENGINE_RENDERER_VK is enabled.
class VkViewportWidget final : public QWidget {
	Q_OBJECT
public:
	explicit VkViewportWidget(const QString& rendererPathHint = QString(), QWidget* parent = nullptr);

	bool loadRenderer(const QString& path);
	bool loadMap(const QString& path);
	QString status() const { return m_status; }
	QString rendererPath() const { return m_rendererPath; }

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;

private:
	QString locateDefaultRenderer() const;
	void updateCameraText();

	QString m_rendererPath;
	QString m_status;
	bool m_loaded{false};
	QString m_mapPath;

	// Simple camera state for orbit/fly preview
	QPoint m_lastPos;
	float m_yaw{0.0f};
	float m_pitch{20.0f};
	float m_distance{6.0f};
	float m_panX{0.0f};
	float m_panY{0.0f};
	QString m_cameraInfo;

	QElapsedTimer m_frameTimer;
	qint64 m_lastFrameMs{0};

#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	refexport_t* m_re{nullptr};
#endif
};
