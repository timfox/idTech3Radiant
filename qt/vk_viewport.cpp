#include "vk_viewport.h"

#include <QCoreApplication>
#include <QLibrary>
#include <QPainter>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QColor>
#include <QFileInfo>

#ifdef RADIANT_USE_ENGINE_RENDERER_VK
#include "tr_public.h" // ensure renderer headers are reachable; not yet invoked
#endif

VkViewportWidget::VkViewportWidget(const QString &rendererPathHint, QWidget *parent)
	: QWidget(parent)
{
	QString candidate = rendererPathHint;
	if (candidate.isEmpty()) {
		candidate = locateDefaultRenderer();
	}
	if (!candidate.isEmpty()) {
		loadRenderer(candidate);
	} else {
		m_status = QStringLiteral("No renderer path hint provided; not loaded");
	}
	setMinimumSize(640, 360);
}

QString VkViewportWidget::locateDefaultRenderer() const
{
	// Try a few conventional spots relative to the app or repo layout.
	const QString appDir = QCoreApplication::applicationDirPath();
	const QStringList candidates = {
		appDir + QStringLiteral("/idtech3_vulkan_x86_64.so"),
		appDir + QStringLiteral("/idtech3_vulkan.so"),
		appDir + QStringLiteral("/../build/idtech3_vulkan_x86_64.so"),
		appDir + QStringLiteral("/../build/idtech3_vulkan.so"),
		appDir + QStringLiteral("/../idtech3_vulkan_x86_64.dll"),
		appDir + QStringLiteral("/../idtech3_vulkan.dll")
	};

	for (const QString &path : candidates) {
		if (QFile::exists(path)) {
			const QString canon = QFileInfo(path).canonicalFilePath();
			return canon.isEmpty() ? path : canon;
		}
	}
	return QString();
}

bool VkViewportWidget::loadRenderer(const QString &path)
{
	m_rendererPath = path;
	QLibrary lib(path);
	lib.setLoadHints(QLibrary::ResolveAllSymbolsHint);

	if (!lib.load()) {
		m_loaded = false;
		m_status = QStringLiteral("Failed to load renderer: %1").arg(lib.errorString());
		update();
		return false;
	}

	// In a future step, resolve GetRefAPI and initialize with a Radiant-owned refimport_t.
	m_loaded = true;
	const QString canon = QFileInfo(path).canonicalFilePath();
	m_status = QStringLiteral("Renderer loaded: %1").arg(canon.isEmpty() ? path : canon);
	update();
	return true;
}

void VkViewportWidget::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QPainter p(this);
	p.fillRect(rect(), QColor(18, 18, 18));

	const QString title = m_loaded ? QStringLiteral("Vulkan/PBR Viewport (engine)") : QStringLiteral("Renderer not loaded");
	const QString detail = m_status.isEmpty() ? QStringLiteral("No status") : m_status;

	p.setPen(Qt::white);
	p.drawText(rect().adjusted(12, 12, -12, -12), Qt::AlignLeft | Qt::AlignTop,
	           title + QStringLiteral("\n") + detail + QStringLiteral("\n\n(WYSIWYG viewport stub; hook GetRefAPI next.)"));
}
