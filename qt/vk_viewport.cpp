#include "vk_viewport.h"
#include "gizmo.h"
#include "clipping_tool.h"
#include "polygon_tool.h"
#include "vertex_tool.h"
#include "texture_paint_tool.h"
#include "extrusion_tool.h"

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
#include <QKeyEvent>
#include <QTimer>
#include <QSettings>
#include <QMatrix4x4>
#include <QResizeEvent>
#include <QShowEvent>
#include <cmath>
#include <algorithm>
#include <cstring>

#ifdef RADIANT_USE_ENGINE_RENDERER_VK
#include "tr_public.h" // ensure renderer headers are reachable; not yet invoked
#include "renderer_bridge.h"
#include "renderer_import.h"
#endif

// Static grid members
float VkViewportWidget::s_gridSize = 32.0f;
bool VkViewportWidget::s_microgridEnabled = false;

VkViewportWidget::VkViewportWidget(const QString &rendererPathHint, ViewType view, QWidget *parent)
	: QWidget(parent)
{
	// Set attributes for proper rendering
	setAttribute(Qt::WA_PaintOnScreen, false);
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setAttribute(Qt::WA_NativeWindow, true);
	
#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	// Ensure the native window exists so we can create a Vulkan surface later.
	// Note: winId() may return 0 until widget is shown, so we'll set it in showEvent
	SetVkSurfaceWindow( static_cast<quintptr>( winId() ) );
#endif

	m_viewType = view;
	switch (m_viewType) {
	case ViewType::Top:
		m_pitch = -89.0f;
		m_yaw = 0.0f;
		m_distance = 256.0f;
		break;
	case ViewType::Front:
		m_pitch = 0.0f;
		m_yaw = 0.0f;
		m_distance = 256.0f;
		break;
	case ViewType::Side:
		m_pitch = 0.0f;
		m_yaw = 90.0f;
		m_distance = 256.0f;
		break;
	case ViewType::Perspective:
	default:
		break;
	}

	m_frameTimer.start();
	
	// Load grid settings
	QSettings settings;
	s_gridSize = settings.value("gridSize", 32.0f).toFloat();
	s_microgridEnabled = settings.value("microgridEnabled", false).toBool();
	
	// Setup WASD camera movement timer
	m_cameraUpdateTimer = new QTimer(this);
	connect(m_cameraUpdateTimer, &QTimer::timeout, this, &VkViewportWidget::updateCameraMovement);
	m_cameraUpdateTimer->start(16); // ~60 FPS updates
	
	// Initialize gizmo
	m_gizmo = new Gizmo();
	m_gizmo->setPosition(m_selection);
	m_gizmo->setMode(GizmoMode::Box);
	m_gizmoMode = 1; // Box mode

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

	// Simple frame pump
	auto *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this](){ update(); });
	timer->start(16); // ~60 FPS
}

QString VkViewportWidget::locateDefaultRenderer() const
{
	// Try a few conventional spots relative to the app or repo layout.
	const QString appDir = QCoreApplication::applicationDirPath();
	QStringList candidates;

	// 1) Explicit env override
	const QByteArray envLib = qgetenv("ENGINE_RENDERER_VK_LIB");
	if ( !envLib.isEmpty() ) {
		candidates << QString::fromLocal8Bit(envLib);
	}

	// 2) Alongside the app
	candidates << appDir + QStringLiteral("/idtech3_vulkan_x86_64.so")
	           << appDir + QStringLiteral("/idtech3_vulkan.so");

	// 3) Typical build/release locations relative to install dir
	const QString repoRoot = QFileInfo(appDir + QStringLiteral("/../../..")).canonicalFilePath();
	if ( !repoRoot.isEmpty() ) {
		candidates << repoRoot + QStringLiteral("/build/idtech3_vulkan_x86_64.so")
		           << repoRoot + QStringLiteral("/release/idtech3_vulkan_x86_64.so")
		           << repoRoot + QStringLiteral("/build/idtech3_vulkan.so")
		           << repoRoot + QStringLiteral("/release/idtech3_vulkan.so");
	}

	// 4) Windows names for completeness
	candidates << appDir + QStringLiteral("/../idtech3_vulkan_x86_64.dll")
	           << appDir + QStringLiteral("/../idtech3_vulkan.dll");

	for (const QString &path : candidates) {
		if (QFile::exists(path)) {
			const QString canon = QFileInfo(path).canonicalFilePath();
			return canon.isEmpty() ? path : canon;
		}
	}
	return QString();
}

QString VkViewportWidget::viewName() const {
	switch (m_viewType) {
	case ViewType::Top: return QStringLiteral("Top");
	case ViewType::Front: return QStringLiteral("Front");
	case ViewType::Side: return QStringLiteral("Side");
	case ViewType::Perspective:
	default: return QStringLiteral("Perspective");
	}
}

bool VkViewportWidget::loadRenderer(const QString &path)
{
#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	static RendererBridge bridge;
	static refexport_t* sharedRe = nullptr;
	static QString sharedPath;

	QString resolved = path;
	if ( resolved.isEmpty() ) {
		resolved = locateDefaultRenderer();
	}

	if ( sharedRe && (!resolved.isEmpty() && resolved == sharedPath) ) {
		m_loaded = true;
		m_rendererPath = sharedPath;
		m_status = QStringLiteral("Renderer reused (shared)");
		m_re = sharedRe;
		update();
		return true;
	}

	if ( !bridge.load(resolved) ) {
		m_loaded = false;
		m_status = bridge.status();
		update();
		return false;
	}

	// Construct a minimal refimport to initialize the renderer.
	refimport_t import = MakeQtRefImport(QStringLiteral("/tmp/radiant_qt_renderer.log"));
	GetRefAPI_t getRefApi = bridge.getRefApi();
	if ( !getRefApi ) {
		m_loaded = false;
		m_status = QStringLiteral("GetRefAPI unresolved");
		update();
		return false;
	}

	refexport_t* re = getRefApi( REF_API_VERSION, &import );
	if ( !re ) {
		m_loaded = false;
		m_status = QStringLiteral("GetRefAPI returned null (init failed)");
		update();
		return false;
	}

	// Initialize renderer with current window if available
	// Note: winId() may be 0 if widget hasn't been shown yet
	// In that case, we'll set it in showEvent
	quintptr winIdValue = static_cast<quintptr>(winId());
	if (winIdValue != 0) {
		SetVkSurfaceWindow(winIdValue);
	}

	sharedRe = re;
	sharedPath = resolved;

	m_loaded = true;
	m_rendererPath = resolved;
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
	// If renderer is available, run a frame. Otherwise draw placeholder.
#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	if ( m_re ) {
		const int w = std::max(1, width());
		const int h = std::max(1, height());

		vec3_t forward{};
		vec3_t right{};
		vec3_t up{};
		vec3_t vieworg{};
		vec3_t target = { m_panX, m_panY, 0.0f };

		if ( m_viewType == ViewType::Perspective ) {
			// Build camera basis (orbit/freelook)
			const float yawRad   = m_yaw   * (float)M_PI / 180.0f;
			const float pitchRad = m_pitch * (float)M_PI / 180.0f;
			const float cy = std::cos(yawRad),  sy = std::sin(yawRad);
			const float cp = std::cos(pitchRad), sp = std::sin(pitchRad);

			forward[0] = cp * cy; forward[1] = cp * sy; forward[2] = -sp;
			right[0]   = -sy;     right[1]   = cy;      right[2]   = 0.0f;
			up[0]      = sp * cy; up[1]      = sp * sy; up[2]      = cp;

			vieworg[0] = target[0] - forward[0] * m_distance;
			vieworg[1] = target[1] - forward[1] * m_distance;
			vieworg[2] = target[2] - forward[2] * m_distance;
		} else if ( m_viewType == ViewType::Top ) {
			forward[0] = 0.0f; forward[1] = 0.0f; forward[2] = -1.0f;
			right[0]   = 1.0f; right[1]   = 0.0f; right[2]   = 0.0f;
			up[0]      = 0.0f; up[1]      = 1.0f; up[2]      = 0.0f;
			vieworg[0] = m_panX;
			vieworg[1] = m_panY;
			vieworg[2] = m_distance;
		} else if ( m_viewType == ViewType::Front ) {
			forward[0] = 0.0f; forward[1] = 1.0f; forward[2] = 0.0f;
			right[0]   = 1.0f; right[1]   = 0.0f; right[2]   = 0.0f;
			up[0]      = 0.0f; up[1]      = 0.0f; up[2]      = 1.0f;
			vieworg[0] = m_panX;
			vieworg[1] = -m_distance;
			vieworg[2] = m_panY;
		} else { // Side
			forward[0] = -1.0f; forward[1] = 0.0f; forward[2] = 0.0f;
			right[0]   = 0.0f;  right[1]   = 1.0f; right[2]   = 0.0f;
			up[0]      = 0.0f;  up[1]      = 0.0f; up[2]      = 1.0f;
			vieworg[0] = m_distance;
			vieworg[1] = m_panX;
			vieworg[2] = m_panY;
		}

		refdef_t rd{};
		rd.x = 0; 
		rd.y = 0; 
		rd.width = w; 
		rd.height = h;
		rd.fov_x = (m_viewType == ViewType::Perspective) ? 75.0f : 90.0f;
		rd.fov_y = rd.fov_x * (float)h / (float)w;
		VectorCopy(vieworg, rd.vieworg);
		VectorCopy(forward, rd.viewaxis[0]);
		VectorCopy(right,   rd.viewaxis[1]);
		VectorCopy(up,      rd.viewaxis[2]);
		rd.time = static_cast<int>( m_frameTimer.elapsed() );

		// Render frame with Vulkan renderer
		// Ensure viewport dimensions are valid
		if (w > 0 && h > 0) {
			m_re->BeginFrame(STEREO_CENTER);
			m_re->RenderScene(&rd);
			m_re->EndFrame(nullptr, nullptr);
		}
		return;
	}
#endif

	QPainter p(this);
	p.fillRect(rect(), QColor(26, 26, 26));

	// Draw grid with configurable size
	float gridStep = s_gridSize;
	if (gridStep < 1.0f) {
		gridStep = 32.0f; // Default fallback
	}
	
	p.setPen(QColor(60, 60, 60));
	for (int x = 0; x < width(); x += static_cast<int>(gridStep)) {
		p.drawLine(x, 0, x, height());
	}
	for (int y = 0; y < height(); y += static_cast<int>(gridStep)) {
		p.drawLine(0, y, width(), y);
	}

	// Draw gizmo if enabled
	if (m_gizmo && m_gizmoMode == 2 && m_hasSelection) { // 2 = Gizmo mode
		// Snap selection to grid if enabled
		float grid = s_gridSize;
		if (grid > 0.0f) {
			QVector3D snapped = m_selection;
			snapped.setX(std::round(snapped.x() / grid) * grid);
			snapped.setY(std::round(snapped.y() / grid) * grid);
			snapped.setZ(std::round(snapped.z() / grid) * grid);
			m_gizmo->setPosition(snapped);
		} else {
			m_gizmo->setPosition(m_selection);
		}
		
		// Build view and projection matrices
		QMatrix4x4 viewMatrix, projMatrix;
		buildViewMatrices(viewMatrix, projMatrix);
		
		m_gizmo->render(p, viewMatrix, projMatrix, width(), height());
	}
	
	// Draw selection indicators
	if (m_hasSelection) {
		// Draw all selected objects
		for (int i = 0; i < m_selectedObjects.size(); ++i) {
			const QVector3D& sel = m_selectedObjects[i];
			bool isPrimary = (sel - m_selection).lengthSquared() < 0.01f;
			
			// Convert 3D selection to 2D screen coordinates
			QPoint screenPos;
			switch (m_viewType) {
			case ViewType::Top:
				screenPos = QPoint(
					width()/2 + static_cast<int>((sel.x() - m_panX) * 256.0f / m_distance),
					height()/2 - static_cast<int>((sel.y() - m_panY) * 256.0f / m_distance)
				);
				break;
			case ViewType::Front:
				screenPos = QPoint(
					width()/2 + static_cast<int>((sel.x() - m_panX) * 256.0f / m_distance),
					height()/2 - static_cast<int>((sel.z() - m_panY) * 256.0f / m_distance)
				);
				break;
			case ViewType::Side:
				screenPos = QPoint(
					width()/2 + static_cast<int>((sel.y() - m_panX) * 256.0f / m_distance),
					height()/2 - static_cast<int>((sel.z() - m_panY) * 256.0f / m_distance)
				);
				break;
			case ViewType::Perspective:
			default:
				screenPos = QPoint(width()/2, height()/2); // Center for perspective
				break;
			}

			// Draw selection indicator (primary selection is brighter)
			QColor selColor = isPrimary ? QColor(255, 255, 0) : QColor(200, 200, 100);
			p.setPen(QPen(selColor, isPrimary ? 2 : 1));
			const int crossSize = isPrimary ? 10 : 6;
			p.drawLine(screenPos.x() - crossSize, screenPos.y(), screenPos.x() + crossSize, screenPos.y());
			p.drawLine(screenPos.x(), screenPos.y() - crossSize, screenPos.x(), screenPos.y() + crossSize);

			// Draw selection circle
			p.setPen(QPen(selColor, 1));
			p.setBrush(Qt::NoBrush);
			p.drawEllipse(screenPos, crossSize, crossSize);
		}
	}
	
	// Draw drag select rectangle
	if (m_dragSelecting && !m_dragSelectRect.isEmpty()) {
		p.setPen(QPen(QColor(100, 150, 255), 2));
		p.setBrush(QBrush(QColor(100, 150, 255, 30)));
		p.drawRect(m_dragSelectRect);
	}
	
	// Draw polygon tool if active
	if (m_polygonTool && (m_polygonTool->pointCount() > 0 || !m_polygonTool->currentPoint().isNull())) {
		QMatrix4x4 viewMatrix, projMatrix;
		buildViewMatrices(viewMatrix, projMatrix);
		m_polygonTool->render(p, viewMatrix, projMatrix, width(), height());
	}
	
	// Draw clipping tool if active
	if (m_clippingTool && m_clippingTool->pointCount() > 0) {
		QMatrix4x4 viewMatrix, projMatrix;
		buildViewMatrices(viewMatrix, projMatrix);
		m_clippingTool->render(p, viewMatrix, projMatrix, width(), height());
	}

	const QString title = m_loaded ? QStringLiteral("Vulkan/PBR Viewport (engine)") : QStringLiteral("Renderer not loaded");
	const QString detail = m_status.isEmpty() ? QStringLiteral("No status") : m_status;
	const QString cam = m_cameraInfo.isEmpty() ? QStringLiteral("Cam: yaw/pitch/dst/pan TBD") : m_cameraInfo;
	const QString view = QStringLiteral("View: %1").arg(viewName());
	const QString sel = m_hasSelection ? QStringLiteral("Selection: (%.1f, %.1f, %.1f)").arg(m_selection.x()).arg(m_selection.y()).arg(m_selection.z()) : QStringLiteral("No selection");

	p.setPen(Qt::white);
	p.drawText(rect().adjusted(12, 12, -12, -12), Qt::AlignLeft | Qt::AlignTop,
	           title + QStringLiteral("\n") + view + QStringLiteral("\n") + detail + QStringLiteral("\n") + cam + QStringLiteral("\n") + sel +
	           QStringLiteral("\n(WYSIWYG viewport stub; hook GetRefAPI next.)"));
}

bool VkViewportWidget::loadMap(const QString& path){
#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	if ( !m_re ) {
		m_status = QStringLiteral("Renderer not initialized");
		update();
		return false;
	}

	// Ensure window is created before loading map
	quintptr winIdValue = static_cast<quintptr>(winId());
	if (winIdValue == 0) {
		// Force window creation
		(void)winId();
		winIdValue = static_cast<quintptr>(winId());
		if (winIdValue != 0) {
			SetVkSurfaceWindow(winIdValue);
		}
	}

	glconfig_t cfg{};
	memset(&cfg, 0, sizeof(cfg));
	cfg.vidWidth = std::max(1, width());
	cfg.vidHeight = std::max(1, height());
	cfg.colorBits = 24;
	cfg.depthBits = 24;
	cfg.stencilBits = 8;
	cfg.displayFrequency = 60;
	cfg.isFullscreen = qfalse;
	cfg.stereoEnabled = qfalse;
	cfg.smpActive = qfalse;
	cfg.textureEnvAddAvailable = qtrue;
	cfg.driverType = GLDRV_ICD;
	cfg.hardwareType = GLHW_GENERIC;
	cfg.textureCompression = TC_NONE;
	cfg.deviceSupportsGamma = qfalse;

	m_re->BeginRegistration(&cfg);
	m_re->LoadWorld(path.toUtf8().constData());
	m_re->EndRegistration();

	m_mapPath = path;
	m_status = QStringLiteral("Loaded map: %1").arg(path);
	update();
	return true;
#else
	Q_UNUSED(path);
	m_mapPath = path;
	m_status = QStringLiteral("Map path set (renderer not available): %1").arg(path);
	update();
	return true;
#endif
}

void VkViewportWidget::mousePressEvent(QMouseEvent *event){
	m_lastPos = event->position().toPoint();
	
	// Handle right-click for gizmo operation cycling
	if (event->button() == Qt::RightButton && m_gizmoMode == 2 && m_hasSelection) { // 2 = Gizmo mode
		if (m_gizmo) {
			m_gizmo->cycleOperation();
			// Emit signal to update main window
			int op = static_cast<int>(m_gizmo->operation());
			Q_EMIT gizmoOperationChanged(op);
			update();
		}
		QWidget::mousePressEvent(event);
		return;
	}
	
	if ( event->button() == Qt::LeftButton ) {
		// Calculate 3D position from mouse click
		QVector3D clickPos = screenToWorld(event->pos());
		
		// Handle clipping tool
		if (m_clippingTool) {
			// Check if clicking on a handle
			QVector3D rayOrigin, rayDir;
			// Simplified ray calculation
			rayOrigin = clickPos;
			rayDir = QVector3D(0, 0, -1); // Simplified
			
			int handleIndex;
			float hitDistance;
			if (m_clippingTool->hitTestHandle(rayOrigin, rayDir, handleIndex, hitDistance)) {
				// Start dragging handle
				m_isDragging = true;
				m_dragStartPos = event->pos();
				update();
				return;
			}
			
			// Add new point (with validation)
			if (m_clippingTool->isThreePointMode()) {
				if (m_clippingTool->pointCount() < 3) {
					m_clippingTool->addPoint(clickPos);
				}
			} else {
				if (m_clippingTool->pointCount() < 2) {
					m_clippingTool->addPoint(clickPos);
				}
			}
			update();
			return;
		}
		
		// Handle polygon tool
		if (m_polygonTool) {
			// Only add points in 2D views (Top, Front, Side)
			if (m_viewType != ViewType::Perspective) {
				// Validate point count (reasonable limit)
				if (m_polygonTool->pointCount() < 64) {
					m_polygonTool->addPoint(clickPos);
					update();
				}
			}
			return;
		}
		
		// Handle vertex tool
		if (m_vertexTool) {
			// In full implementation, would:
			// 1. Raycast to find nearest vertex
			// 2. Select vertex (or add to selection if Ctrl pressed)
			// For now, just log
			if (event->modifiers() & Qt::ControlModifier) {
				// Multi-select - would add vertex to selection
			} else {
				// Single select - would select vertex
			}
			update();
			return;
		}
		
		// Handle texture paint tool
		if (m_texturePaintTool) {
			if (!m_texturePaintTool->isPainting()) {
				QString texture = m_currentMaterial.isEmpty() ? QStringLiteral("common/caulk") : m_currentMaterial;
				m_texturePaintTool->startPaint(clickPos, texture);
			} else {
				m_texturePaintTool->updatePaint(clickPos);
			}
			update();
			return;
		}
		
		// Handle extrusion tool
		if (m_extrusionTool) {
			// In full implementation, would:
			// 1. Raycast to find face under cursor
			// 2. Start extrusion preview
			// 3. Allow dragging to set extrusion distance
			update();
			return;
		}
		
		// Marquee selection: start drag selection if no object hit
		// or if Ctrl is held for additive selection
		bool startMarquee = false;

		if (event->modifiers() & Qt::ControlModifier) {
			// Ctrl+click always starts additive marquee selection
			startMarquee = true;
		} else {
			// Check if we clicked on an existing selection or object
			// For now, we'll start marquee selection after a short drag distance
			// In full implementation, would check if click hit any selectable object
			m_pendingMarqueeStart = true;
			m_dragStartPos = event->pos();
			m_dragStartTime = QDateTime::currentMSecsSinceEpoch();
			return; // Wait for mouse move to decide
		}

		if (startMarquee) {
			m_dragSelecting = true;
			m_dragStartPos = event->pos();
			m_dragSelectRect = QRect();
			QWidget::mousePressEvent(event);
			return;
		}

		// Standard single-click selection
		m_selection = clickPos;
		m_hasSelection = true;
		m_isDragging = true;
		m_dragStartPos = event->pos();
		m_dragStartSelection = m_selection;
		Q_EMIT selectionChanged(m_selection);
		update(); // Trigger repaint to show selection
	}
	QWidget::mousePressEvent(event);
}

void VkViewportWidget::mouseMoveEvent(QMouseEvent *event){
		const QPoint delta = event->pos() - m_lastPos;
	m_lastPos = event->pos();

	// ===== CAMERA CONTROLS =====
	// Alt + Left-click drag = Orbit (rotate around target)
	if ((event->modifiers() & Qt::AltModifier) && (event->buttons() & Qt::LeftButton) && m_viewType == ViewType::Perspective) {
		// Orbit camera around target
		m_yaw += delta.x() * 0.5f; // Horizontal rotation
		m_pitch -= delta.y() * 0.5f; // Vertical rotation (inverted)

		// Clamp pitch to avoid gimbal lock
		m_pitch = std::max(-89.0f, std::min(89.0f, m_pitch));

		update();
		return;
	}

	// Alt + Middle-click drag = Pan/Track (move camera position)
	if ((event->modifiers() & Qt::AltModifier) && (event->buttons() & Qt::MiddleButton) && m_viewType == ViewType::Perspective) {
		// Pan camera position
		float panSpeed = m_distance * 0.01f; // Scale pan speed with distance
		m_panX -= delta.x() * panSpeed;
		m_panY += delta.y() * panSpeed; // Inverted Y for natural feel

		update();
		return;
	}

	// Alt + Right-click drag = Dolly/Zoom (change distance)
	if ((event->modifiers() & Qt::AltModifier) && (event->buttons() & Qt::RightButton) && m_viewType == ViewType::Perspective) {
		// Dolly camera (zoom in/out)
		m_distance *= (1.0f + delta.y() * 0.01f);
		m_distance = std::max(0.1f, std::min(10000.0f, m_distance)); // Clamp distance

		update();
		return;
	}

	// Handle pending marquee start
	if (m_pendingMarqueeStart && (event->buttons() & Qt::LeftButton)) {
		QPoint dragDelta = event->pos() - m_dragStartPos;
		int dragDistance = std::abs(dragDelta.x()) + std::abs(dragDelta.y());

		// Start marquee selection if dragged far enough
		if (dragDistance > 5) { // 5 pixel threshold
			m_dragSelecting = true;
			m_pendingMarqueeStart = false;
			m_dragSelectRect = QRect();
		} else {
			// Still waiting for drag to begin
			return;
		}
	}

	// Handle drag select (marquee selection)
	if (m_dragSelecting && (event->buttons() & Qt::LeftButton)) {
		m_dragSelectRect = QRect(m_dragStartPos, event->pos()).normalized();
		update();
		return;
	}
	
	// Handle clipping tool handle dragging
	if (m_clippingTool && m_isDragging && (event->buttons() & Qt::LeftButton)) {
		QVector3D clickPos = screenToWorld(event->pos());
		
		// Snap to grid
		float grid = s_gridSize;
		if (grid > 0.0f) {
			clickPos.setX(std::round(clickPos.x() / grid) * grid);
			clickPos.setY(std::round(clickPos.y() / grid) * grid);
			clickPos.setZ(std::round(clickPos.z() / grid) * grid);
		}
		
		// Find which handle is being dragged (simplified - would use proper hit test)
		// For now, update the last point
		if (m_clippingTool->pointCount() > 0) {
			int lastIndex = m_clippingTool->pointCount() - 1;
			m_clippingTool->moveHandle(lastIndex, clickPos);
			update();
		}
		return;
	}
	
		// Handle polygon tool preview
	if (m_polygonTool && (event->buttons() & Qt::NoButton)) {
		QVector3D hoverPos;
		if (m_viewType != ViewType::Perspective) {
			hoverPos = screenToWorld(event->pos());
		}
		
		if (!hoverPos.isNull()) {
			m_polygonTool->setCurrentPoint(hoverPos);
			update();
		}
	}
	
	// Handle texture paint tool dragging
	if (m_texturePaintTool && m_texturePaintTool->isPainting() && (event->buttons() & Qt::LeftButton)) {
		QVector3D paintPos = screenToWorld(event->pos());
		m_texturePaintTool->updatePaint(paintPos);
		update();
	}
	
	// Handle extrusion tool preview
	if (m_extrusionTool && m_extrusionToolActive) {
		// In full implementation, would show extrusion preview
		// For now, just update viewport
		update();
	}
	
		// Handle light power adjustment (Alt+drag on light entity)
	if ((event->modifiers() & Qt::AltModifier) && m_hasSelection && (event->buttons() & Qt::LeftButton)) {
		// Calculate drag delta
                QPoint dragDelta = event->pos() - m_dragStartPos;
		float powerDelta = -dragDelta.y() * 0.1f; // Negative because dragging up should increase
		Q_EMIT lightPowerChanged(powerDelta);
		m_dragStartPos = event->pos(); // Update for next frame
		update();
		return;
	}
	
	// Handle gizmo manipulation
	if (m_gizmoMode == 2 && m_gizmo && m_hasSelection && (event->buttons() & Qt::LeftButton)) { // 2 = Gizmo mode
		// Build ray from mouse position
		QPoint mousePos = event->pos();
		QVector3D rayOrigin, rayDir;
		
		// Calculate ray based on viewport type
		if (m_viewType == ViewType::Top) {
			QVector3D worldPos = screenToWorld(mousePos);
			rayOrigin = QVector3D(worldPos.x(), worldPos.y(), m_distance);
			rayDir = QVector3D(0.0f, 0.0f, -1.0f);
		} else if (m_viewType == ViewType::Front) {
			QVector3D worldPos = screenToWorld(mousePos);
			rayOrigin = QVector3D(worldPos.x(), -m_distance, worldPos.z());
			rayDir = QVector3D(0.0f, 1.0f, 0.0f);
		} else if (m_viewType == ViewType::Side) {
			QVector3D worldPos = screenToWorld(mousePos);
			rayOrigin = QVector3D(m_distance, worldPos.y(), worldPos.z());
			rayDir = QVector3D(-1.0f, 0.0f, 0.0f);
		} else {
			// Perspective view - simplified ray
			rayOrigin = screenToWorld(mousePos);
			rayDir = QVector3D(0.0f, 0.0f, -1.0f);
		}
		
		// Test gizmo hit
		int hitAxis;
		float hitDistance;
		if (m_gizmo->hitTest(rayOrigin, rayDir, hitAxis, hitDistance)) {
			// Manipulate selection along hit axis
			QVector3D newPos = m_gizmo->projectToAxis(rayOrigin + rayDir * hitDistance, hitAxis);
			
			// Snap to grid
			float grid = s_gridSize;
			if (grid > 0.0f) {
				newPos.setX(std::round(newPos.x() / grid) * grid);
				newPos.setY(std::round(newPos.y() / grid) * grid);
				newPos.setZ(std::round(newPos.z() / grid) * grid);
			}
			
			m_selection = newPos;
			m_gizmo->setPosition(newPos);
			Q_EMIT selectionChanged(m_selection);
			update();
		}
	}

	if ( m_viewType == ViewType::Perspective ) {
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
	} else {
		// Ortho: left/middle both pan
		if ( event->buttons() & (Qt::LeftButton | Qt::MiddleButton) ) {
			const float scale = 0.5f * std::max(1.0f, m_distance / 128.0f);
			m_panX -= delta.x() * scale;
			m_panY += delta.y() * scale;
			updateCameraText();
			update();
		}
	}

	if ( m_hasSelection && (event->buttons() & Qt::RightButton) ) {
		// Drag selection in screen plane
		const float moveScale = 0.1f * std::max(1.0f, m_distance / 64.0f);
		m_selection.setX( m_selection.x() - delta.x() * moveScale );
		m_selection.setY( m_selection.y() + delta.y() * moveScale );
		Q_EMIT selectionChanged(m_selection);
		update();
	}
	QWidget::mouseMoveEvent(event);
}

void VkViewportWidget::mouseReleaseEvent(QMouseEvent *event){
	if ( event->button() == Qt::LeftButton ) {
		m_isDragging = false;

		// Handle pending marquee start (user clicked without dragging far enough)
		if (m_pendingMarqueeStart) {
			m_pendingMarqueeStart = false;

			// Do a regular selection at the click position
			QVector3D clickPos = screenToWorld(event->pos());
			m_selection = clickPos;
			m_hasSelection = true;
			Q_EMIT selectionChanged(m_selection);
			update();
			return;
		}
		
		// Handle texture paint tool end
		if (m_texturePaintTool && m_texturePaintTool->isPainting()) {
			m_texturePaintTool->endPaint();
			update();
		}
		
		if (m_dragSelecting) {
			// Select all objects within drag rectangle
			if (!m_dragSelectRect.isEmpty()) {
				// In full implementation, would:
				// 1. Project drag rectangle to 3D space
				// 2. Find all objects/brushes within the rectangle
				// 3. Add them to selection
				
				// For now, add selection at center of drag rectangle
				QPoint center = m_dragSelectRect.center();
				QVector3D newSelection;
				
				switch (m_viewType) {
				case ViewType::Top:
					newSelection = QVector3D(
						m_panX + (center.x() - width()/2.0f) * m_distance / 256.0f,
						m_panY + (height()/2.0f - center.y()) * m_distance / 256.0f,
						0.0f
					);
					break;
				case ViewType::Front:
					newSelection = QVector3D(
						m_panX + (center.x() - width()/2.0f) * m_distance / 256.0f,
						m_distance,
						m_panY + (height()/2.0f - center.y()) * m_distance / 256.0f
					);
					break;
				case ViewType::Side:
					newSelection = QVector3D(
						m_distance,
						m_panX + (center.x() - width()/2.0f) * m_distance / 256.0f,
						m_panY + (height()/2.0f - center.y()) * m_distance / 256.0f
					);
					break;
				case ViewType::Perspective:
				default:
					newSelection = QVector3D(m_panX, m_panY, m_distance * 0.5f);
					break;
				}
				
				addToSelection(newSelection);
			}
			
			m_dragSelecting = false;
			m_dragSelectRect = QRect();
			update();
		}
	}
	QWidget::mouseReleaseEvent(event);
}

void VkViewportWidget::clearSelection() {
	m_hasSelection = false;
	m_selection = QVector3D();
	m_selectedObjects.clear();
	Q_EMIT selectionChanged(m_selection);
	update();
}

void VkViewportWidget::addToSelection(const QVector3D& pos) {
	// Check if already selected
	for (const QVector3D& existing : m_selectedObjects) {
		if ((existing - pos).lengthSquared() < 0.01f) {
			return; // Already selected
		}
	}
	
	m_selectedObjects.append(pos);
	m_hasSelection = true;
	m_selection = pos; // Set as primary selection
	Q_EMIT selectionChanged(pos);
	update();
}

void VkViewportWidget::keyPressEvent(QKeyEvent* event) {
	// Handle Tab - focus on selection
	if (event->key() == Qt::Key_Tab && !event->modifiers()) {
		focusOnSelection();
		update();
		return;
	}

	// Handle Home key - reset camera
	if (event->key() == Qt::Key_Home && !event->modifiers()) {
		resetCamera();
		event->accept();
		return;
	}
	
	// Handle WASD camera movement (only in perspective view)
	if (m_viewType == ViewType::Perspective) {
		if (event->key() == Qt::Key_W) {
			m_cameraKeys[0] = true; // Forward
			event->accept();
			return;
		} else if (event->key() == Qt::Key_A) {
			m_cameraKeys[1] = true; // Left
			event->accept();
			return;
		} else if (event->key() == Qt::Key_S) {
			m_cameraKeys[2] = true; // Backward
			event->accept();
			return;
		} else if (event->key() == Qt::Key_D) {
			m_cameraKeys[3] = true; // Right
			event->accept();
			return;
		} else if (event->key() == Qt::Key_Shift) {
			m_cameraShift = true;
			event->accept();
			return;
		}
	}
	
	// Handle axis locking (X, Y, Z keys) - only when not in WASD mode
	if (m_gizmo && m_gizmoMode == 2 && !m_cameraKeys[0] && !m_cameraKeys[1] && !m_cameraKeys[2] && !m_cameraKeys[3]) {
		if (event->key() == Qt::Key_X && !event->modifiers()) {
			bool locked = m_gizmo->isAxisLocked(0);
			m_gizmo->setLockedAxes(!locked, m_gizmo->isAxisLocked(1), m_gizmo->isAxisLocked(2));
			update();
			return;
		} else if (event->key() == Qt::Key_Y && !event->modifiers()) {
			bool locked = m_gizmo->isAxisLocked(1);
			m_gizmo->setLockedAxes(m_gizmo->isAxisLocked(0), !locked, m_gizmo->isAxisLocked(2));
			update();
			return;
		} else if (event->key() == Qt::Key_Z && !event->modifiers()) {
			bool locked = m_gizmo->isAxisLocked(2);
			m_gizmo->setLockedAxes(m_gizmo->isAxisLocked(0), m_gizmo->isAxisLocked(1), !locked);
			update();
			return;
		}
	}
	
	// Handle Ctrl for drag select
	if (event->key() == Qt::Key_Control) {
		// Ctrl key pressed - will enable drag select on next mouse press
	}
	QWidget::keyPressEvent(event);
}

void VkViewportWidget::keyReleaseEvent(QKeyEvent* event) {
	// Handle WASD camera movement release
	if (m_viewType == ViewType::Perspective) {
		if (event->key() == Qt::Key_W) {
			m_cameraKeys[0] = false;
			event->accept();
			return;
		} else if (event->key() == Qt::Key_A) {
			m_cameraKeys[1] = false;
			event->accept();
			return;
		} else if (event->key() == Qt::Key_S) {
			m_cameraKeys[2] = false;
			event->accept();
			return;
		} else if (event->key() == Qt::Key_D) {
			m_cameraKeys[3] = false;
			event->accept();
			return;
		} else if (event->key() == Qt::Key_Shift) {
			m_cameraShift = false;
			event->accept();
			return;
		}
	}
	QWidget::keyReleaseEvent(event);
}

void VkViewportWidget::updateCameraMovement() {
	if (m_viewType != ViewType::Perspective) return;
	if (!m_cameraKeys[0] && !m_cameraKeys[1] && !m_cameraKeys[2] && !m_cameraKeys[3]) return;
	
	// Calculate movement speed
	float moveSpeed = 0.1f * m_distance * 0.01f; // Scale with distance
	if (m_cameraShift) {
		moveSpeed *= 3.0f; // Faster with Shift
	}
	
	// Calculate forward/right vectors
	const float yawRad = m_yaw * (float)M_PI / 180.0f;
	const float pitchRad = m_pitch * (float)M_PI / 180.0f;
	const float cy = std::cos(yawRad), sy = std::sin(yawRad);
	const float cp = std::cos(pitchRad), sp = std::sin(pitchRad);
	
	QVector3D forward(cp * cy, cp * sy, -sp);
	QVector3D right(-sy, cy, 0.0f);
	
	// Apply movement
	QVector3D movement(0.0f, 0.0f, 0.0f);
	if (m_cameraKeys[0]) movement += forward * moveSpeed; // W - forward
	if (m_cameraKeys[2]) movement -= forward * moveSpeed; // S - backward
	if (m_cameraKeys[1]) movement -= right * moveSpeed; // A - left
	if (m_cameraKeys[3]) movement += right * moveSpeed; // D - right
	
	m_panX += movement.x();
	m_panY += movement.y();
	
	updateCameraText();
	update();
}

void VkViewportWidget::focusOnSelection() {
	if (!m_hasSelection) return;
	
	// Focus camera on selected object
	m_panX = m_selection.x();
	m_panY = m_selection.y();
	
	// Adjust distance to show object clearly
	if (m_viewType == ViewType::Perspective) {
		m_distance = 128.0f; // Default focus distance
	}
	
	updateCameraText();
}

void VkViewportWidget::wheelEvent(QWheelEvent *event){
	const float steps = event->angleDelta().y() / 120.0f;

	// Smooth zooming
	if ( m_viewType == ViewType::Perspective ) {
		float zoomFactor = 1.0f - steps * 0.1f; // More subtle zoom
		m_distance *= zoomFactor;
		m_distance = std::max(0.1f, std::min(10000.0f, m_distance)); // Reasonable limits
	} else {
		// Orthographic zoom
		const float zoomStep = std::max(1.0f, m_distance * 0.1f);
		m_distance = std::max(8.0f, m_distance - steps * zoomStep);
	}

	updateCameraText();
	update();
	QWidget::wheelEvent(event);
}

void VkViewportWidget::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	
#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	if (m_re) {
		// Notify renderer of viewport size change
		// The renderer should handle resize internally via the Vulkan surface
		// Force a repaint to update the viewport with new size
		update();
	}
#endif
}

void VkViewportWidget::showEvent(QShowEvent* event) {
	QWidget::showEvent(event);
	
#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	// Ensure window ID is set when widget becomes visible
	// This is critical for Vulkan surface creation
	quintptr winIdValue = static_cast<quintptr>(winId());
	if (winIdValue != 0) {
		SetVkSurfaceWindow(winIdValue);
		
		// If renderer is loaded but wasn't initialized with a window before,
		// we may need to reload the renderer now that window is available
		if (m_re && !m_loaded && !m_rendererPath.isEmpty()) {
			// Try to reload renderer now that window is available
			loadRenderer(m_rendererPath);
		}
	}
#endif
}

void VkViewportWidget::updateCameraText(){
	m_cameraInfo = QStringLiteral("Cam yaw=%1 pitch=%2 dist=%3 pan=(%4,%5)")
		.arg(m_yaw,      0, 'f', 1)
		.arg(m_pitch,    0, 'f', 1)
		.arg(m_distance, 0, 'f', 2)
		.arg(m_panX,     0, 'f', 2)
		.arg(m_panY,     0, 'f', 2);
}

void VkViewportWidget::buildViewMatrices(QMatrix4x4& viewMatrix, QMatrix4x4& projMatrix) const {
	QVector3D eye, center, up;
	
	if (m_viewType == ViewType::Perspective) {
		const float yawRad = m_yaw * (float)M_PI / 180.0f;
		const float pitchRad = m_pitch * (float)M_PI / 180.0f;
		const float cy = std::cos(yawRad), sy = std::sin(yawRad);
		const float cp = std::cos(pitchRad), sp = std::sin(pitchRad);
		
		QVector3D forward(cp * cy, cp * sy, -sp);
		QVector3D right(-sy, cy, 0.0f);
		QVector3D upVec(sp * cy, sp * sy, cp);
		
		eye = QVector3D(m_panX, m_panY, 0.0f) - forward * m_distance;
		center = QVector3D(m_panX, m_panY, 0.0f);
		up = upVec;
	} else if (m_viewType == ViewType::Top) {
		eye = QVector3D(m_panX, m_panY, m_distance);
		center = QVector3D(m_panX, m_panY, 0.0f);
		up = QVector3D(0.0f, 1.0f, 0.0f);
	} else if (m_viewType == ViewType::Front) {
		eye = QVector3D(m_panX, -m_distance, m_panY);
		center = QVector3D(m_panX, 0.0f, m_panY);
		up = QVector3D(0.0f, 0.0f, 1.0f);
	} else { // Side
		eye = QVector3D(m_distance, m_panX, m_panY);
		center = QVector3D(0.0f, m_panX, m_panY);
		up = QVector3D(0.0f, 0.0f, 1.0f);
	}
	
	viewMatrix.lookAt(eye, center, up);
	
	const float fov = (m_viewType == ViewType::Perspective) ? 75.0f : 90.0f;
	const float aspect = static_cast<float>(width()) / static_cast<float>(height());
	const float nearPlane = 0.1f;
	const float farPlane = 10000.0f;
	
	if (m_viewType == ViewType::Perspective) {
		projMatrix.perspective(fov, aspect, nearPlane, farPlane);
	} else {
		const float orthoSize = m_distance * 0.5f;
		projMatrix.ortho(-orthoSize * aspect, orthoSize * aspect,
		                -orthoSize, orthoSize,
		                nearPlane, farPlane);
	}
}

QVector3D VkViewportWidget::screenToWorld(const QPoint& screenPos) const {
	QVector3D worldPos;
	
	switch (m_viewType) {
	case ViewType::Top:
		worldPos = QVector3D(
			m_panX + (screenPos.x() - width()/2.0f) * m_distance / 256.0f,
			m_panY + (height()/2.0f - screenPos.y()) * m_distance / 256.0f,
			0.0f
		);
		break;
	case ViewType::Front:
		worldPos = QVector3D(
			m_panX + (screenPos.x() - width()/2.0f) * m_distance / 256.0f,
			m_distance,
			m_panY + (height()/2.0f - screenPos.y()) * m_distance / 256.0f
		);
		break;
	case ViewType::Side:
		worldPos = QVector3D(
			m_distance,
			m_panX + (screenPos.x() - width()/2.0f) * m_distance / 256.0f,
			m_panY + (height()/2.0f - screenPos.y()) * m_distance / 256.0f
		);
		break;
	case ViewType::Perspective:
	default:
		// For perspective view, use a simple depth-based selection
		worldPos = QVector3D(m_panX, m_panY, m_distance * 0.5f);
		break;
	}
	
	return worldPos;
}

void VkViewportWidget::setGridSize(float size) {
	if (size < 0.125f) size = 0.125f; // Minimum
	if (size > 1024.0f) size = 1024.0f; // Maximum
	s_gridSize = size;
	
	QSettings settings;
	settings.setValue("gridSize", size);
}

void VkViewportWidget::setMicrogridEnabled(bool enabled) {
	s_microgridEnabled = enabled;
	
	QSettings settings;
	settings.setValue("microgridEnabled", enabled);
	
	// If disabled, round grid size to nearest power of 2
	if (!enabled && s_gridSize < 1.0f) {
		s_gridSize = 1.0f;
		settings.setValue("gridSize", s_gridSize);
	}
}

void VkViewportWidget::setGizmoMode(int mode) {
	m_gizmoMode = mode;
	if (m_gizmo) {
		m_gizmo->setMode(static_cast<GizmoMode>(mode));
	}
	update();
}

void VkViewportWidget::setGizmoOperation(int op) {
	if (m_gizmo) {
		m_gizmo->setOperation(static_cast<GizmoOperation>(op));
		update();
	}
}

void VkViewportWidget::toggleGizmoSpace() {
	if (m_gizmo) {
		m_gizmo->toggleSpace();
		update();
	}
}

void VkViewportWidget::resetCamera() {
	// Reset camera to default perspective view
	m_yaw = 0.0f;
	m_pitch = 20.0f;
	m_distance = 6.0f;
	m_panX = 0.0f;
	m_panY = 0.0f;
	update();
}
