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
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>

#ifdef RADIANT_USE_ENGINE_RENDERER_VK
#include "tr_public.h" // ensure renderer headers are reachable; not yet invoked
#include "renderer_bridge.h"
#include "renderer_import.h"
#endif

VkViewportWidget::VkViewportWidget(const QString &rendererPathHint, QWidget *parent)
	: QWidget(parent)
{
#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	// Ensure the native window exists so we can create a Vulkan surface later.
	(void)winId();
	SetVkSurfaceWindow( static_cast<quintptr>( winId() ) );
#endif

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
#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	static RendererBridge bridge;
	if ( !bridge.load(path) ) {
		m_loaded = false;
		m_status = bridge.status();
		update();
		return false;
	}

	// Construct a minimal refimport to initialize the renderer.
	refimport_t ri = MakeQtRefImport(QStringLiteral("/tmp/radiant_qt_renderer.log"));
	GetRefAPI_t getRefApi = bridge.getRefApi();
	if ( !getRefApi ) {
		m_loaded = false;
		m_status = QStringLiteral("GetRefAPI unresolved");
		update();
		return false;
	}

	refexport_t* re = getRefApi( REF_API_VERSION, &ri );
	if ( !re ) {
		m_loaded = false;
		m_status = QStringLiteral("GetRefAPI returned null (init failed)");
		update();
		return false;
	}

	m_loaded = true;
	m_rendererPath = path;
	m_status = QStringLiteral("Renderer initialized via GetRefAPI");
	m_re = re;
	update();
	return true;
#else
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
#endif
}

void VkViewportWidget::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QPainter p(this);
	p.fillRect(rect(), QColor(26, 26, 26));

	// Draw a simple grid
	const int gridStep = 32;
	p.setPen(QColor(60, 60, 60));
	for (int x = 0; x < width(); x += gridStep) {
		p.drawLine(x, 0, x, height());
	}
	for (int y = 0; y < height(); y += gridStep) {
		p.drawLine(0, y, width(), y);
	}

	const QString title = m_loaded ? QStringLiteral("Vulkan/PBR Viewport (engine)") : QStringLiteral("Renderer not loaded");
	const QString detail = m_status.isEmpty() ? QStringLiteral("No status") : m_status;
	const QString cam = m_cameraInfo.isEmpty() ? QStringLiteral("Cam: yaw/pitch/dst/pan TBD") : m_cameraInfo;

	p.setPen(Qt::white);
	p.drawText(rect().adjusted(12, 12, -12, -12), Qt::AlignLeft | Qt::AlignTop,
	           title + QStringLiteral("\n") + detail + QStringLiteral("\n") + cam +
	           QStringLiteral("\n(WYSIWYG viewport stub; hook GetRefAPI next.)"));
}

bool VkViewportWidget::loadMap(const QString& path){
#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	if ( !m_re ) {
		m_status = QStringLiteral("Renderer not initialized");
		update();
		return false;
	}

	glconfig_t cfg{};
	m_re->BeginRegistration(&cfg);
	m_re->LoadWorld(path.toUtf8().constData());
	m_re->EndRegistration();

	m_mapPath = path;
	m_status = QStringLiteral("Loaded map: %1").arg(path);
	update();
	return true;
#else
	Q_UNUSED(path);
	return false;
#endif
}

void VkViewportWidget::mousePressEvent(QMouseEvent *event){
	m_lastPos = event->pos();
	QWidget::mousePressEvent(event);
}

void VkViewportWidget::mouseMoveEvent(QMouseEvent *event){
	const QPoint delta = event->pos() - m_lastPos;
	m_lastPos = event->pos();

	if ( event->buttons() & Qt::LeftButton ) {
		m_yaw   += delta.x() * 0.25f;
		m_pitch += delta.y() * 0.25f;
		m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
		updateCameraText();
		update();
	} else if ( event->buttons() & Qt::MiddleButton ) {
		m_panX += delta.x() * 0.01f;
		m_panY += delta.y() * 0.01f;
		updateCameraText();
		update();
	}
	QWidget::mouseMoveEvent(event);
}

void VkViewportWidget::wheelEvent(QWheelEvent *event){
	const float steps = event->angleDelta().y() / 120.0f;
	m_distance = std::max(0.5f, m_distance - steps * 0.5f);
	updateCameraText();
	update();
	QWidget::wheelEvent(event);
}

void VkViewportWidget::updateCameraText(){
	m_cameraInfo = QStringLiteral("Cam yaw=%1 pitch=%2 dist=%3 pan=(%4,%5)")
		.arg(m_yaw,      0, 'f', 1)
		.arg(m_pitch,    0, 'f', 1)
		.arg(m_distance, 0, 'f', 2)
		.arg(m_panX,     0, 'f', 2)
		.arg(m_panY,     0, 'f', 2);
}
