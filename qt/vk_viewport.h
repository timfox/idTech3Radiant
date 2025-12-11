#pragma once

#include <QString>
#include <QWidget>

// Simple Vulkan renderer host placeholder for Qt Radiant.
// Loads the engine's Vulkan renderer shared library and shows status text.
// Future work: wire GetRefAPI and drive the renderer with a Radiant scene graph.
class VkViewportWidget final : public QWidget
{
public:
	explicit VkViewportWidget(const QString &rendererPathHint = QString(), QWidget *parent = nullptr);

	// Attempt to load the renderer shared library. Returns true on success.
	bool loadRenderer(const QString &path);

	QString status() const { return m_status; }
	QString rendererPath() const { return m_rendererPath; }

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QString locateDefaultRenderer() const;
	QString m_rendererPath;
	QString m_status;
	bool m_loaded{false};
};
