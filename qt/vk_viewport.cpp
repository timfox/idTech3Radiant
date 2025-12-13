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
#include <QFontMetrics>
#include <QRadialGradient>
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

	// Initialize bookmarks
	for (int i = 0; i < 10; ++i) {
		m_bookmarks[i].valid = false;
	}

	// Initialize FPS monitoring
	m_fpsTimer.start();

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
	p.setRenderHint(QPainter::Antialiasing, true); // Enable anti-aliasing for smoother rendering

	// Choose background color based on wireframe mode
	QColor bgColor = m_wireframeMode ? QColor(20, 20, 25) : QColor(32, 32, 32);
	p.fillRect(rect(), bgColor);

	// Update FPS counter
	updateFPS();

	// Draw world axis indicators (X=red, Y=green, Z=blue)
	if (m_showAxes) {
		drawWorldAxes(p);
	}

	// Draw improved grid with configurable size
	if (m_showGrid) {
		drawEnhancedGrid(p);
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
	
	// Draw enhanced selection indicators
	drawSelectionIndicators(p);

	// Draw camera frustum in orthographic views (perspective camera visualization)
	if (m_viewType != ViewType::Perspective) {
		drawCameraFrustum(p);
	}

	// Draw measurement overlay if active
	if (m_showMeasurements) {
		drawMeasurementOverlay(p);
	}

	// Draw navigation minimap in corner
	if (m_showMinimap) {
		drawMinimap(p);
	}

	// Draw performance warnings if FPS is low
	if (m_currentFPS < 30.0f && m_showPerformanceWarnings) {
		drawPerformanceWarning(p);
	}

	// Draw measurements
	if (!m_measurements.isEmpty()) {
		drawMeasurements(p);
	}

	// Draw crosshair
	if (m_showCrosshair) {
		drawCrosshair(p);
	}

	// Draw coordinate display
	if (m_showCoordinates) {
		drawCoordinateDisplay(p);
	}

	// Draw viewport information (FPS, stats, etc.)
	drawViewportInfo(p);
	
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

	// Draw enhanced viewport information
	if (m_showStats) {
		drawViewportInfo(p);
	}

	// Draw FPS counter if enabled
	if (m_showFPS) {
		drawFPSCounter(p);
	}

	// Draw coordinate display if mouse is over viewport
	if (rect().contains(mapFromGlobal(QCursor::pos()))) {
		drawCoordinateDisplay(p);
	}
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

		// Handle measurement tool
		if (m_showMeasurements && (event->modifiers() & Qt::ControlModifier)) {
			if (m_measurementStart.isNull()) {
				// Start new measurement
				m_measurementStart = clickPos;
				if (m_console) {
					m_console->appendPlainText(QStringLiteral("Measurement started at (%1, %2, %3)")
						.arg(clickPos.x(), 0, 'f', 2)
						.arg(clickPos.y(), 0, 'f', 2)
						.arg(clickPos.z(), 0, 'f', 2));
				}
			} else {
				// Complete measurement
				addMeasurement(m_measurementStart, clickPos);
				m_measurementStart = QVector3D(); // Reset
				if (m_console) {
					float distance = (clickPos - m_measurementStart).length();
					m_console->appendPlainText(QStringLiteral("Measurement completed: %1 units")
						.arg(distance, 0, 'f', 3));
				}
			}
			update();
			return;
		}

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

		// Check for measurement creation (Ctrl+click for measurement points)
		if (m_showMeasurements && (event->modifiers() & Qt::ControlModifier)) {
			if (m_measurementStart.isNull()) {
				// First measurement point
				m_measurementStart = clickPos;
				if (m_console) {
					m_console->appendPlainText(QStringLiteral("Measurement start: (%.2f, %.2f, %.2f)")
						.arg(clickPos.x()).arg(clickPos.y()).arg(clickPos.z()));
				}
			} else {
				// Second measurement point - create measurement
				addMeasurement(m_measurementStart, clickPos);
				m_measurementStart = QVector3D(); // Reset
				if (m_console) {
					m_console->appendPlainText(QStringLiteral("Measurement created: %.2f units")
						.arg((clickPos - m_measurementStart).length()));
				}
			}
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

QPoint VkViewportWidget::worldToScreen(const QVector3D& worldPos) const {
	QMatrix4x4 viewMatrix, projMatrix;
	buildViewMatrices(viewMatrix, projMatrix);

	// Transform world position to clip space
	QVector4D worldPos4(worldPos.x(), worldPos.y(), worldPos.z(), 1.0f);
	QVector4D viewPos = viewMatrix * worldPos4;
	QVector4D clipPos = projMatrix * viewPos;

	if (clipPos.w() <= 0.0f) {
		return QPoint(-1000, -1000); // Off-screen
	}

	float invW = 1.0f / clipPos.w();
	QPoint screenPos(
		static_cast<int>((clipPos.x() * invW + 1.0f) * 0.5f * width()),
		static_cast<int>((1.0f - clipPos.y() * invW) * 0.5f * height())
	);

	return screenPos;
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

void VkViewportWidget::updateFPS() {
	m_frameCount++;

	// Update FPS every 500ms for smooth display
	if (m_fpsTimer.elapsed() >= 500) {
		float elapsedSeconds = m_fpsTimer.elapsed() / 1000.0f;
		m_currentFPS = m_frameCount / elapsedSeconds;

		// Reset counters
		m_frameCount = 0;
		m_fpsTimer.restart();
	}
}

// ===== ENHANCED VIEWPORT RENDERING =====

void VkViewportWidget::drawWorldAxes(QPainter& painter) {
	// Draw world coordinate system axes in the corner
	const int axisLength = 40;
	const int axisThickness = 3;
	const int cornerMargin = 20;

	// Bottom-left corner position
	QPoint origin(cornerMargin, height() - cornerMargin);

	// Draw X axis (red) - right
	painter.setPen(QPen(QColor(255, 100, 100), axisThickness, Qt::SolidLine, Qt::RoundCap));
	painter.drawLine(origin, origin + QPoint(axisLength, 0));

	// Draw Y axis (green) - up
	painter.setPen(QPen(QColor(100, 255, 100), axisThickness, Qt::SolidLine, Qt::RoundCap));
	painter.drawLine(origin, origin - QPoint(0, axisLength));

	// Draw Z axis (blue) - diagonal for 3D effect
	painter.setPen(QPen(QColor(100, 150, 255), axisThickness, Qt::SolidLine, Qt::RoundCap));
	painter.drawLine(origin, origin + QPoint(axisLength/2, -axisLength/2));

	// Draw axis labels
	painter.setPen(QPen(Qt::white, 1));
	painter.setFont(QFont("Arial", 8, QFont::Bold));
	painter.drawText(origin + QPoint(axisLength + 5, 5), "X");
	painter.drawText(origin + QPoint(-10, -axisLength - 5), "Y");
	painter.drawText(origin + QPoint(axisLength/2 + 10, -axisLength/2 - 5), "Z");
}

void VkViewportWidget::drawEnhancedGrid(QPainter& painter) {
	float gridStep = s_gridSize;
	if (gridStep < 1.0f) {
		gridStep = 32.0f; // Default fallback
	}

	const int viewportWidth = width();
	const int viewportHeight = height();

	// Calculate center point
	QPoint center(viewportWidth / 2, viewportHeight / 2);

	// Draw major grid lines (every N units)
	const int majorGridInterval = 4; // Every 4th grid line is major
	const float majorGridSize = gridStep * majorGridInterval;

	// Calculate zoom-based opacity for depth fading effect
	float zoomOpacity = qBound(0.1f, 1.0f - (m_distance - 1.0f) * 0.1f, 1.0f);

	// Minor grid lines (thinner, lighter) with depth fading
	QColor minorColor(70, 70, 70);
	minorColor.setAlphaF(minorColor.alphaF() * zoomOpacity);
	painter.setPen(QPen(minorColor, 1, Qt::DotLine));

	for (int x = 0; x < viewportWidth; x += static_cast<int>(gridStep)) {
		if ((x / static_cast<int>(gridStep)) % majorGridInterval != 0) {
			painter.drawLine(x, 0, x, viewportHeight);
		}
	}
	for (int y = 0; y < viewportHeight; y += static_cast<int>(gridStep)) {
		if ((y / static_cast<int>(gridStep)) % majorGridInterval != 0) {
			painter.drawLine(0, y, viewportWidth, y);
		}
	}

	// Major grid lines (thicker, more visible) with depth fading
	QColor majorColor(100, 100, 100);
	majorColor.setAlphaF(majorColor.alphaF() * zoomOpacity);
	painter.setPen(QPen(majorColor, 2, Qt::SolidLine));

	for (int x = 0; x < viewportWidth; x += static_cast<int>(majorGridSize)) {
		painter.drawLine(x, 0, x, viewportHeight);
	}
	for (int y = 0; y < viewportHeight; y += static_cast<int>(majorGridSize)) {
		painter.drawLine(0, y, viewportWidth, y);
	}

	// Draw center cross (origin indicator) with pulsing effect
	static int crossPulseCounter = 0;
	crossPulseCounter = (crossPulseCounter + 1) % 60;
	float crossPulse = 0.5f + 0.5f * sin(crossPulseCounter * 0.1f);

	QColor crossColor(150, 150, 150);
	crossColor.setAlphaF(crossColor.alphaF() * pulse);
	painter.setPen(QPen(crossColor, 1, Qt::DashLine));

	const int crossSize = 20;
	painter.drawLine(center.x() - crossSize, center.y(), center.x() + crossSize, center.y());
	painter.drawLine(center.x(), center.y() - crossSize, center.x(), center.y() + crossSize);

	// Enhanced center point with pulsing effect and gradient
	static int centerPulseCounter = 0;
	centerPulseCounter = (centerPulseCounter + 1) % 120;
	float centerPulse = 0.3f + 0.7f * fabs(sin(centerPulseCounter * 0.05f));

	QRadialGradient gradient(center, 8);
	gradient.setColorAt(0, QColor(100, 150, 255, static_cast<int>(200 * centerPulse)));
	gradient.setColorAt(0.7, QColor(150, 200, 255, static_cast<int>(150 * centerPulse)));
	gradient.setColorAt(1, QColor(200, 200, 200, static_cast<int>(50 * centerPulse)));

	painter.setPen(QPen(QColor(100, 150, 255, static_cast<int>(255 * centerPulse)), 2));
	painter.setBrush(QBrush(gradient));
	painter.drawEllipse(center, 6, 6);

	// Add inner highlight
	QColor innerColor(255, 255, 255, static_cast<int>(150 * centerPulse));
	painter.setPen(Qt::NoPen);
	painter.setBrush(QBrush(innerColor));
	painter.drawEllipse(center, 2, 2);

	// Draw grid coordinate labels at edges (for orthographic views)
	if (m_viewType != ViewType::Perspective) {
		drawGridLabels(painter);
	}
}

void VkViewportWidget::drawViewportInfo(QPainter& painter) {
	const QString title = m_loaded ? QStringLiteral("Vulkan/PBR Viewport (engine)") : QStringLiteral("Qt Viewport");
	const QString detail = m_status.isEmpty() ? QStringLiteral("Ready") : m_status;
	const QString cam = QStringLiteral("Cam: Y%.0f P%.0f D%.1f").arg(m_yaw).arg(m_pitch).arg(m_distance);
	const QString view = QStringLiteral("View: %1").arg(viewName());
	const QString sel = m_hasSelection ?
		QStringLiteral("Sel: (%.1f, %.1f, %.1f)").arg(m_selection.x()).arg(m_selection.y()).arg(m_selection.z()) :
		QStringLiteral("No Selection");

	const QString grid = QStringLiteral("Grid: %.1f").arg(s_gridSize);

	// Add bookmark status
	QString bookmarks;
	for (int i = 0; i < 10; ++i) {
		if (m_bookmarkSaved[i]) {
			bookmarks += QString::number(i);
		}
	}
	const QString bookmarkInfo = bookmarks.isEmpty() ? QStringLiteral("No bookmarks") :
								QStringLiteral("Bookmarks: %1").arg(bookmarks);

	// Create info text with better formatting
	QStringList infoLines;
	infoLines << title;
	infoLines << view;
	infoLines << detail;
	infoLines << cam;
	infoLines << sel;
	infoLines << grid;
	infoLines << bookmarkInfo;

	// If Vulkan renderer is not loaded, add helpful hint
	if (!m_loaded) {
		infoLines << QStringLiteral("Tip: Enable RADIANT_USE_ENGINE_RENDERER_VK");
	}

	// Draw background for text readability
	QRect textRect(10, 10, 300, infoLines.size() * 16 + 10);
	painter.fillRect(textRect, QColor(0, 0, 0, 120));

	// Draw text with better colors and spacing
	painter.setPen(QPen(QColor(255, 255, 255), 1));
	painter.setFont(QFont("Consolas", 9));

	int y = 20;
	for (const QString& line : infoLines) {
		painter.drawText(15, y, line);
		y += 16;
	}

	// Draw zoom level indicator in bottom-right corner
	const QString zoomText = QStringLiteral("Zoom: %1%").arg(static_cast<int>(100.0f / m_distance * 10.0f));
	QRect zoomRect(width() - 120, height() - 30, 110, 20);
	painter.fillRect(zoomRect, QColor(0, 0, 0, 120));
	painter.setPen(QPen(QColor(200, 200, 255), 1));
	painter.drawText(zoomRect.adjusted(5, 0, -5, 0), Qt::AlignVCenter | Qt::AlignRight, zoomText);
}

void VkViewportWidget::drawSelectionIndicators(QPainter& painter) {
	if (!m_hasSelection) return;

	// Draw all selected objects with enhanced visuals
	for (int i = 0; i < m_selectedObjects.size(); ++i) {
		const QVector3D& sel = m_selectedObjects[i];
		bool isPrimary = (sel - m_selection).lengthSquared() < 0.01f;

		// Convert 3D selection to 2D screen coordinates
		QPoint screenPos = worldToScreen(sel);

		// Skip if outside viewport bounds (with margin)
		const int margin = 50;
		if (screenPos.x() < -margin || screenPos.x() > width() + margin ||
			screenPos.y() < -margin || screenPos.y() > height() + margin) {
			continue;
		}

		// Enhanced selection indicator styling
		QColor selColor = isPrimary ? QColor(255, 220, 0) : QColor(180, 180, 100);
		const int crossSize = isPrimary ? 12 : 8;
		const int lineWidth = isPrimary ? 3 : 2;

		// Draw selection cross with glow effect
		painter.setPen(QPen(selColor.lighter(150), lineWidth + 2, Qt::SolidLine, Qt::RoundCap));
		painter.drawLine(screenPos.x() - crossSize, screenPos.y(), screenPos.x() + crossSize, screenPos.y());
		painter.drawLine(screenPos.x(), screenPos.y() - crossSize, screenPos.x(), screenPos.y() + crossSize);

		painter.setPen(QPen(selColor, lineWidth, Qt::SolidLine, Qt::RoundCap));
		painter.drawLine(screenPos.x() - crossSize, screenPos.y(), screenPos.x() + crossSize, screenPos.y());
		painter.drawLine(screenPos.x(), screenPos.y() - crossSize, screenPos.x(), screenPos.y() + crossSize);

		// Draw selection circle with fill
		painter.setPen(QPen(selColor, 2));
		painter.setBrush(QBrush(selColor.lighter(180)));
		painter.drawEllipse(screenPos, crossSize, crossSize);

		// Draw coordinate label for primary selection
		if (isPrimary) {
			QString coordText = QStringLiteral("(%.1f, %.1f, %.1f)").arg(sel.x()).arg(sel.y()).arg(sel.z());
			QRect labelRect(screenPos.x() + crossSize + 5, screenPos.y() - 10, 120, 20);

			// Background for label
			painter.fillRect(labelRect, QColor(0, 0, 0, 180));
			painter.setPen(QPen(selColor, 1));
			painter.drawRect(labelRect);

			// Label text
			painter.setPen(QPen(Qt::white, 1));
			painter.setFont(QFont("Arial", 8));
			painter.drawText(labelRect.adjusted(3, 0, -3, 0), Qt::AlignVCenter | Qt::AlignLeft, coordText);
		}
	}
}

void VkViewportWidget::drawFPSCounter(QPainter& painter) {
	// Draw FPS counter in top-right corner
	const QString fpsText = QStringLiteral("FPS: %1").arg(static_cast<int>(m_currentFPS));
	QRect fpsRect(width() - 80, 10, 70, 25);

	// Background for FPS counter
	QColor bgColor = m_currentFPS < 30 ? QColor(180, 50, 50, 200) :
					 m_currentFPS < 60 ? QColor(180, 180, 50, 200) :
					 QColor(50, 180, 50, 200);

	painter.fillRect(fpsRect.adjusted(-5, -5, 5, 5), bgColor);

	// FPS text
	painter.setPen(QPen(Qt::white, 2));
	painter.setFont(QFont("Consolas", 10, QFont::Bold));
	painter.drawText(fpsRect, Qt::AlignCenter, fpsText);

	// Border
	painter.setPen(QPen(Qt::black, 1));
	painter.setBrush(Qt::NoBrush);
	painter.drawRect(fpsRect.adjusted(-5, -5, 5, 5));
}

void VkViewportWidget::drawGridLabels(QPainter& painter) {
	// Draw coordinate labels at viewport edges for orthographic views
	const int margin = 5;
	const int labelSpacing = 50;

	painter.setFont(QFont("Arial", 8));
	painter.setPen(QPen(QColor(150, 150, 150), 1));

	// Calculate world coordinates at viewport edges
	float worldLeft = -m_panX - (width() / 2.0f) * m_distance / 256.0f;
	float worldRight = -m_panX + (width() / 2.0f) * m_distance / 256.0f;
	float worldTop = m_panY + (height() / 2.0f) * m_distance / 256.0f;
	float worldBottom = m_panY - (height() / 2.0f) * m_distance / 256.0f;

	// Draw X-axis labels (horizontal edges)
	for (int x = margin; x < width(); x += labelSpacing) {
		float worldX = worldLeft + (worldRight - worldLeft) * (x / (float)width());
		if (m_viewType == ViewType::Top) {
			painter.drawText(x - 10, margin + 10, QString::number(worldX, 'f', 0));
		} else if (m_viewType == ViewType::Front) {
			painter.drawText(x - 10, margin + 10, QString::number(worldX, 'f', 0));
		}
	}

	// Draw Y/Z-axis labels (vertical edges)
	for (int y = margin; y < height(); y += labelSpacing) {
		float worldY = worldTop - (worldTop - worldBottom) * (y / (float)height());
		if (m_viewType == ViewType::Top) {
			painter.drawText(margin, y + 3, QString::number(worldY, 'f', 0));
		} else if (m_viewType == ViewType::Side) {
			painter.drawText(margin, y + 3, QString::number(worldY, 'f', 0));
		}
	}

	// Draw axis labels
	painter.setPen(QPen(QColor(255, 100, 100), 2)); // Red for X
	painter.drawText(width() - 20, height() - 5, "X");

	painter.setPen(QPen(QColor(100, 255, 100), 2)); // Green for Y
	if (m_viewType == ViewType::Top) {
		painter.drawText(5, 15, "Y");
	} else {
		painter.drawText(5, height() - 5, "Y");
	}

	painter.setPen(QPen(QColor(100, 150, 255), 2)); // Blue for Z
	if (m_viewType == ViewType::Front) {
		painter.drawText(5, height() - 5, "Z");
	} else if (m_viewType == ViewType::Side) {
		painter.drawText(width() - 20, 15, "Z");
	}
}

void VkViewportWidget::drawCameraFrustum(QPainter& painter) {
	// Draw perspective camera frustum in orthographic views
	// This shows what the perspective camera can see

	const float fovRadians = 75.0f * M_PI / 180.0f; // Default FOV
	const float aspectRatio = (float)width() / height();
	const float nearPlane = 0.1f;
	const float farPlane = m_distance * 2.0f; // Extend frustum based on camera distance

	// Calculate frustum corners at far plane
	const float tanHalfFov = tan(fovRadians * 0.5f);
	const float nearHeight = 2.0f * tanHalfFov * nearPlane;
	const float nearWidth = nearHeight * aspectRatio;
	const float farHeight = 2.0f * tanHalfFov * farPlane;
	const float farWidth = farHeight * aspectRatio;

	// Camera position in world space
	QVector3D cameraPos(0, 0, 0); // Perspective camera is at origin in view space

	// Transform camera space frustum to screen space
	QVector<QPoint> frustumPoints;

	// Near plane corners (not visible, but used for lines)
	QVector3D nearTopLeft(-nearWidth * 0.5f, nearHeight * 0.5f, -nearPlane);
	QVector3D nearTopRight(nearWidth * 0.5f, nearHeight * 0.5f, -nearPlane);
	QVector3D nearBottomLeft(-nearWidth * 0.5f, -nearHeight * 0.5f, -nearPlane);
	QVector3D nearBottomRight(nearWidth * 0.5f, -nearHeight * 0.5f, -nearPlane);

	// Far plane corners
	QVector3D farTopLeft(-farWidth * 0.5f, farHeight * 0.5f, -farPlane);
	QVector3D farTopRight(farWidth * 0.5f, farHeight * 0.5f, -farPlane);
	QVector3D farBottomLeft(-farWidth * 0.5f, -farHeight * 0.5f, -farPlane);
	QVector3D farBottomRight(farWidth * 0.5f, -farHeight * 0.5f, -farPlane);

	// Convert to screen coordinates
	auto toScreen = [&](const QVector3D& world) -> QPoint {
		return worldToScreen(world);
	};

	// Draw frustum lines with semi-transparent cyan color
	QColor frustumColor(0, 255, 255, 100);
	painter.setPen(QPen(frustumColor, 2, Qt::SolidLine));

	// Near plane (optional, usually not visible)
	// painter.drawLine(toScreen(nearTopLeft), toScreen(nearTopRight));
	// painter.drawLine(toScreen(nearTopRight), toScreen(nearBottomRight));
	// painter.drawLine(toScreen(nearBottomRight), toScreen(nearBottomLeft));
	// painter.drawLine(toScreen(nearBottomLeft), toScreen(nearTopLeft));

	// Far plane
	painter.drawLine(toScreen(farTopLeft), toScreen(farTopRight));
	painter.drawLine(toScreen(farTopRight), toScreen(farBottomRight));
	painter.drawLine(toScreen(farBottomRight), toScreen(farBottomLeft));
	painter.drawLine(toScreen(farBottomLeft), toScreen(farTopLeft));

	// Connecting lines from camera to far plane
	QPoint cameraScreen = toScreen(cameraPos);
	painter.drawLine(cameraScreen, toScreen(farTopLeft));
	painter.drawLine(cameraScreen, toScreen(farTopRight));
	painter.drawLine(cameraScreen, toScreen(farBottomLeft));
	painter.drawLine(cameraScreen, toScreen(farBottomRight));

	// Draw camera icon at frustum origin
	painter.setPen(QPen(QColor(0, 255, 255), 2));
	painter.setBrush(QBrush(QColor(0, 255, 255, 50)));
	painter.drawEllipse(cameraScreen, 8, 8);

	// Camera direction indicator
	QPoint cameraDirEnd = toScreen(QVector3D(0, 0, -0.5f));
	painter.drawLine(cameraScreen, cameraDirEnd);
}

void VkViewportWidget::drawMeasurementOverlay(QPainter& painter) {
	// Draw measurement lines, distances, and angles between selected objects
	if (m_selectedObjects.size() < 2) return;

	painter.setFont(QFont("Arial", 8, QFont::Bold));

	for (int i = 0; i < m_selectedObjects.size(); ++i) {
		for (int j = i + 1; j < m_selectedObjects.size(); ++j) {
			QPoint p1 = worldToScreen(m_selectedObjects[i]);
			QPoint p2 = worldToScreen(m_selectedObjects[j]);

			// Skip if either point is off-screen
			if (p1.x() < -50 || p1.x() > width() + 50 || p1.y() < -50 || p1.y() > height() + 50 ||
				p2.x() < -50 || p2.x() > width() + 50 || p2.y() < -50 || p2.y() > height() + 50) {
				continue;
			}

			// Draw measurement line with gradient effect
			QLinearGradient gradient(p1, p2);
			gradient.setColorAt(0, QColor(255, 255, 0, 200));
			gradient.setColorAt(0.5, QColor(255, 220, 0, 150));
			gradient.setColorAt(1, QColor(255, 255, 0, 200));

			painter.setPen(QPen(QBrush(gradient), 2, Qt::SolidLine));
			painter.drawLine(p1, p2);

			// Calculate and display distance
			float distance = (m_selectedObjects[i] - m_selectedObjects[j]).length();
			QPoint midPoint = (p1 + p2) / 2;

			// Draw distance measurement
			QString distText = QStringLiteral("%.2f").arg(distance);
			QFontMetrics fm(painter.font());
			QRect distRect = fm.boundingRect(distText);
			distRect.moveCenter(midPoint);
			distRect.adjust(-4, -4, 4, 4);

			// Background with rounded corners
			QPainterPath bgPath;
			bgPath.addRoundedRect(distRect, 3, 3);
			painter.fillPath(bgPath, QColor(0, 0, 0, 200));
			painter.setPen(QPen(QColor(255, 255, 0), 1));
			painter.drawPath(bgPath);

			painter.setPen(QPen(Qt::white, 1));
			painter.drawText(distRect, Qt::AlignCenter, distText);

			// Draw units label below
			QRect unitsRect = distRect.adjusted(0, distRect.height(), 0, 4);
			painter.setPen(QPen(QColor(200, 200, 200), 1));
			painter.setFont(QFont("Arial", 6));
			painter.drawText(unitsRect, Qt::AlignCenter, "units");
		}
	}

	// Draw angle measurements for triangles (3+ points)
	if (m_selectedObjects.size() >= 3) {
		for (int i = 0; i < m_selectedObjects.size(); ++i) {
			for (int j = i + 1; j < m_selectedObjects.size(); ++j) {
				for (int k = j + 1; k < m_selectedObjects.size(); ++k) {
					drawAngleMeasurement(painter, m_selectedObjects[i], m_selectedObjects[j], m_selectedObjects[k]);
				}
			}
		}
	}
}

void VkViewportWidget::drawAngleMeasurement(QPainter& painter, const QVector3D& p1, const QVector3D& p2, const QVector3D& vertex) {
	// Calculate angle at vertex between p1 and p2
	QVector3D v1 = (p1 - vertex).normalized();
	QVector3D v2 = (p2 - vertex).normalized();

	// Calculate angle in degrees
	float dot = QVector3D::dotProduct(v1, v2);
	float angle = qRadiansToDegrees(acos(qBound(-1.0f, dot, 1.0f)));

	// Only show angles between 15-165 degrees for readability
	if (angle < 15.0f || angle > 165.0f) return;

	QPoint screenVertex = worldToScreen(vertex);

	// Find arc center and draw angle arc
	QVector3D bisector = (v1 + v2).normalized();
	float radius = 30.0f; // Fixed screen radius for arc

	QPoint arcCenter = screenVertex + QPoint(bisector.x() * radius, -bisector.y() * radius);

	// Calculate start and span angles
	QVector3D refVec(1, 0, 0); // Reference vector along X-axis
	float angle1 = atan2(-v1.y(), v1.x()) * 180.0f / M_PI;
	float angle2 = atan2(-v2.y(), v2.x()) * 180.0f / M_PI;

	// Normalize angles
	while (angle1 < 0) angle1 += 360;
	while (angle2 < 0) angle2 += 360;

	float startAngle = qMin(angle1, angle2);
	float spanAngle = qAbs(angle1 - angle2);

	// Draw angle arc
	painter.setPen(QPen(QColor(0, 255, 255), 2));
	painter.setBrush(Qt::NoBrush);
	painter.drawArc(arcCenter.x() - radius, arcCenter.y() - radius,
					radius * 2, radius * 2, startAngle * 16, spanAngle * 16);

	// Draw angle text
	QString angleText = QStringLiteral("%.1f°").arg(angle);
	QFontMetrics fm(painter.font());
	QRect angleRect = fm.boundingRect(angleText);
	angleRect.moveCenter(arcCenter);
	angleRect.adjust(-3, -3, 3, 3);

	// Background for angle text
	painter.fillRect(angleRect, QColor(0, 0, 0, 200));
	painter.setPen(QPen(QColor(0, 255, 255), 1));
	painter.drawRect(angleRect);

	painter.setPen(QPen(Qt::white, 1));
	painter.drawText(angleRect, Qt::AlignCenter, angleText);
}

void VkViewportWidget::drawMinimap(QPainter& painter) {
	// Draw a small navigation minimap in the top-right corner
	const int minimapSize = 120;
	const int margin = 15;
	const int borderWidth = 2;

	QRect minimapRect(width() - minimapSize - margin, margin, minimapSize, minimapSize);

	// Minimap background
	painter.fillRect(minimapRect, QColor(20, 20, 20, 200));
	painter.setPen(QPen(QColor(100, 100, 100), borderWidth));
	painter.drawRect(minimapRect.adjusted(-borderWidth/2, -borderWidth/2, borderWidth/2, borderWidth/2));

	// Draw a simple representation of the scene bounds (placeholder)
	// In a full implementation, this would show the actual scene geometry
	const int sceneMargin = 10;
	QRect sceneRect = minimapRect.adjusted(sceneMargin, sceneMargin, -sceneMargin, -sceneMargin);

	// Draw scene bounds
	painter.setPen(QPen(QColor(150, 150, 150), 1, Qt::DashLine));
	painter.drawRect(sceneRect);

	// Draw camera viewport indicator
	float cameraX = (m_panX + 1000.0f) / 2000.0f; // Normalize to 0-1 range (placeholder calculation)
	float cameraY = (m_panY + 1000.0f) / 2000.0f;
	float viewportWidth = minimapSize - 2 * sceneMargin;
	float viewportHeight = minimapSize - 2 * sceneMargin;

	QRect viewportRect(
		minimapRect.x() + sceneMargin + cameraX * viewportWidth - 15,
		minimapRect.y() + sceneMargin + cameraY * viewportHeight - 10,
		30, 20
	);

	painter.setPen(QPen(QColor(0, 255, 255), 2));
	painter.setBrush(QBrush(QColor(0, 255, 255, 50)));
	painter.drawRect(viewportRect);

	// Minimap title
	painter.setPen(QPen(Qt::white, 1));
	painter.setFont(QFont("Arial", 7, QFont::Bold));
	painter.drawText(minimapRect.adjusted(0, -12, 0, 0), Qt::AlignHCenter, "NAV");

	// Zoom indicator
	float zoomLevel = 100.0f / m_distance;
	painter.setFont(QFont("Arial", 6));
	painter.drawText(minimapRect.adjusted(0, minimapSize + 2, 0, 0), Qt::AlignHCenter,
					QStringLiteral("Z:%1%").arg(static_cast<int>(zoomLevel)));
}

void VkViewportWidget::drawCoordinateDisplay(QPainter& painter) {
	// Get mouse position in widget coordinates
	QPoint mousePos = mapFromGlobal(QCursor::pos());

	// Only show if mouse is actually over the viewport
	if (!rect().contains(mousePos)) return;

	// Convert to world coordinates
	QVector3D worldPos = screenToWorld(mousePos);

	// Get snapped position if snapping is active
	QVector3D snappedPos = SnappingSystem::instance().snapPosition(worldPos);
	bool isSnapped = (worldPos - snappedPos).length() < 0.01f;

	// Format coordinate display with snapping preview
	QString coordText;
	QString snapText;

	if (isSnapped) {
		switch (m_viewType) {
		case ViewType::Top:
			coordText = QStringLiteral("X: %.2f  Y: %.2f").arg(snappedPos.x()).arg(snappedPos.y());
			break;
		case ViewType::Front:
			coordText = QStringLiteral("X: %.2f  Z: %.2f").arg(snappedPos.x()).arg(snappedPos.z());
			break;
		case ViewType::Side:
			coordText = QStringLiteral("Y: %.2f  Z: %.2f").arg(snappedPos.y()).arg(snappedPos.z());
			break;
		case ViewType::Perspective:
		default:
			coordText = QStringLiteral("X: %.2f  Y: %.2f  Z: %.2f").arg(snappedPos.x()).arg(snappedPos.y()).arg(snappedPos.z());
			break;
		}
		snapText = QStringLiteral("SNAPPED");
	} else {
		switch (m_viewType) {
		case ViewType::Top:
			coordText = QStringLiteral("X: %.2f  Y: %.2f").arg(worldPos.x()).arg(worldPos.y());
			break;
		case ViewType::Front:
			coordText = QStringLiteral("X: %.2f  Z: %.2f").arg(worldPos.x()).arg(worldPos.z());
			break;
		case ViewType::Side:
			coordText = QStringLiteral("Y: %.2f  Z: %.2f").arg(worldPos.y()).arg(worldPos.z());
			break;
		case ViewType::Perspective:
		default:
			coordText = QStringLiteral("X: %.2f  Y: %.2f  Z: %.2f").arg(worldPos.x()).arg(worldPos.y()).arg(worldPos.z());
			break;
		}
		snapText = QStringLiteral("FREE");
	}

	// Position in bottom-left corner with dynamic width
	QFontMetrics fm(QFont("Consolas", 9, QFont::Bold));
	int coordWidth = fm.horizontalAdvance(coordText) + 60;
	int snapWidth = fm.horizontalAdvance(snapText) + 20;
	int totalWidth = coordWidth + snapWidth + 10;

	QRect coordRect(10, height() - 35, totalWidth, 25);

	// Background for readability with rounded corners
	QPainterPath bgPath;
	bgPath.addRoundedRect(coordRect.adjusted(-5, -5, 5, 5), 5, 5);
	painter.fillPath(bgPath, QColor(0, 0, 0, 200));

	// Border with snap status color
	QColor borderColor = isSnapped ? QColor(100, 255, 100) : QColor(150, 150, 150);
	painter.setPen(QPen(borderColor, 1));
	painter.drawPath(bgPath);

	// Coordinate text
	painter.setPen(QPen(QColor(200, 200, 255), 1));
	painter.setFont(QFont("Consolas", 9, QFont::Bold));
	painter.drawText(coordRect.adjusted(5, 0, -snapWidth-10, 0), Qt::AlignVCenter | Qt::AlignLeft, coordText);

	// Snap status indicator
	QColor snapColor = isSnapped ? QColor(100, 255, 100) : QColor(200, 200, 200);
	painter.setPen(QPen(snapColor, 1));
	painter.drawText(coordRect.adjusted(coordWidth - snapWidth + 5, 0, -5, 0), Qt::AlignVCenter | Qt::AlignRight, snapText);

	// Draw snap preview line if snapping and there's a significant difference
	if (isSnapped && (worldPos - snappedPos).length() > 0.1f) {
		QPoint worldScreen = worldToScreen(worldPos);
		QPoint snapScreen = worldToScreen(snappedPos);

		painter.setPen(QPen(QColor(255, 255, 0, 150), 1, Qt::DashLine));
		painter.drawLine(worldScreen, snapScreen);

		// Small indicator at snap target
		painter.setPen(QPen(QColor(100, 255, 100), 2));
		painter.setBrush(QBrush(QColor(100, 255, 100, 100)));
		painter.drawEllipse(snapScreen, 3, 3);
	}
}

void VkViewportWidget::saveBookmark(int slot) {
	if (slot < 0 || slot >= 10) return;

	m_bookmarks[slot] = {m_yaw, m_pitch, m_distance, m_panX, m_panY, m_viewType};
	m_bookmarkSaved[slot] = true;
}

void VkViewportWidget::loadBookmark(int slot) {
	if (slot < 0 || slot >= 10 || !m_bookmarkSaved[slot]) return;

	const auto& bookmark = m_bookmarks[slot];
	m_yaw = bookmark.yaw;
	m_pitch = bookmark.pitch;
	m_distance = bookmark.distance;
	m_panX = bookmark.panX;
	m_panY = bookmark.panY;

	// Note: We don't change viewType as bookmarks are per-viewport
	update();
}

bool VkViewportWidget::hasBookmark(int slot) const {
	return slot >= 0 && slot < 10 && m_bookmarkSaved[slot];
}

void VkViewportWidget::drawPerformanceWarning(QPainter& painter) {
	if (m_currentFPS >= 30.0f) return;

	// Draw performance warning overlay
	QColor warningColor(255, 100, 100, 150);
	painter.fillRect(rect(), warningColor);

	// Warning text
	painter.setPen(QPen(Qt::white, 2));
	painter.setFont(QFont("Arial", 14, QFont::Bold));
	painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("⚠ LOW FPS\n%1 FPS").arg(static_cast<int>(m_currentFPS)));

	// Performance tips
	painter.setFont(QFont("Arial", 10));
	QStringList tips = {
		"Try reducing viewport resolution",
		"Disable wireframe mode",
		"Hide unnecessary geometry",
		"Reduce texture detail"
	};

	int y = height() / 2 + 50;
	for (const QString& tip : tips) {
		painter.drawText(width() / 2 - 150, y, 300, 20, Qt::AlignCenter, tip);
		y += 20;
	}
}

// ===== VIEWPORT ENHANCEMENT METHODS =====

// Measurement tools
void VkViewportWidget::addMeasurement(const QVector3D& start, const QVector3D& end) {
	Measurement measurement;
	measurement.start = start;
	measurement.end = end;
	measurement.distance = (end - start).length();
	measurement.active = true;
	m_measurements.append(measurement);
	update();
}

void VkViewportWidget::clearMeasurements() {
	m_measurements.clear();
	update();
}

// Bookmarks
void VkViewportWidget::saveBookmark(int slot) {
	if (slot < 0 || slot >= 10) return;

	m_bookmarks[slot].yaw = m_yaw;
	m_bookmarks[slot].pitch = m_pitch;
	m_bookmarks[slot].distance = m_distance;
	m_bookmarks[slot].panX = m_panX;
	m_bookmarks[slot].panY = m_panY;
	m_bookmarks[slot].valid = true;

	// Could save to QSettings here for persistence
}


// Performance monitoring
void VkViewportWidget::updatePerformanceStats(float fps, float frameTime, int triangles, int drawCalls) {
	m_fps = fps;
	m_frameTime = frameTime;
	m_triangleCount = triangles;
	m_drawCallCount = drawCalls;
	update();
}

// Enhanced measurement drawing
void VkViewportWidget::drawMeasurements(QPainter& painter) {
	if (m_measurements.isEmpty()) return;

	painter.setFont(QFont("Arial", 8));

	for (const Measurement& measurement : m_measurements) {
		if (!measurement.active) continue;

		// Convert 3D points to screen coordinates
		QPoint startScreen = worldToScreen(measurement.start);
		QPoint endScreen = worldToScreen(measurement.end);

		// Draw measurement line
		QColor lineColor(255, 255, 0, 200); // Yellow with transparency
		painter.setPen(QPen(lineColor, 2));
		painter.drawLine(startScreen, endScreen);

		// Draw distance label
		QPoint midPoint = (startScreen + endScreen) / 2;
		QString distanceText = QStringLiteral("%.2f units").arg(measurement.distance);

		// Background for text
		QFontMetrics fm(painter.font());
		QRect textRect = fm.boundingRect(distanceText);
		textRect.moveCenter(midPoint);

		painter.setPen(Qt::NoPen);
		painter.setBrush(QBrush(QColor(0, 0, 0, 180)));
		painter.drawRect(textRect.adjusted(-2, -2, 2, 2));

		// Text
		painter.setPen(QPen(Qt::white));
		painter.drawText(textRect, Qt::AlignCenter, distanceText);

		// Draw endpoint markers
		painter.setPen(QPen(lineColor, 3));
		painter.setBrush(QBrush(lineColor));
		painter.drawEllipse(startScreen, 4, 4);
		painter.drawEllipse(endScreen, 4, 4);
	}
}

void VkViewportWidget::drawCameraFrustum(QPainter& painter) {
	// TODO: Draw camera frustum visualization for orthographic views
	Q_UNUSED(painter);
}

void VkViewportWidget::drawMeasurementOverlay(QPainter& painter) {
	// TODO: Draw measurement creation overlay
	Q_UNUSED(painter);
}

void VkViewportWidget::drawMinimap(QPainter& painter) {
	// TODO: Draw navigation minimap in corner
	Q_UNUSED(painter);
}

void VkViewportWidget::drawPerformanceWarning(QPainter& painter) {
	// Draw performance warning when FPS is low
	QColor warningColor(255, 165, 0, 200); // Orange
	painter.setPen(QPen(warningColor, 2));
	painter.setBrush(QBrush(QColor(255, 165, 0, 100)));

	int warningSize = 100;
	int x = width() - warningSize - 10;
	int y = 10;

	painter.drawRoundedRect(x, y, warningSize, 40, 5, 5);

	painter.setPen(QPen(Qt::white));
	painter.setFont(QFont("Arial", 8));
	painter.drawText(x + 5, y + 15, "Low FPS!");
	painter.drawText(x + 5, y + 30, QString("Current: %1").arg(m_currentFPS, 0, 'f', 1));
}

void VkViewportWidget::drawMeasurements(QPainter& painter) {
	for (const Measurement& measurement : m_measurements) {
		if (!measurement.active) continue;

		QPoint startScreen = worldToScreen(measurement.start);
		QPoint endScreen = worldToScreen(measurement.end);

		if (startScreen.x() < 0 || startScreen.y() < 0 || startScreen.x() >= width() || startScreen.y() >= height() ||
			endScreen.x() < 0 || endScreen.y() < 0 || endScreen.x() >= width() || endScreen.y() >= height()) {
			continue; // Skip measurements where endpoints are not visible
		}

		QColor lineColor(255, 255, 0, 180); // Yellow measurement lines
		painter.setPen(QPen(lineColor, 2));

		// Draw measurement line
		painter.drawLine(startScreen, endScreen);

		// Calculate midpoint for text
		QPoint midPoint = (startScreen + endScreen) / 2;

		// Draw background for text
		QFontMetrics fm(painter.font());
		QString distanceText = QString("%1").arg(measurement.distance, 0, 'f', 2);
		QRect textRect = fm.boundingRect(distanceText);
		textRect.moveCenter(midPoint);

		painter.setPen(Qt::NoPen);
		painter.setBrush(QBrush(QColor(0, 0, 0, 180)));
		painter.drawRect(textRect.adjusted(-2, -2, 2, 2));

		// Text
		painter.setPen(QPen(Qt::white));
		painter.drawText(textRect, Qt::AlignCenter, distanceText);

		// Draw endpoint markers
		painter.setPen(QPen(lineColor, 3));
		painter.setBrush(QBrush(lineColor));
		painter.drawEllipse(startScreen, 4, 4);
		painter.drawEllipse(endScreen, 4, 4);
	}
}

void VkViewportWidget::drawCoordinateDisplay(QPainter& painter) {
	// TODO: Draw coordinate display
	Q_UNUSED(painter);
}

void VkViewportWidget::drawCrosshair(QPainter& painter) {
	int centerX = width() / 2;
	int centerY = height() / 2;

	QColor crosshairColor(255, 255, 255, 150);
	painter.setPen(QPen(crosshairColor, 1));

	// Horizontal line
	painter.drawLine(centerX - 10, centerY, centerX + 10, centerY);
	// Vertical line
	painter.drawLine(centerX, centerY - 10, centerX, centerY + 10);

	// Center dot
	painter.setPen(Qt::NoPen);
	painter.setBrush(QBrush(crosshairColor));
	painter.drawEllipse(centerX - 1, centerY - 1, 2, 2);
}

// ===== ENHANCED VIEWPORT FEATURES =====

void VkViewportWidget::addMeasurement(const QVector3D& start, const QVector3D& end) {
	Measurement measurement;
	measurement.start = start;
	measurement.end = end;
	measurement.distance = (end - start).length();
	measurement.active = true;

	m_measurements.append(measurement);
	update();
}

void VkViewportWidget::clearMeasurements() {
	m_measurements.clear();
	update();
}

void VkViewportWidget::saveBookmark(int slot) {
	if (slot < 0 || slot >= 10) return;

	m_bookmarks[slot].yaw = m_yaw;
	m_bookmarks[slot].pitch = m_pitch;
	m_bookmarks[slot].distance = m_distance;
	m_bookmarks[slot].panX = m_panX;
	m_bookmarks[slot].panY = m_panY;
	m_bookmarks[slot].valid = true;

	// Show feedback
	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Camera bookmark saved to slot %1").arg(slot));
	}
}

void VkViewportWidget::loadBookmark(int slot) {
	if (slot < 0 || slot >= 10 || !m_bookmarks[slot].valid) return;

	m_yaw = m_bookmarks[slot].yaw;
	m_pitch = m_bookmarks[slot].pitch;
	m_distance = m_bookmarks[slot].distance;
	m_panX = m_bookmarks[slot].panX;
	m_panY = m_bookmarks[slot].panY;

	update();

	// Show feedback
	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Camera bookmark loaded from slot %1").arg(slot));
	}
}

void VkViewportWidget::updatePerformanceStats(float fps, float frameTime, int triangles, int drawCalls) {
	m_fps = fps;
	m_frameTime = frameTime;
	m_triangleCount = triangles;
	m_drawCallCount = drawCalls;
}

QPoint VkViewportWidget::worldToScreen(const QVector3D& worldPos) const {
	QVector4D pos4(worldPos.x(), worldPos.y(), worldPos.z(), 1.0f);

	// Get current view and projection matrices
	QMatrix4x4 viewMatrix, projMatrix;
	buildViewMatrices(viewMatrix, projMatrix);

	QVector4D viewPos = viewMatrix * pos4;
	QVector4D clipPos = projMatrix * viewPos;

	if (clipPos.w() <= 0.0f) {
		return QPoint(-1, -1); // Behind camera
	}

	float invW = 1.0f / clipPos.w();
	QPoint screenPos(
		static_cast<int>((clipPos.x() * invW + 1.0f) * 0.5f * width()),
		static_cast<int>((1.0f - clipPos.y() * invW) * 0.5f * height())
	);

	return screenPos;
}

void VkViewportWidget::drawWorldAxes(QPainter& painter) {
	if (!m_showAxes) return;

	// Draw world coordinate axes
	QVector3D origin(0, 0, 0);
	QPoint originScreen = worldToScreen(origin);

	if (originScreen.x() < 0 || originScreen.y() < 0 ||
		originScreen.x() >= width() || originScreen.y() >= height()) {
		return; // Origin not visible
	}

	// Draw axis lines (X=red, Y=green, Z=blue)
	const float axisLength = 1.0f;

	// X axis (red)
	QVector3D xAxis(axisLength, 0, 0);
	QPoint xScreen = worldToScreen(xAxis);
	if (xScreen.x() >= 0 && xScreen.y() >= 0 && xScreen.x() < width() && xScreen.y() < height()) {
		painter.setPen(QPen(Qt::red, 2));
		painter.drawLine(originScreen, xScreen);
		painter.drawText(xScreen + QPoint(5, 0), "X");
	}

	// Y axis (green)
	QVector3D yAxis(0, axisLength, 0);
	QPoint yScreen = worldToScreen(yAxis);
	if (yScreen.x() >= 0 && yScreen.y() >= 0 && yScreen.x() < width() && yScreen.y() < height()) {
		painter.setPen(QPen(Qt::green, 2));
		painter.drawLine(originScreen, yScreen);
		painter.drawText(yScreen + QPoint(5, 0), "Y");
	}

	// Z axis (blue)
	QVector3D zAxis(0, 0, axisLength);
	QPoint zScreen = worldToScreen(zAxis);
	if (zScreen.x() >= 0 && zScreen.y() >= 0 && zScreen.x() < width() && zScreen.y() < height()) {
		painter.setPen(QPen(Qt::blue, 2));
		painter.drawLine(originScreen, zScreen);
		painter.drawText(zScreen + QPoint(5, 0), "Z");
	}
}

void VkViewportWidget::drawEnhancedGrid(QPainter& painter) {
	if (!m_showGrid) return;

	QColor gridColor(100, 100, 100, 80);
	painter.setPen(QPen(gridColor, 1));

	float gridSize = VkViewportWidget::gridSize();
	if (gridSize <= 0.0f) return;

	// Calculate visible grid range based on current view
	QRectF visibleRect;
	switch (m_viewType) {
	case ViewType::Top:
		visibleRect = QRectF(-m_panX - 10, -m_panY - 10, 20, 20);
		break;
	case ViewType::Front:
		visibleRect = QRectF(-m_panX - 10, -m_panY - 10, 20, 20);
		break;
	case ViewType::Side:
		visibleRect = QRectF(-m_panX - 10, -m_panY - 10, 20, 20);
		break;
	default:
		return; // Don't draw grid for perspective view
	}

	// Draw grid lines
	int startX = static_cast<int>(visibleRect.left() / gridSize) * gridSize;
	int endX = static_cast<int>(visibleRect.right() / gridSize + 1) * gridSize;
	int startY = static_cast<int>(visibleRect.top() / gridSize) * gridSize;
	int endY = static_cast<int>(visibleRect.bottom() / gridSize + 1) * gridSize;

	// Vertical lines
	for (int x = startX; x <= endX; x += gridSize) {
		QVector3D worldStart(x, visibleRect.top(), 0);
		QVector3D worldEnd(x, visibleRect.bottom(), 0);

		QPoint screenStart = worldToScreen(worldStart);
		QPoint screenEnd = worldToScreen(worldEnd);

		if (screenStart.x() >= 0 && screenStart.x() < width()) {
			painter.drawLine(screenStart.x(), 0, screenStart.x(), height());
		}
	}

	// Horizontal lines
	for (int y = startY; y <= endY; y += gridSize) {
		QVector3D worldStart(visibleRect.left(), y, 0);
		QVector3D worldEnd(visibleRect.right(), y, 0);

		QPoint screenStart = worldToScreen(worldStart);
		QPoint screenEnd = worldToScreen(worldEnd);

		if (screenStart.y() >= 0 && screenStart.y() < height()) {
			painter.drawLine(0, screenStart.y(), width(), screenStart.y());
		}
	}
}

void VkViewportWidget::drawViewportInfo(QPainter& painter) {
	if (!m_showStats) return;

	QColor textColor(255, 255, 255, 200);
	painter.setPen(QPen(textColor));
	painter.setFont(QFont("Arial", 9));

	int lineHeight = 15;
	int startY = 10;
	int startX = 10;

	if (m_showFPS) {
		painter.drawText(startX, startY, QString("FPS: %1").arg(m_fps, 0, 'f', 1));
		startY += lineHeight;
	}

	if (m_showStats) {
		painter.drawText(startX, startY, QString("Triangles: %1").arg(m_triangleCount));
		startY += lineHeight;
		painter.drawText(startX, startY, QString("Draw Calls: %1").arg(m_drawCallCount));
		startY += lineHeight;
		painter.drawText(startX, startY, QString("Frame Time: %1ms").arg(m_frameTime, 0, 'f', 2));
		startY += lineHeight;
	}

	// View mode info
	QString viewMode;
	switch (m_viewType) {
	case ViewType::Perspective: viewMode = "Perspective"; break;
	case ViewType::Top: viewMode = "Top"; break;
	case ViewType::Front: viewMode = "Front"; break;
	case ViewType::Side: viewMode = "Side"; break;
	}

	if (m_viewType == ViewType::Perspective) {
		painter.drawText(startX, startY, QString("View: %1 (Yaw: %2°, Pitch: %3°)")
			.arg(viewMode).arg(m_yaw, 0, 'f', 1).arg(m_pitch, 0, 'f', 1));
	} else {
		painter.drawText(startX, startY, QString("View: %1").arg(viewMode));
	}
}

void VkViewportWidget::drawSelectionIndicators(QPainter& painter) {
	if (!m_hasSelection) return;

	// Draw selection highlight
	QPoint selectionScreen = worldToScreen(m_selection);
	if (selectionScreen.x() >= 0 && selectionScreen.y() >= 0 &&
		selectionScreen.x() < width() && selectionScreen.y() < height()) {

		QColor selectionColor(255, 255, 0, 150);
		painter.setPen(QPen(selectionColor, 2));
		painter.setBrush(Qt::NoBrush);

		int radius = 8;
		painter.drawEllipse(selectionScreen.x() - radius, selectionScreen.y() - radius,
						   radius * 2, radius * 2);

		// Selection coordinates if enabled
		if (m_showCoordinates) {
			painter.setPen(QPen(Qt::white));
			painter.setFont(QFont("Arial", 8));
			QString coords = QString("(%1, %2, %3)")
				.arg(m_selection.x(), 0, 'f', 2)
				.arg(m_selection.y(), 0, 'f', 2)
				.arg(m_selection.z(), 0, 'f', 2);
			painter.drawText(selectionScreen + QPoint(12, 0), coords);
		}
	}
}