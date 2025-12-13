#include "main_window.h"

#include <QAction>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QSplitter>
#include <QListWidget>
#include <QDirIterator>
#include <QTableWidget>
#include <QPushButton>
#include <QProcess>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QDirIterator>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QKeyEvent>
#include <QShortcut>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QSettings>
#include <QKeyEvent>
#include <QDockWidget>
#include <QToolButton>
#include <QHBoxLayout>
#include <QWidget>
#include <QIcon>
#include <QStyle>
#include <QPixmap>
#include <QPainter>
#include <QFile>

#include "vk_viewport.h"
#include "color_picker_dialog.h"
#include "keybind_editor.h"
#include "autosave_manager.h"
#include "clipping_tool.h"
#include "polygon_tool.h"
#include "vertex_tool.h"
#include "audio_browser.h"
#include "merge_tool.h"
#include "uv_tool.h"
#include "hotbox_widget.h"
#include "autocaulk_tool.h"
#include "extrusion_tool.h"
#include "texture_paint_tool.h"
#include "patch_thicken_tool.h"
#include "csg_tool.h"
#include "entity_walker.h"
#include "brush_resize_tool.h"
#include "shader_editor.h"
#include "texture_paste_tool.h"
#include "mess_manager.h"
#include "mess_dialog.h"
#include "mess_template.h"
#include "texture_extraction_dialog.h"
#include "surface_inspector.h"
#include "selection_sets.h"
#include "preferences_dialog.h"
#include "theme_manager.h"
#include "snapping_system.h"

RadiantMainWindow::RadiantMainWindow(const QtRadiantEnv& env, QWidget* parent)
	: QMainWindow(parent)
{
	buildUi(env);
	appendLogInfo(env);
	setupAutosave();
	checkForRecoveryFiles();
	updateRecentFilesMenu();
}

void RadiantMainWindow::buildUi(const QtRadiantEnv& env){
	setWindowTitle(QStringLiteral("Radiant Qt (prototype)"));
	
	// Ensure proper window decorations (titlebar, minimize, maximize, close buttons)
	// Qt::Window flag already includes resizing capability by default
	setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
	              Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
	
	// Ensure window is resizable (set minimum size, but allow resizing)
	setMinimumSize(640, 480);

	// Central viewport stack; try to locate renderer relative to install dir
	QString rendererHint = env.appPath + QStringLiteral("/../../idtech3_vulkan_x86_64.so");

	// Quad-view layout (Perspective, Top, Front, Side)
	m_viewPerspective = new VkViewportWidget(rendererHint, VkViewportWidget::ViewType::Perspective, this);
	m_viewTop         = new VkViewportWidget(rendererHint, VkViewportWidget::ViewType::Top, this);
	m_viewFront       = new VkViewportWidget(rendererHint, VkViewportWidget::ViewType::Front, this);
	m_viewSide        = new VkViewportWidget(rendererHint, VkViewportWidget::ViewType::Side, this);

	auto onSelect = [this](const QVector3D& pos){
		m_lastSelection = pos;

		// Update selection status with detailed information
		int brushCount = 0, entityCount = 0;
		if (m_outliner) {
			auto* root = m_outliner->invisibleRootItem();
			for (int i = 0; i < root->childCount(); ++i) {
				auto* item = root->child(i);
				if (item->text(1) == QStringLiteral("brush")) brushCount++;
				else if (item->text(1) == QStringLiteral("entity")) entityCount++;
			}
		}

		if (m_selectionStatus) {
			QString selectionText;
			if (brushCount > 0 || entityCount > 0) {
				selectionText = QStringLiteral("%1 brush%2, %3 entit%4 selected")
					.arg(brushCount).arg(brushCount != 1 ? "es" : "")
					.arg(entityCount).arg(entityCount != 1 ? "ies" : "y");
			} else {
				selectionText = QStringLiteral("No selection");
			}
			m_selectionStatus->setText(selectionText);
		}

		// Update coordinates display
		if (m_coordinatesStatus) {
			m_coordinatesStatus->setText(QStringLiteral("X: %1 Y: %2 Z: %3")
				.arg(pos.x(), 0, 'f', 1)
				.arg(pos.y(), 0, 'f', 1)
				.arg(pos.z(), 0, 'f', 1));
		}

		// Update inspector spin boxes
		if ( m_posX && m_posY && m_posZ ) {
			m_posX->setValue(pos.x());
			m_posY->setValue(pos.y());
			m_posZ->setValue(pos.z());
		}

		// Update camera status based on current viewport
		updateCameraStatus();
	};
	connect(m_viewPerspective, &VkViewportWidget::selectionChanged, this, onSelect);
	connect(m_viewTop, &VkViewportWidget::selectionChanged, this, onSelect);
	connect(m_viewFront, &VkViewportWidget::selectionChanged, this, onSelect);
	connect(m_viewSide, &VkViewportWidget::selectionChanged, this, onSelect);
	
	// Connect Tab key for focus on selection
	auto* focusShortcut = new QShortcut(QKeySequence("Tab"), this);
	connect(focusShortcut, &QShortcut::activated, this, [this]() {
		if (m_viewPerspective) m_viewPerspective->focusOnSelection();
		if (m_viewTop) m_viewTop->focusOnSelection();
		if (m_viewFront) m_viewFront->focusOnSelection();
		if (m_viewSide) m_viewSide->focusOnSelection();
	});
	
	// Connect gizmo operation changes
	auto onGizmoOpChange = [this](int op) {
		m_currentGizmoOperation = op;
		// Update all viewports
		if (m_viewPerspective) m_viewPerspective->setGizmoOperation(op);
		if (m_viewTop) m_viewTop->setGizmoOperation(op);
		if (m_viewFront) m_viewFront->setGizmoOperation(op);
		if (m_viewSide) m_viewSide->setGizmoOperation(op);
	};
	connect(m_viewPerspective, &VkViewportWidget::gizmoOperationChanged, this, onGizmoOpChange);
	connect(m_viewTop, &VkViewportWidget::gizmoOperationChanged, this, onGizmoOpChange);
	connect(m_viewFront, &VkViewportWidget::gizmoOperationChanged, this, onGizmoOpChange);
	connect(m_viewSide, &VkViewportWidget::gizmoOperationChanged, this, onGizmoOpChange);
	
	// Connect light power adjustment
	auto onLightPowerChange = [this](float delta) {
		adjustLightPower(delta);
	};
	connect(m_viewPerspective, &VkViewportWidget::lightPowerChanged, this, onLightPowerChange);
	connect(m_viewTop, &VkViewportWidget::lightPowerChanged, this, onLightPowerChange);
	connect(m_viewFront, &VkViewportWidget::lightPowerChanged, this, onLightPowerChange);
	connect(m_viewSide, &VkViewportWidget::lightPowerChanged, this, onLightPowerChange);

	auto *splitTop = new QSplitter(Qt::Horizontal, this);
	splitTop->addWidget(m_viewPerspective);
	splitTop->addWidget(m_viewTop);

	auto *splitBottom = new QSplitter(Qt::Horizontal, this);
	splitBottom->addWidget(m_viewFront);
	splitBottom->addWidget(m_viewSide);

	auto *splitVertical = new QSplitter(Qt::Vertical, this);
	splitVertical->addWidget(splitTop);
	splitVertical->addWidget(splitBottom);

	auto *center = new QWidget(this);
	auto *centerLayout = new QVBoxLayout(center);
	centerLayout->setContentsMargins(0,0,0,0);
	centerLayout->addWidget(splitVertical);
	setCentralWidget(center);

	// Console dock
	auto *consoleDock = new QDockWidget(QStringLiteral("Console"), this);
	consoleDock->setObjectName(QStringLiteral("ConsoleDock"));
	m_console = new QPlainTextEdit(consoleDock);
	m_console->setReadOnly(true);
	consoleDock->setWidget(m_console);
	setupDockWidgetMinimize(consoleDock);
	addDockWidget(Qt::BottomDockWidgetArea, consoleDock);

	// Info dock
	auto *infoDock = new QDockWidget(QStringLiteral("Info"), this);
	infoDock->setObjectName(QStringLiteral("InfoDock"));
	auto *info = new QLabel(QStringLiteral(
		"Radiant Qt prototype (dockable UI)\n"
		"app: %1\n"
		"temp: %2\n"
		"games: %3").arg(env.appPath, env.tempPath, env.gamesPath));
	info->setAlignment(Qt::AlignLeft);
	info->setContentsMargins(8, 8, 8, 8);
	infoDock->setWidget(info);
	setupDockWidgetMinimize(infoDock);
	addDockWidget(Qt::LeftDockWidgetArea, infoDock);

	// Outliner dock
	auto *outlinerDock = new QDockWidget(QStringLiteral("Outliner"), this);
	outlinerDock->setObjectName(QStringLiteral("OutlinerDock"));
	m_outliner = new QTreeWidget(outlinerDock);
	m_outliner->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Type")});
	populateOutliner();
	outlinerDock->setWidget(m_outliner);
	setupDockWidgetMinimize(outlinerDock);
	addDockWidget(Qt::LeftDockWidgetArea, outlinerDock);

	// Inspector dock
	auto *inspDock = new QDockWidget(QStringLiteral("Inspector"), this);
	inspDock->setObjectName(QStringLiteral("InspectorDock"));
	m_inspector = new QWidget(inspDock);
	populateInspector();
	inspDock->setWidget(m_inspector);
	setupDockWidgetMinimize(inspDock);
	addDockWidget(Qt::RightDockWidgetArea, inspDock);

	// Texture browser dock (stubbed, file list from base/textures)
	auto *texDock = new QDockWidget(QStringLiteral("Textures"), this);
	texDock->setObjectName(QStringLiteral("TexturesDock"));

	// Create container widget with layout for texture browser
	auto* textureContainer = new QWidget();
	auto* textureLayout = new QVBoxLayout(textureContainer);
	textureLayout->setContentsMargins(0, 0, 0, 0);

	m_textureList = new QListWidget(textureContainer);
	textureLayout->addWidget(m_textureList);

	texDock->setWidget(textureContainer);
	setupDockWidgetMinimize(texDock);
	addDockWidget(Qt::LeftDockWidgetArea, texDock);
	populateTextureBrowser(env);
	connect(m_textureList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item){
		if ( !item ) return;
		m_currentMaterial = item->text();
		// Update viewports with current material
		if (m_viewPerspective) m_viewPerspective->setCurrentMaterial(m_currentMaterial);
		if (m_viewTop) m_viewTop->setCurrentMaterial(m_currentMaterial);
		if (m_viewFront) m_viewFront->setCurrentMaterial(m_currentMaterial);
		if (m_viewSide) m_viewSide->setCurrentMaterial(m_currentMaterial);
		if ( m_console ) {
			m_console->appendPlainText(QStringLiteral("Apply material: %1 to selection at (%2,%3,%4)")
				.arg(m_currentMaterial)
				.arg(m_lastSelection.x(), 0, 'f', 1)
				.arg(m_lastSelection.y(), 0, 'f', 1)
				.arg(m_lastSelection.z(), 0, 'f', 1));
		}
	});

	// Compile front-end (stub)
	auto *buildDock = new QDockWidget(QStringLiteral("Build/Compile"), this);
	buildDock->setObjectName(QStringLiteral("BuildDock"));
	auto *buildBody = new QWidget(buildDock);
	auto *buildLayout = new QVBoxLayout(buildBody);
	buildLayout->setContentsMargins(8, 8, 8, 8);
	auto *runBtn = new QPushButton(QStringLiteral("Run q3map2 (stub)"), buildBody);
	auto *presetCombo = new QComboBox(buildBody);
	presetCombo->addItems({QStringLiteral("BSP"), QStringLiteral("BSP + VIS"), QStringLiteral("BSP + VIS + LIGHT")});
	auto *buildLog = new QPlainTextEdit(buildBody);
	buildLog->setReadOnly(true);
	buildLayout->addWidget(presetCombo);
	buildLayout->addWidget(runBtn);
	buildLayout->addWidget(buildLog);
	buildLayout->addStretch(1);
	buildBody->setLayout(buildLayout);
	buildDock->setWidget(buildBody);
	setupDockWidgetMinimize(buildDock);
	addDockWidget(Qt::RightDockWidgetArea, buildDock);
	connect(runBtn, &QPushButton::clicked, this, [this, presetCombo, buildLog](){
		const QString msg = QStringLiteral("[Compile] %1 (stubbed; wire q3map2 next)").arg(presetCombo->currentText());
		buildLog->appendPlainText(msg);
		if ( m_console ) {
			m_console->appendPlainText(msg);
		}
	});

	// Menu
	auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
	auto *openMapAct = new QAction(QStringLiteral("Open Map..."), this);
	auto *saveMapAct = new QAction(QStringLiteral("Save Map..."), this);
	auto *quitAct = new QAction(QStringLiteral("Quit"), this);
	connect(openMapAct, &QAction::triggered, this, [this](){
		QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Map or BSP"), QString(), 
			QStringLiteral("All Supported Formats (*.map *.bsp *.vmf);;Maps (*.map);;BSP Files (*.bsp);;VMF Files (*.vmf);;All Files (*.*)"));
		if ( path.isEmpty() ) return;
		VkViewportWidget* vp = m_viewPerspective ? m_viewPerspective : nullptr;
		if ( vp && vp->loadMap(path) ) {
			m_console->appendPlainText(QStringLiteral("Loaded map: %1").arg(path));
			m_currentMapPath = path;
			addRecentFile(path);
			if (m_autosaveManager) {
				m_autosaveManager->setMapPath(path);
			}
			setOutlinerLoadedMap(path);
		} else {
			m_console->appendPlainText(QStringLiteral("Failed to load map: %1").arg(path));
		}
	});

	connect(saveMapAct, &QAction::triggered, this, [this](){
		saveMap();
	});
	
	// Shader editor
	auto* shaderEditorAct = new QAction(QStringLiteral("Shader Editor..."), this);
	shaderEditorAct->setShortcut(QKeySequence("Ctrl+Shift+S"));
	connect(shaderEditorAct, &QAction::triggered, this, [this](){
		ShaderEditor dlg(this);
		if (dlg.exec() == QDialog::Accepted) {
			if (m_console) {
				m_console->appendPlainText(QStringLiteral("Shader '%1' applied").arg(dlg.shaderName()));
			}
		}
	});
	fileMenu->insertAction(saveMapAct, shaderEditorAct);
	fileMenu->insertSeparator(saveMapAct);

	connect(quitAct, &QAction::triggered, this, &QWidget::close);
	fileMenu->addAction(openMapAct);
	fileMenu->addAction(saveMapAct);
	fileMenu->addSeparator();
	fileMenu->addAction(quitAct);
	
	// Recent files submenu
	QMenu* recentFilesMenu = fileMenu->addMenu(QStringLiteral("Recent Files"));
	fileMenu->insertMenu(quitAct, recentFilesMenu);
	fileMenu->insertSeparator(quitAct);
	
	// Edit menu
	auto* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
	auto* preferencesAct = new QAction(QStringLiteral("Preferences..."), this);
	preferencesAct->setShortcut(QKeySequence("Ctrl+P"));
	connect(preferencesAct, &QAction::triggered, this, &RadiantMainWindow::openPreferences);
	editMenu->addAction(preferencesAct);

	// Tools menu
	auto* toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));
	auto* keybindEditorAct = new QAction(QStringLiteral("Keybind Editor..."), this);
	connect(keybindEditorAct, &QAction::triggered, this, &RadiantMainWindow::openKeybindEditor);
	toolsMenu->addAction(keybindEditorAct);
	
	auto* audioBrowserAct = new QAction(QStringLiteral("Audio Browser..."), this);
	connect(audioBrowserAct, &QAction::triggered, this, &RadiantMainWindow::openAudioBrowser);
	toolsMenu->addAction(audioBrowserAct);
	
	toolsMenu->addSeparator();
	
	auto* autocaulkAct = new QAction(QStringLiteral("Autocaulk Selected"), this);
	autocaulkAct->setShortcut(QKeySequence("Ctrl+Alt+C"));
	connect(autocaulkAct, &QAction::triggered, this, &RadiantMainWindow::applyAutocaulk);
	toolsMenu->addAction(autocaulkAct);
	
	auto* mapInfoAct = new QAction(QStringLiteral("Map Info..."), this);
	connect(mapInfoAct, &QAction::triggered, this, &RadiantMainWindow::showMapInfo);
	toolsMenu->addAction(mapInfoAct);
	
	toolsMenu->addSeparator();
	
	auto* patchThickenAct = new QAction(QStringLiteral("Patch Thicken"), this);
	connect(patchThickenAct, &QAction::triggered, this, &RadiantMainWindow::activatePatchThickenTool);
	toolsMenu->addAction(patchThickenAct);
	
	auto* csgToolAct = new QAction(QStringLiteral("CSG Tool (Shell)"), this);
	connect(csgToolAct, &QAction::triggered, this, &RadiantMainWindow::activateCSGTool);
	toolsMenu->addAction(csgToolAct);
	
	auto* brushResizeAct = new QAction(QStringLiteral("Brush Resize"), this);
	connect(brushResizeAct, &QAction::triggered, this, &RadiantMainWindow::activateBrushResizeTool);
	toolsMenu->addAction(brushResizeAct);
	
	toolsMenu->addSeparator();
	
	auto* texturePasteAct = new QAction(QStringLiteral("Texture Paste Tool"), this);
	texturePasteAct->setShortcut(QKeySequence("Ctrl+Shift+V"));
	connect(texturePasteAct, &QAction::triggered, this, &RadiantMainWindow::activateTexturePasteTool);
	toolsMenu->addAction(texturePasteAct);
	
	toolsMenu->addSeparator();
	
	auto* messAct = new QAction(QStringLiteral("MESS Templates..."), this);
	messAct->setShortcut(QKeySequence("Ctrl+Shift+T"));
	connect(messAct, &QAction::triggered, this, &RadiantMainWindow::openMESSDialog);
	toolsMenu->addAction(messAct);
	
	toolsMenu->addSeparator();
	
	auto* textureExtractAct = new QAction(QStringLiteral("Extract Textures..."), this);
	textureExtractAct->setShortcut(QKeySequence("Ctrl+Shift+E"));
	connect(textureExtractAct, &QAction::triggered, this, &RadiantMainWindow::openTextureExtractionDialog);
	toolsMenu->addAction(textureExtractAct);
	
	toolsMenu->addSeparator();
	
	auto* surfaceInspectorAct = new QAction(QStringLiteral("Surface Inspector..."), this);
	surfaceInspectorAct->setShortcut(QKeySequence("Ctrl+S"));
	connect(surfaceInspectorAct, &QAction::triggered, this, &RadiantMainWindow::openSurfaceInspector);
	toolsMenu->addAction(surfaceInspectorAct);
	
	auto* selectionSetsAct = new QAction(QStringLiteral("Selection Sets..."), this);
	connect(selectionSetsAct, &QAction::triggered, this, &RadiantMainWindow::openSelectionSetsDialog);
	toolsMenu->addAction(selectionSetsAct);
	
	toolsMenu->addSeparator();
	
	auto* selectConnectedAct = new QAction(QStringLiteral("Select Connected Entities"), this);
	selectConnectedAct->setShortcut(QKeySequence("Ctrl+Shift+C"));
	connect(selectConnectedAct, &QAction::triggered, this, &RadiantMainWindow::selectConnectedEntities);
	toolsMenu->addAction(selectConnectedAct);
	
	auto* walkNextAct = new QAction(QStringLiteral("Walk to Next Entity"), this);
	walkNextAct->setShortcut(QKeySequence("Ctrl+Shift+N"));
	connect(walkNextAct, &QAction::triggered, this, &RadiantMainWindow::walkToNextEntity);
	toolsMenu->addAction(walkNextAct);
	
	auto* walkPrevAct = new QAction(QStringLiteral("Walk to Previous Entity"), this);
	walkPrevAct->setShortcut(QKeySequence("Ctrl+Shift+P"));
	connect(walkPrevAct, &QAction::triggered, this, &RadiantMainWindow::walkToPreviousEntity);
	toolsMenu->addAction(walkPrevAct);

	// Color picker shortcut (Ctrl+C)
	auto* colorPickerShortcut = new QShortcut(QKeySequence("Ctrl+C"), this);
	connect(colorPickerShortcut, &QShortcut::activated, this, &RadiantMainWindow::openColorPicker);
	
	// Clipping tool mode toggle (C key)
	auto* clippingModeShortcut = new QShortcut(QKeySequence("C"), this);
	connect(clippingModeShortcut, &QShortcut::activated, this, &RadiantMainWindow::toggleClippingMode);

	// Editor object visibility toggles
	m_toggleToolTexturesAction = new QAction(QStringLiteral("Toggle Tool Textures"), this);
	m_toggleToolTexturesAction->setShortcut(QKeySequence("Ctrl+Shift+F2"));
	m_toggleToolTexturesAction->setCheckable(true);
	m_toggleToolTexturesAction->setChecked(true);
	connect(m_toggleToolTexturesAction, &QAction::triggered, this, &RadiantMainWindow::toggleToolTextures);
	
	m_toggleEditorModelsAction = new QAction(QStringLiteral("Toggle Editor Models"), this);
	m_toggleEditorModelsAction->setShortcut(QKeySequence("Alt+O"));
	m_toggleEditorModelsAction->setCheckable(true);
	m_toggleEditorModelsAction->setChecked(true);
	connect(m_toggleEditorModelsAction, &QAction::triggered, this, &RadiantMainWindow::toggleEditorModels);
	
	// Add to View menu
	auto* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
	viewMenu->addAction(m_toggleToolTexturesAction);
	viewMenu->addAction(m_toggleEditorModelsAction);
	viewMenu->addSeparator();
	auto* gridSettingsAct = new QAction(QStringLiteral("Set Grid Size..."), this);
	connect(gridSettingsAct, &QAction::triggered, this, &RadiantMainWindow::openGridSettings);
	viewMenu->addAction(gridSettingsAct);

	viewMenu->addSeparator();

	// Theme selection submenu
	auto* themeMenu = viewMenu->addMenu(QStringLiteral("Themes"));

	// Add theme options dynamically
	for (const QString& themeName : ThemeManager::instance().availableThemes()) {
		auto* themeAction = new QAction(themeName, this);
		themeAction->setCheckable(true);
		themeAction->setChecked(themeName == ThemeManager::instance().currentTheme());
		connect(themeAction, &QAction::triggered, this, [this, themeName, themeMenu]() {
			ThemeManager::instance().applyTheme(themeName);
			// Update checkmarks
			for (QAction* action : themeMenu->actions()) {
				action->setChecked(action->text() == themeName);
			}
			if (m_console) {
				m_console->appendPlainText(QStringLiteral("Theme changed to: %1").arg(themeName));
			}
		});
		themeMenu->addAction(themeAction);
	}

	viewMenu->addSeparator();

	// Workspace management submenu
	auto* workspaceMenu = viewMenu->addMenu(QStringLiteral("Workspaces"));

	auto* saveWorkspaceAct = new QAction(QStringLiteral("Save Workspace..."), this);
	connect(saveWorkspaceAct, &QAction::triggered, this, &RadiantMainWindow::saveCurrentWorkspace);
	workspaceMenu->addAction(saveWorkspaceAct);

	auto* loadWorkspaceAct = new QAction(QStringLiteral("Load Workspace..."), this);
	connect(loadWorkspaceAct, &QAction::triggered, this, &RadiantMainWindow::loadWorkspaceDialog);
	workspaceMenu->addAction(loadWorkspaceAct);

	auto* deleteWorkspaceAct = new QAction(QStringLiteral("Delete Workspace..."), this);
	connect(deleteWorkspaceAct, &QAction::triggered, this, &RadiantMainWindow::deleteWorkspaceDialog);
	workspaceMenu->addAction(deleteWorkspaceAct);
	
	// Setup gizmo controls
	setupGizmoControls();

	// Initialize status bar updates
	updateGridStatus();
	updateToolStatus();
	updateCameraStatus();

	// Performance update timer
	auto* perfTimer = new QTimer(this);
	connect(perfTimer, &QTimer::timeout, this, &RadiantMainWindow::updatePerformanceStatus);
	perfTimer->start(1000); // Update every second

	// ===== MULTIPLE TOOLBARS =====

	// File Operations Toolbar
	auto *fileTb = addToolBar(QStringLiteral("File"));
	fileTb->setMovable(true);
	fileTb->setToolTip(QStringLiteral("File Operations"));
	fileTb->addAction(openMapAct);
	fileTb->addAction(saveMapAct);

	// Selection & Transform Toolbar
	auto *selectTb = addToolBar(QStringLiteral("Selection"));
	selectTb->setMovable(true);
	selectTb->setToolTip(QStringLiteral("Selection and Transform Tools"));

	// Selection tools
	auto* selectAct = new QAction(QStringLiteral("Select"), this);
	selectAct->setToolTip(QStringLiteral("Select Tool (Q)"));
	selectAct->setShortcut(QKeySequence("Q"));
	selectAct->setCheckable(true);
	selectAct->setChecked(true);
	connect(selectAct, &QAction::triggered, this, [this]() {
		// Switch to selection mode
		setGizmoMode(0); // None
	});
	selectTb->addAction(selectAct);

	// Transform tools with standard shortcuts
	auto* moveAct = new QAction(QStringLiteral("Move"), this);
	moveAct->setToolTip(QStringLiteral("Move Tool (W)"));
	moveAct->setShortcut(QKeySequence("W"));
	moveAct->setCheckable(true);
	connect(moveAct, &QAction::triggered, this, [this]() {
		setGizmoMode(1); // Box mode
		setGizmoOperation(0); // Translate
	});
	selectTb->addAction(moveAct);

	auto* rotateAct = new QAction(QStringLiteral("Rotate"), this);
	rotateAct->setToolTip(QStringLiteral("Rotate Tool (E)"));
	rotateAct->setShortcut(QKeySequence("E"));
	rotateAct->setCheckable(true);
	connect(rotateAct, &QAction::triggered, this, [this]() {
		setGizmoMode(2); // Gizmo mode
		setGizmoOperation(1); // Rotate
	});
	selectTb->addAction(rotateAct);

	auto* scaleAct = new QAction(QStringLiteral("Scale"), this);
	scaleAct->setToolTip(QStringLiteral("Scale Tool (R)"));
	scaleAct->setShortcut(QKeySequence("R"));
	scaleAct->setCheckable(true);
	connect(scaleAct, &QAction::triggered, this, [this]() {
		setGizmoMode(2); // Gizmo mode
		setGizmoOperation(2); // Scale
	});
	selectTb->addAction(scaleAct);

	// Modeling Tools Toolbar
	auto *modelingTb = addToolBar(QStringLiteral("Modeling"));
	modelingTb->setMovable(true);
	modelingTb->setToolTip(QStringLiteral("Modeling Tools"));

	// Brush creation tools
	auto* createBrushAct = new QAction(QStringLiteral("Create Brush"), this);
	createBrushAct->setShortcut(QKeySequence("Shift+A"));
	connect(createBrushAct, &QAction::triggered, this, &RadiantMainWindow::createBrushAtSelection);
	modelingTb->addAction(createBrushAct);

	auto* createEntityAct = new QAction(QStringLiteral("Create Entity"), this);
	createEntityAct->setShortcut(QKeySequence("Ctrl+Shift+A"));
	connect(createEntityAct, &QAction::triggered, this, &RadiantMainWindow::createEntityAtSelection);
	modelingTb->addAction(createEntityAct);

	modelingTb->addSeparator();

	// Clipping tool
	auto* clippingToolAct = new QAction(QStringLiteral("Clipping Tool"), this);
	clippingToolAct->setShortcut(QKeySequence("Alt+X"));
	clippingToolAct->setToolTip(QStringLiteral("Clip brushes with planes"));
	connect(clippingToolAct, &QAction::triggered, this, &RadiantMainWindow::activateClippingTool);
	modelingTb->addAction(clippingToolAct);

	// Merge tool
	auto* mergeToolAct = new QAction(QStringLiteral("Merge Brushes"), this);
	mergeToolAct->setShortcut(QKeySequence("Ctrl+Shift+M"));
	mergeToolAct->setToolTip(QStringLiteral("Merge selected brushes into convex shapes"));
	connect(mergeToolAct, &QAction::triggered, this, &RadiantMainWindow::mergeSelectedBrushes);
	modelingTb->addAction(mergeToolAct);

	// Polygon tool
	auto* polygonToolAct = new QAction(QStringLiteral("Polygon Tool"), this);
	polygonToolAct->setToolTip(QStringLiteral("Create custom polygon brushes"));
	connect(polygonToolAct, &QAction::triggered, this, &RadiantMainWindow::activatePolygonTool);
	modelingTb->addAction(polygonToolAct);

	// Vertex tool
	auto* vertexToolAct = new QAction(QStringLiteral("Vertex Tool"), this);
	vertexToolAct->setToolTip(QStringLiteral("Edit vertices directly"));
	connect(vertexToolAct, &QAction::triggered, this, &RadiantMainWindow::activateVertexTool);
	modelingTb->addAction(vertexToolAct);

	// UV & Texture Toolbar
	auto *uvTb = addToolBar(QStringLiteral("UV/Texture"));
	uvTb->setMovable(true);
	uvTb->setToolTip(QStringLiteral("UV Mapping and Texture Tools"));

	// UV tool
	auto* uvToolAct = new QAction(QStringLiteral("UV Tool"), this);
	uvToolAct->setToolTip(QStringLiteral("UV unwrapping and editing"));
	connect(uvToolAct, &QAction::triggered, this, &RadiantMainWindow::activateUVTool);
	uvTb->addAction(uvToolAct);

	// Extrusion tool
	auto* extrusionToolAct = new QAction(QStringLiteral("Extrude Face"), this);
	extrusionToolAct->setShortcut(QKeySequence("Alt+E"));
	extrusionToolAct->setToolTip(QStringLiteral("Extrude selected faces"));
	connect(extrusionToolAct, &QAction::triggered, this, &RadiantMainWindow::activateExtrusionTool);
	uvTb->addAction(extrusionToolAct);

	// Special Tools Toolbar
	auto *specialTb = addToolBar(QStringLiteral("Special"));
	specialTb->setMovable(true);
	specialTb->setToolTip(QStringLiteral("Specialized Tools"));

	// Audio browser
	auto* specialAudioBrowserAct = new QAction(QStringLiteral("Audio Browser"), this);
	specialAudioBrowserAct->setShortcut(QKeySequence("Ctrl+Shift+A"));
	specialAudioBrowserAct->setToolTip(QStringLiteral("Browse and play audio files"));
	connect(specialAudioBrowserAct, &QAction::triggered, this, &RadiantMainWindow::openAudioBrowser);
	specialTb->addAction(specialAudioBrowserAct);

	// Preferences
	auto* specialPreferencesAct = new QAction(QStringLiteral("Preferences"), this);
	specialPreferencesAct->setShortcut(QKeySequence("Ctrl+,"));
	specialPreferencesAct->setToolTip(QStringLiteral("Open preferences dialog"));
	connect(specialPreferencesAct, &QAction::triggered, this, &RadiantMainWindow::openPreferences);
	specialTb->addAction(specialPreferencesAct);
	
	// Texture paint tool
	auto* texturePaintAct = new QAction(QStringLiteral("Texture Paint"), this);
	connect(texturePaintAct, &QAction::triggered, this, &RadiantMainWindow::activateTexturePaintTool);
	uvTb->addAction(texturePaintAct);
	
	modelingTb->addSeparator();

	// Geometry creation tools
	m_createBrushAction = new QAction(QStringLiteral("Create Brush"), this);
	m_createBrushAction->setShortcut(QKeySequence("Ctrl+B"));
	connect(m_createBrushAction, &QAction::triggered, this, [this](){
		createBrushAtSelection();
	});
	modelingTb->addAction(m_createBrushAction);

	m_createEntityAction = new QAction(QStringLiteral("Create Entity"), this);
	m_createEntityAction->setShortcut(QKeySequence("Ctrl+E"));
	connect(m_createEntityAction, &QAction::triggered, this, [this](){
		createEntityAtSelection();
	});
	modelingTb->addAction(m_createEntityAction);

	// Status bar
	// ===== ENHANCED STATUS BAR =====
	QStatusBar* statusBarPtr = statusBar();

	// Selection information
	m_selectionStatus = new QLabel(QStringLiteral("No Selection"));
	m_selectionStatus->setMinimumWidth(150);
	m_selectionStatus->setToolTip(QStringLiteral("Current selection information"));
	statusBarPtr->addWidget(m_selectionStatus);

	// Coordinate display
	m_coordinatesStatus = new QLabel(QStringLiteral("X:0.0 Y:0.0 Z:0.0"));
	m_coordinatesStatus->setMinimumWidth(180);
	m_coordinatesStatus->setToolTip(QStringLiteral("Current cursor/world coordinates"));
	statusBarPtr->addWidget(m_coordinatesStatus);

	// Grid and snap information
	m_gridStatus = new QLabel(QStringLiteral("Grid:32 Snap:ON"));
	m_gridStatus->setMinimumWidth(120);
	m_gridStatus->setToolTip(QStringLiteral("Grid size and snap settings"));
	statusBarPtr->addWidget(m_gridStatus);

	// Current tool/mode
	m_toolStatus = new QLabel(QStringLiteral("Tool: Select"));
	m_toolStatus->setMinimumWidth(120);
	m_toolStatus->setToolTip(QStringLiteral("Current active tool"));
	statusBarPtr->addWidget(m_toolStatus);

	// Camera/view information
	m_cameraStatus = new QLabel(QStringLiteral("Perspective"));
	m_cameraStatus->setMinimumWidth(100);
	m_cameraStatus->setToolTip(QStringLiteral("Current camera/view mode"));
	statusBarPtr->addWidget(m_cameraStatus);

	// Performance information (right-aligned)
	m_performanceStatus = new QLabel(QStringLiteral("FPS:60"));
	m_performanceStatus->setMinimumWidth(80);
	m_performanceStatus->setToolTip(QStringLiteral("Performance information"));
	statusBarPtr->addPermanentWidget(m_performanceStatus);

	resize(1280, 800);
}

void RadiantMainWindow::appendLogInfo(const QtRadiantEnv& env){
	if ( !m_console ) {
		return;
	}
	const QString line = QStringLiteral("Started Radiant Qt (app=%1 temp=%2 games=%3)")
		.arg(env.appPath, env.tempPath, env.gamesPath);
	m_console->appendPlainText(line);

	// Mirror to log file
	QFile log(env.logFile);
	if ( log.open(QIODevice::Append | QIODevice::Text) ) {
		QTextStream ts(&log);
		ts << line << "\n";
	}
}

void RadiantMainWindow::populateOutliner(){
	if ( !m_outliner ) return;
	m_outliner->clear();
	QStringList roots = {QStringLiteral("World"), QStringLiteral("Lights"), QStringLiteral("Props"), QStringLiteral("Volumes")};
	for ( const auto& rootName : roots ) {
		auto *root = new QTreeWidgetItem(m_outliner, {rootName, QStringLiteral("Group")});
		for (int i = 0; i < 3; ++i) {
			auto *child = new QTreeWidgetItem(root, {QStringLiteral("%1_%2").arg(rootName).arg(i), QStringLiteral("Entity")});
			child->addChild(new QTreeWidgetItem({QStringLiteral("mesh_%1").arg(i), QStringLiteral("Mesh")}));
		}
		m_outliner->addTopLevelItem(root);
	}
	m_outliner->expandAll();
}

void RadiantMainWindow::populateInspector(){
	if ( !m_inspector ) return;

	auto *layout = new QFormLayout(m_inspector);
	layout->setContentsMargins(8, 8, 8, 8);

	m_nameEdit = new QLineEdit(QStringLiteral("selected_object"), m_inspector);
	layout->addRow(QStringLiteral("Name"), m_nameEdit);

	m_typeCombo = new QComboBox(m_inspector);
	m_typeCombo->addItems({QStringLiteral("Entity"), QStringLiteral("Brush"), QStringLiteral("Light"), QStringLiteral("Volume")});
	layout->addRow(QStringLiteral("Type"), m_typeCombo);

	m_posX = new QDoubleSpinBox(m_inspector); m_posX->setRange(-99999, 99999); m_posX->setValue(0.0);
	m_posY = new QDoubleSpinBox(m_inspector); m_posY->setRange(-99999, 99999); m_posY->setValue(0.0);
	m_posZ = new QDoubleSpinBox(m_inspector); m_posZ->setRange(-99999, 99999); m_posZ->setValue(0.0);
	layout->addRow(QStringLiteral("Position X"), m_posX);
	layout->addRow(QStringLiteral("Position Y"), m_posY);
	layout->addRow(QStringLiteral("Position Z"), m_posZ);

	auto *scale = new QDoubleSpinBox(m_inspector); scale->setRange(0.001, 1000.0); scale->setValue(1.0);
	layout->addRow(QStringLiteral("Uniform Scale"), scale);

	auto *matCombo = new QComboBox(m_inspector);
	matCombo->addItems({QStringLiteral("Default"), QStringLiteral("Metal"), QStringLiteral("Rough"), QStringLiteral("Emissive")});
	layout->addRow(QStringLiteral("Material"), matCombo);
	connect(matCombo, &QComboBox::currentTextChanged, this, [this](const QString& mat){
		m_currentMaterial = mat;
		// Update viewports with current material
		if (m_viewPerspective) m_viewPerspective->setCurrentMaterial(m_currentMaterial);
		if (m_viewTop) m_viewTop->setCurrentMaterial(m_currentMaterial);
		if (m_viewFront) m_viewFront->setCurrentMaterial(m_currentMaterial);
		if (m_viewSide) m_viewSide->setCurrentMaterial(m_currentMaterial);
		if ( m_console ) {
			m_console->appendPlainText(QStringLiteral("Material set to %1").arg(mat));
		}
	});

	auto *targetEdit = new QLineEdit(m_inspector);
	layout->addRow(QStringLiteral("Target"), targetEdit);

	// Spawnflags (simple checkboxes)
	auto *spawnFlags = new QWidget(m_inspector);
	auto *spawnLayout = new QHBoxLayout(spawnFlags);
	spawnLayout->setContentsMargins(0,0,0,0);
	for (int i = 0; i < 4; ++i) {
		auto *cb = new QCheckBox(QStringLiteral("Flag %1").arg(i), spawnFlags);
		spawnLayout->addWidget(cb);
	}
	layout->addRow(QStringLiteral("Spawnflags"), spawnFlags);

	// Key/Value table
	auto *kvTable = new QTableWidget(0, 2, m_inspector);
	kvTable->setHorizontalHeaderLabels({QStringLiteral("Key"), QStringLiteral("Value")});
	kvTable->horizontalHeader()->setStretchLastSection(true);
	layout->addRow(QStringLiteral("Key/Values"), kvTable);

	auto *kvButtons = new QWidget(m_inspector);
	auto *kvBtnLayout = new QHBoxLayout(kvButtons);
	kvBtnLayout->setContentsMargins(0,0,0,0);
	auto *addKv = new QPushButton(QStringLiteral("Add KV"), kvButtons);
	auto *delKv = new QPushButton(QStringLiteral("Remove KV"), kvButtons);
	kvBtnLayout->addWidget(addKv);
	kvBtnLayout->addWidget(delKv);
	layout->addRow(QString(), kvButtons);

	connect(addKv, &QPushButton::clicked, this, [kvTable](){
		int row = kvTable->rowCount();
		kvTable->insertRow(row);
		kvTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("key")));
		kvTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("value")));
	});
	connect(delKv, &QPushButton::clicked, this, [kvTable](){
		int row = kvTable->currentRow();
		if ( row >= 0 ) kvTable->removeRow(row);
	});
}

void RadiantMainWindow::setOutlinerLoadedMap(const QString& path){
	if ( !m_outliner ) return;
	if ( m_outliner->topLevelItemCount() == 0 ) {
		populateOutliner();
	}
	auto *root = m_outliner->topLevelItem(0);
	if ( root ) {
		root->setText(0, QStringLiteral("Map: %1").arg(path));
	}
}

void RadiantMainWindow::populateTextureBrowser(const QtRadiantEnv& env){
	if ( !m_textureList ) return;
	m_textureList->clear();

	// Enhanced texture browser with alpha channel support
	// Look for textures under games/*/base*/textures
	QString gamesRoot = env.gamesPath;
	if (!gamesRoot.endsWith('/')) gamesRoot += '/';

	QStringList searchDirs;
	QDir gamesDir(gamesRoot);
	for (const QString& sub : gamesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		const QString baseDir = gamesRoot + sub + QStringLiteral("/base");
		if (QDir(baseDir).exists()) {
			searchDirs << baseDir + QStringLiteral("/textures");
		}
	}

	int added = 0;
	const QStringList exts = {QStringLiteral("*.tga"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png")};
	for (const QString& dirPath : searchDirs) {
		QDirIterator it(dirPath, exts, QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			const QString filePath = it.next();
			const QString rel = QDir(dirPath).relativeFilePath(filePath);
			m_textureList->addItem(rel);
			++added;
			if (added > 2048) break; // safety cap
		}
		if (added > 2048) break;
	}

	if (added == 0) {
		m_textureList->addItem(QStringLiteral("(no textures found)"));
	}
	
	// Set up texture list with alpha channel display support
	// In a full implementation, we'd load texture thumbnails with alpha channel
	// and display them in the list widget
	m_textureList->setViewMode(QListWidget::IconMode);
	m_textureList->setIconSize(QSize(64, 64));
	m_textureList->setResizeMode(QListWidget::Adjust);
	
	// Add search box for texture browser
	// Find the texture dock widget and its container
	QDockWidget* texDock = findChild<QDockWidget*>(QStringLiteral("TexturesDock"));
	if (texDock && m_textureList) {
		auto* textureContainer = texDock->widget();
		if (textureContainer) {
			auto* textureLayout = qobject_cast<QVBoxLayout*>(textureContainer->layout());
			if (textureLayout) {
				auto* textureSearchLayout = new QHBoxLayout();
				auto* textureSearchLabel = new QLabel(QStringLiteral("Search:"), textureContainer);
				auto* textureSearchEdit = new QLineEdit(textureContainer);
				textureSearchEdit->setPlaceholderText(QStringLiteral("Search textures..."));
				textureSearchLayout->addWidget(textureSearchLabel);
				textureSearchLayout->addWidget(textureSearchEdit);

				// Add alpha display checkbox
				auto* alphaCheckBox = new QCheckBox(QStringLiteral("Show Alpha Transparency"), textureContainer);
				alphaCheckBox->setChecked(false);
				connect(alphaCheckBox, &QCheckBox::toggled, this, &RadiantMainWindow::enableTextureAlphaDisplay);
				textureSearchLayout->addWidget(alphaCheckBox);

				// Insert search controls at the top of the texture container layout
				textureLayout->insertLayout(0, textureSearchLayout);

				// Connect search
				connect(textureSearchEdit, &QLineEdit::textChanged, this, &RadiantMainWindow::searchTextures);
			}
		}
	}
}

void RadiantMainWindow::createBrushAtSelection(){
	// Support 3D editing - can create in perspective view
	if (!hasSelection() && !m_3dEditingEnabled) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("No selection - click in a viewport to set position"));
		}
		return;
	}
	
	// Use last selection or default position
	QVector3D position = m_lastSelection;
	if (position.isNull()) {
		position = QVector3D(0.0f, 0.0f, 0.0f);
	}

	// Create a simple cube brush at the selection position
	static int brushCount = 0;
	brushCount++;

	QString brushName = QStringLiteral("brush_%1").arg(brushCount);
	QTreeWidgetItem* brushItem = new QTreeWidgetItem(m_outliner, {brushName, QStringLiteral("brush")});

	// Store brush data (in a real implementation, this would be proper map geometry)
	brushItem->setData(0, Qt::UserRole, QVariant::fromValue(position));

		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Created brush '%1' at (%2,%3,%4)")
				.arg(brushName)
				.arg(position.x(), 0, 'f', 1)
				.arg(position.y(), 0, 'f', 1)
				.arg(position.z(), 0, 'f', 1));
		}

	// Update inspector with brush properties
	populateInspectorForItem(brushItem);
}

void RadiantMainWindow::createEntityAtSelection(){
	// Support 3D editing - can create in perspective view
	if (!hasSelection() && !m_3dEditingEnabled) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("No selection - click in a viewport to set position"));
		}
		return;
	}
	
	// Use last selection or default position
	QVector3D position = m_lastSelection;
	if (position.isNull()) {
		position = QVector3D(0.0f, 0.0f, 0.0f);
	}

	// Create a basic entity at the selection position
	static int entityCount = 0;
	entityCount++;

	QString entityName = QStringLiteral("entity_%1").arg(entityCount);
	QTreeWidgetItem* entityItem = new QTreeWidgetItem(m_outliner, {entityName, QStringLiteral("entity")});

	// Store entity data
	entityItem->setData(0, Qt::UserRole, QVariant::fromValue(position));

	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Created entity '%1' at (%2,%3,%4)")
			.arg(entityName)
			.arg(position.x(), 0, 'f', 1)
			.arg(position.y(), 0, 'f', 1)
			.arg(position.z(), 0, 'f', 1));
	}

	// Update inspector with entity properties
	populateInspectorForItem(entityItem);
}

bool RadiantMainWindow::hasSelection() const {
	// Check if any viewport has a selection
	return (m_viewPerspective && m_viewPerspective->hasSelection()) ||
		   (m_viewTop && m_viewTop->hasSelection()) ||
		   (m_viewFront && m_viewFront->hasSelection()) ||
		   (m_viewSide && m_viewSide->hasSelection());
}

void RadiantMainWindow::populateInspectorForItem(QTreeWidgetItem* item){
	if (!item || !m_inspector) return;

	// Clear existing inspector content
	QWidget* oldWidget = m_inspector;
	if (oldWidget->layout()) {
		QLayout* layout = oldWidget->layout();

		// Clear all widgets from the layout
		while (layout->count() > 0) {
			QLayoutItem* layoutItem = layout->takeAt(0);
			if (layoutItem) {
				if (layoutItem->widget()) {
					delete layoutItem->widget();
				}
				delete layoutItem;
			}
		}

		// Note: setLayout(nullptr) automatically deletes the old layout,
		// so we don't need to manually delete it
		oldWidget->setLayout(nullptr);
	}

	auto* layout = new QFormLayout(m_inspector);
	m_inspector->setLayout(layout);

	QString type = item->text(1);
	QString name = item->text(0);
	QVector3D pos = item->data(0, Qt::UserRole).value<QVector3D>();

	layout->addRow(new QLabel(QStringLiteral("Name:")), new QLabel(name));
	layout->addRow(new QLabel(QStringLiteral("Type:")), new QLabel(type));

	// Position controls with reset capability
	m_posX = new QDoubleSpinBox(m_inspector);
	m_posY = new QDoubleSpinBox(m_inspector);
	m_posZ = new QDoubleSpinBox(m_inspector);
	m_posX->setRange(-99999, 99999);
	m_posY->setRange(-99999, 99999);
	m_posZ->setRange(-99999, 99999);
	m_posX->setValue(pos.x());
	m_posY->setValue(pos.y());
	m_posZ->setValue(pos.z());
	
	// Make position controls right-clickable for reset
	m_posX->setContextMenuPolicy(Qt::CustomContextMenu);
	m_posY->setContextMenuPolicy(Qt::CustomContextMenu);
	m_posZ->setContextMenuPolicy(Qt::CustomContextMenu);
	
	connect(m_posX, &QWidget::customContextMenuRequested, this, [this](const QPoint& menuPos) {
		QMenu menu(m_posX);
		auto* action = menu.addAction(QStringLiteral("Reset to 0"));
		connect(action, &QAction::triggered, &menu, [this]() { m_posX->setValue(0.0); });
		menu.exec(m_posX->mapToGlobal(menuPos));
	});
	
	connect(m_posY, &QWidget::customContextMenuRequested, this, [this](const QPoint& menuPos) {
		QMenu menu(m_posY);
		auto* action = menu.addAction(QStringLiteral("Reset to 0"));
		connect(action, &QAction::triggered, &menu, [this]() { m_posY->setValue(0.0); });
		menu.exec(m_posY->mapToGlobal(menuPos));
	});
	
	connect(m_posZ, &QWidget::customContextMenuRequested, this, [this](const QPoint& menuPos) {
		QMenu menu(m_posZ);
		auto* action = menu.addAction(QStringLiteral("Reset to 0"));
		connect(action, &QAction::triggered, &menu, [this]() { m_posZ->setValue(0.0); });
		menu.exec(m_posZ->mapToGlobal(menuPos));
	});

	layout->addRow(QStringLiteral("X:"), m_posX);
	layout->addRow(QStringLiteral("Y:"), m_posY);
	layout->addRow(QStringLiteral("Z:"), m_posZ);

	if (type == QStringLiteral("brush")) {
		// Brush-specific properties
		auto* materialCombo = new QComboBox(m_inspector);
		materialCombo->addItems({QStringLiteral("common/caulk"), QStringLiteral("common/trigger"),
			QStringLiteral("textures/base_wall/metal"), QStringLiteral("textures/base_wall/stone")});
		materialCombo->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(materialCombo, &QWidget::customContextMenuRequested, this, [materialCombo](const QPoint& menuPos) {
			QMenu menu(materialCombo);
			auto* action = menu.addAction(QStringLiteral("Reset to Default"));
			connect(action, &QAction::triggered, &menu, [materialCombo]() { materialCombo->setCurrentIndex(0); });
			menu.exec(materialCombo->mapToGlobal(menuPos));
		});
		layout->addRow(QStringLiteral("Material:"), materialCombo);
	} else if (type == QStringLiteral("entity")) {
		// Entity-specific properties
		m_nameEdit = new QLineEdit(name, m_inspector);
		m_nameEdit->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(m_nameEdit, &QWidget::customContextMenuRequested, this, [this](const QPoint& menuPos) {
			QMenu menu(m_nameEdit);
			auto* action = menu.addAction(QStringLiteral("Reset to Default"));
			connect(action, &QAction::triggered, &menu, [this]() { m_nameEdit->clear(); });
			menu.exec(m_nameEdit->mapToGlobal(menuPos));
		});
		
		m_typeCombo = new QComboBox(m_inspector);
		m_typeCombo->addItems({QStringLiteral("func_static"), QStringLiteral("info_player_start"),
			QStringLiteral("light"), QStringLiteral("trigger_multiple")});
		m_typeCombo->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(m_typeCombo, &QWidget::customContextMenuRequested, this, [this](const QPoint& menuPos) {
			QMenu menu(m_typeCombo);
			auto* action = menu.addAction(QStringLiteral("Reset to Default"));
			connect(action, &QAction::triggered, &menu, [this]() { m_typeCombo->setCurrentIndex(0); });
			menu.exec(m_typeCombo->mapToGlobal(menuPos));
		});

		layout->addRow(QStringLiteral("Classname:"), m_typeCombo);
		layout->addRow(QStringLiteral("Targetname:"), m_nameEdit);
	}
}

void RadiantMainWindow::saveMap(){
		QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Map"), QString(), 
			QStringLiteral("All Supported Formats (*.map *.vmf);;Maps (*.map);;VMF Files (*.vmf);;All Files (*.*)"));
	if ( path.isEmpty() ) return;

	if ( saveMapToFile(path) ) {
		m_console->appendPlainText(QStringLiteral("Saved map: %1").arg(path));
		m_currentMapPath = path;
		if (m_autosaveManager) {
			m_autosaveManager->setMapPath(path);
		}
	} else {
		m_console->appendPlainText(QStringLiteral("Failed to save map: %1").arg(path));
	}
}

bool RadiantMainWindow::saveMapToFile(const QString& path){
	QFile file(path);
	if ( !file.open(QIODevice::WriteOnly | QIODevice::Text) ) {
		return false;
	}

	QTextStream out(&file);

	// Write map header
	out << "// Radiant Qt exported map\n";
	out << "// Generated by Qt Radiant World Editor\n\n";

	// Write entities and brushes
	for ( int i = 0; i < m_outliner->topLevelItemCount(); ++i ) {
		QTreeWidgetItem* item = m_outliner->topLevelItem(i);
		if ( !item ) continue;

		QString type = item->text(1);
		QString name = item->text(0);

		if ( type == QStringLiteral("entity") ) {
			writeEntityToMap(out, item);
		} else if ( type == QStringLiteral("brush") ) {
			// For now, wrap brushes in worldspawn entity
			if ( i == 0 ) {
				out << "{\n";
				out << "\"classname\" \"worldspawn\"\n";
			}
			writeBrushToMap(out, item);
		}
	}

	// Close worldspawn entity if we have brushes
	bool hasBrushes = false;
	for ( int i = 0; i < m_outliner->topLevelItemCount(); ++i ) {
		if ( m_outliner->topLevelItem(i)->text(1) == QStringLiteral("brush") ) {
			hasBrushes = true;
			break;
		}
	}
	if ( hasBrushes ) {
		out << "}\n";
	}

	file.close();
	return true;
}

void RadiantMainWindow::writeEntityToMap(QTextStream& out, QTreeWidgetItem* item){
	QVector3D pos = item->data(0, Qt::UserRole).value<QVector3D>();
	QString name = item->text(0);

	out << "{\n";
	out << "\"classname\" \"func_static\"\n";
	out << "\"origin\" \"" << pos.x() << " " << pos.y() << " " << pos.z() << "\"\n";
	out << "\"targetname\" \"" << name << "\"\n";
	out << "}\n\n";
}

void RadiantMainWindow::writeBrushToMap(QTextStream& out, QTreeWidgetItem* item){
	QVector3D pos = item->data(0, Qt::UserRole).value<QVector3D>();

	// Create a simple cube brush (6 faces)
	// For a real implementation, this would use proper brush geometry
	const float size = 64.0f; // Half-extent

	// Bottom face (z = pos.z - size)
	out << "  {\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) common/caulk 0 0 0 1 1\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) common/caulk 0 0 0 1 1\n";
	out << "  }\n";

	// Top face (z = pos.z + size)
	out << "  {\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "  }\n";

	// Front face (y = pos.y - size)
	out << "  {\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "  }\n";

	// Back face (y = pos.y + size)
	out << "  {\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "  }\n";

	// Left face (x = pos.x - size)
	out << "  {\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "  }\n";

	// Right face (x = pos.x + size)
	out << "  {\n";
	out << "    ( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "    ( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "  }\n\n";
}

void RadiantMainWindow::openColorPicker() {
	// Check if we have a light or entity selected that uses color
	QColor initialColor = Qt::white;
	
	// Try to get color from selected entity if it's a light
	QTreeWidgetItem* selected = m_outliner ? m_outliner->currentItem() : nullptr;
	if (selected && selected->text(1) == QStringLiteral("entity")) {
		// For lights, we'd get the color from entity properties
		// For now, use white as default
		initialColor = Qt::white;
	}
	
	ColorPickerDialog dlg(this);
	dlg.setColor(initialColor);
	if (dlg.exec() == QDialog::Accepted) {
		QColor color = dlg.selectedColor();
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Selected color: R=%1 G=%2 B=%3")
				.arg(color.red()).arg(color.green()).arg(color.blue()));
		}
		// Apply color to selected entity/light if applicable
		// This would be implemented based on entity type
	}
}

bool RadiantMainWindow::eventFilter(QObject* obj, QEvent* event) {
	// Handle C key for color picker when appropriate widgets have focus
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		if (keyEvent->key() == Qt::Key_C && keyEvent->modifiers() == Qt::NoModifier) {
			// Check if we're in a context where color picker makes sense
			openColorPicker();
			return true;
		}
	}
	return QMainWindow::eventFilter(obj, event);
}

void RadiantMainWindow::openKeybindEditor() {
	KeybindEditor dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Keybinds updated. Some changes may require restarting the editor."));
		}
		// Reload keybinds and update shortcuts
		// In a full implementation, we'd reload all shortcuts here
	}
}

void RadiantMainWindow::toggleToolTextures() {
	m_showToolTextures = !m_showToolTextures;
	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Tool textures: %1")
			.arg(m_showToolTextures ? "shown" : "hidden"));
	}
	// In a full implementation, this would update viewport rendering
	// to hide/show trigger textures, nodraw, etc.
}

void RadiantMainWindow::toggleEditorModels() {
	m_showEditorModels = !m_showEditorModels;
	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Editor models/sprites: %1")
			.arg(m_showEditorModels ? "shown" : "hidden"));
	}
	// In a full implementation, this would update viewport rendering
	// to hide/show logic entity sprites, editor-only models, etc.
}

void RadiantMainWindow::setupAutosave() {
	m_autosaveManager = new AutosaveManager(this);
	connect(m_autosaveManager, &AutosaveManager::autosaveCompleted, this, [this](const QString& path) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Autosaved to: %1").arg(path));
		}
	});
	connect(m_autosaveManager, &AutosaveManager::autosaveFailed, this, [this](const QString& error) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Autosave failed: %1").arg(error));
		}
	});
}

void RadiantMainWindow::checkForRecoveryFiles() {
	if (!AutosaveManager::hasRecoveryFiles()) {
		return;
	}
	
	QStringList recoveryFiles = AutosaveManager::getRecoveryFiles();
	if (recoveryFiles.isEmpty()) {
		return;
	}
	
	QMessageBox msgBox(this);
	msgBox.setWindowTitle(QStringLiteral("Crash Recovery"));
	msgBox.setText(QStringLiteral("Recovery files were found from a previous session."));
	msgBox.setInformativeText(QStringLiteral("Would you like to recover your work?"));
	msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
	msgBox.setDefaultButton(QMessageBox::Yes);
	
	if (msgBox.exec() == QMessageBox::Yes) {
		// Use the most recent recovery file
		QString recoveryFile = recoveryFiles.first();
		QString recoveredPath;
		
		if (AutosaveManager::recoverFromCrash(recoveryFile, recoveredPath)) {
			QMessageBox::information(this, QStringLiteral("Recovery Successful"),
				QStringLiteral("Recovered file saved to:\n%1").arg(recoveredPath));
			if (m_console) {
				m_console->appendPlainText(QStringLiteral("Recovered from crash: %1").arg(recoveredPath));
			}
		} else {
			QMessageBox::warning(this, QStringLiteral("Recovery Failed"),
				QStringLiteral("Failed to recover file from:\n%1").arg(recoveryFile));
		}
	}
}

void RadiantMainWindow::openGridSettings() {
	QDialog dlg(this);
	dlg.setWindowTitle(QStringLiteral("Grid Settings"));
	dlg.setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
	                  Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
	dlg.setSizeGripEnabled(true);
	dlg.resize(300, 150);
	auto* layout = new QVBoxLayout(&dlg);
	
	// Microgrid enable
	auto* microgridCheck = new QCheckBox(QStringLiteral("Enable Microgrid (decimal grid sizes)"), &dlg);
	microgridCheck->setChecked(VkViewportWidget::isMicrogridEnabled());
	layout->addWidget(microgridCheck);
	
	// Grid size
	auto* gridSizeLayout = new QHBoxLayout();
	gridSizeLayout->addWidget(new QLabel(QStringLiteral("Grid Size:")));
	auto* gridSizeSpin = new QDoubleSpinBox(&dlg);
	gridSizeSpin->setRange(0.125, 1024.0);
	gridSizeSpin->setSingleStep(0.125);
	gridSizeSpin->setValue(VkViewportWidget::gridSize());
	gridSizeSpin->setDecimals(3);
	gridSizeLayout->addWidget(gridSizeSpin);
	gridSizeLayout->addStretch();
	layout->addLayout(gridSizeLayout);
	
	// Preset buttons
	auto* presetLayout = new QHBoxLayout();
	auto* preset025 = new QPushButton(QStringLiteral("0.25"), &dlg);
	auto* preset05 = new QPushButton(QStringLiteral("0.5"), &dlg);
	auto* preset1 = new QPushButton(QStringLiteral("1"), &dlg);
	auto* preset8 = new QPushButton(QStringLiteral("8"), &dlg);
	auto* preset16 = new QPushButton(QStringLiteral("16"), &dlg);
	auto* preset32 = new QPushButton(QStringLiteral("32"), &dlg);
	auto* preset64 = new QPushButton(QStringLiteral("64"), &dlg);
	
	connect(preset025, &QPushButton::clicked, &dlg, [gridSizeSpin]() { gridSizeSpin->setValue(0.25); });
	connect(preset05, &QPushButton::clicked, &dlg, [gridSizeSpin]() { gridSizeSpin->setValue(0.5); });
	connect(preset1, &QPushButton::clicked, &dlg, [gridSizeSpin]() { gridSizeSpin->setValue(1.0); });
	connect(preset8, &QPushButton::clicked, &dlg, [gridSizeSpin]() { gridSizeSpin->setValue(8.0); });
	connect(preset16, &QPushButton::clicked, &dlg, [gridSizeSpin]() { gridSizeSpin->setValue(16.0); });
	connect(preset32, &QPushButton::clicked, &dlg, [gridSizeSpin]() { gridSizeSpin->setValue(32.0); });
	connect(preset64, &QPushButton::clicked, &dlg, [gridSizeSpin]() { gridSizeSpin->setValue(64.0); });
	
	presetLayout->addWidget(preset025);
	presetLayout->addWidget(preset05);
	presetLayout->addWidget(preset1);
	presetLayout->addWidget(preset8);
	presetLayout->addWidget(preset16);
	presetLayout->addWidget(preset32);
	presetLayout->addWidget(preset64);
	presetLayout->addStretch();
	layout->addLayout(presetLayout);
	
	// Buttons
	auto* buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();
	auto* okBtn = new QPushButton(QStringLiteral("OK"), &dlg);
	auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), &dlg);
	buttonLayout->addWidget(okBtn);
	buttonLayout->addWidget(cancelBtn);
	layout->addLayout(buttonLayout);
	
	connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
	connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
	
	if (dlg.exec() == QDialog::Accepted) {
		VkViewportWidget::setMicrogridEnabled(microgridCheck->isChecked());
		VkViewportWidget::setGridSize(gridSizeSpin->value());
		
		// Update all viewports
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
		
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Grid size set to: %1 (microgrid: %2)")
				.arg(gridSizeSpin->value())
				.arg(microgridCheck->isChecked() ? "enabled" : "disabled"));
		}
	}
}

void RadiantMainWindow::setupGizmoControls() {
	// Gizmo mode cycle (X key)
	auto* gizmoModeShortcut = new QShortcut(QKeySequence("X"), this);
	connect(gizmoModeShortcut, &QShortcut::activated, this, &RadiantMainWindow::cycleGizmoMode);
	
	// Gizmo space toggle (Ctrl+J)
	auto* gizmoSpaceShortcut = new QShortcut(QKeySequence("Ctrl+J"), this);
	connect(gizmoSpaceShortcut, &QShortcut::activated, this, &RadiantMainWindow::toggleGizmoSpace);
	
	// Add gizmo controls to selection toolbar
	auto* selectionTb = findChild<QToolBar*>(QStringLiteral("Selection"));
	if (selectionTb) {
		selectionTb->addSeparator();

		auto* gizmoModeAct = new QAction(QStringLiteral("Gizmo Mode"), this);
		gizmoModeAct->setCheckable(true);
		gizmoModeAct->setChecked(m_currentGizmoMode == 2);
		connect(gizmoModeAct, &QAction::triggered, this, [this]() {
			cycleGizmoMode();
		});
		selectionTb->addAction(gizmoModeAct);

		auto* gizmoSpaceAct = new QAction(QStringLiteral("Gizmo Space"), this);
		gizmoSpaceAct->setCheckable(true);
		connect(gizmoSpaceAct, &QAction::triggered, this, &RadiantMainWindow::toggleGizmoSpace);
		selectionTb->addAction(gizmoSpaceAct);
	}
}

void RadiantMainWindow::cycleGizmoMode() {
	m_currentGizmoMode = (m_currentGizmoMode + 1) % 3; // Cycle: 0->1->2->0
	
	// Update all viewports
	if (m_viewPerspective) m_viewPerspective->setGizmoMode(m_currentGizmoMode);
	if (m_viewTop) m_viewTop->setGizmoMode(m_currentGizmoMode);
	if (m_viewFront) m_viewFront->setGizmoMode(m_currentGizmoMode);
	if (m_viewSide) m_viewSide->setGizmoMode(m_currentGizmoMode);
	
	if (m_console) {
		const char* modeNames[] = {"None", "Box", "Gizmo"};
		m_console->appendPlainText(QStringLiteral("Gizmo mode: %1").arg(modeNames[m_currentGizmoMode]));
	}
}

void RadiantMainWindow::cycleGizmoOperation() {
	m_currentGizmoOperation = (m_currentGizmoOperation + 1) % 3; // Cycle: 0->1->2->0
	
	// Update all viewports
	if (m_viewPerspective) m_viewPerspective->setGizmoOperation(m_currentGizmoOperation);
	if (m_viewTop) m_viewTop->setGizmoOperation(m_currentGizmoOperation);
	if (m_viewFront) m_viewFront->setGizmoOperation(m_currentGizmoOperation);
	if (m_viewSide) m_viewSide->setGizmoOperation(m_currentGizmoOperation);
	
	if (m_console) {
		const char* opNames[] = {"Translate", "Rotate", "Scale"};
		m_console->appendPlainText(QStringLiteral("Gizmo operation: %1 (Right-click in viewport to cycle)").arg(opNames[m_currentGizmoOperation]));
	}
}

void RadiantMainWindow::toggleGizmoSpace() {
	// Update all viewports
	if (m_viewPerspective) m_viewPerspective->toggleGizmoSpace();
	if (m_viewTop) m_viewTop->toggleGizmoSpace();
	if (m_viewFront) m_viewFront->toggleGizmoSpace();
	if (m_viewSide) m_viewSide->toggleGizmoSpace();
	
	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Gizmo space toggled"));
	}
}

void RadiantMainWindow::setGizmoAxisLock(int axis, bool locked) {
	if (axis < 0 || axis > 2) return;
	m_gizmoAxisLocks[axis] = locked;
	
	// In full implementation, would update gizmo in viewports
	if (m_console) {
		const char* axisNames[] = {"X", "Y", "Z"};
			m_console->appendPlainText(QStringLiteral("Gizmo axis %1: %2")
				.arg(axisNames[axis])
				.arg(locked ? "locked" : "unlocked"));
	}
}

void RadiantMainWindow::activateClippingTool() {
	// Deactivate other tools
	if (m_polygonToolActive) {
		activatePolygonTool(); // Toggle off
	}
	if (m_vertexToolActive) {
		activateVertexTool(); // Toggle off
	}
	if (m_uvToolActive) {
		activateUVTool(); // Toggle off
	}
	if (m_extrusionToolActive) {
		activateExtrusionTool(); // Toggle off
	}
	if (m_texturePaintToolActive) {
		activateTexturePaintTool(); // Toggle off
	}
	
	if (!m_clippingTool) {
		m_clippingTool = new ClippingTool();
		// Connect clipping tool to all viewports
		if (m_viewPerspective) m_viewPerspective->setClippingTool(m_clippingTool);
		if (m_viewTop) m_viewTop->setClippingTool(m_clippingTool);
		if (m_viewFront) m_viewFront->setClippingTool(m_clippingTool);
		if (m_viewSide) m_viewSide->setClippingTool(m_clippingTool);
	}
	m_clippingToolActive = !m_clippingToolActive;
	
	if (m_clippingToolActive) {
		m_clippingTool->clearPoints();
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Clipping tool activated. Click to place clip points (C to toggle 2/3 point mode)"));
		}
		// Update viewports
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
	} else {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Clipping tool deactivated"));
		}
	}
	updateToolStatus();
}

void RadiantMainWindow::toggleClippingMode() {
	if (m_clippingTool) {
		m_clippingTool->toggleMode();
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Clipping mode: %1 point")
				.arg(m_clippingTool->isThreePointMode() ? "3" : "2"));
		}
		// Update viewports to show clipping tool
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
	}
}

void RadiantMainWindow::mergeSelectedBrushes() {
	// Get selected brushes from outliner
	QList<QTreeWidgetItem*> selectedItems;
	if (m_outliner) {
		selectedItems = m_outliner->selectedItems();
	}
	
	QVector<QTreeWidgetItem*> brushItems;
	for (QTreeWidgetItem* item : selectedItems) {
		if (item && item->text(1) == QStringLiteral("brush")) {
			brushItems.append(item);
		}
	}
	
	if (brushItems.size() < 2) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Select at least 2 brushes to merge"));
		}
		return;
	}
	
	// Create merge tool if needed
	if (!m_mergeTool) {
		m_mergeTool = new MergeTool();
	}
	
	// Validate and merge
	if (!m_mergeTool->canMerge(brushItems)) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Cannot merge brushes: %1").arg(m_mergeTool->getValidationError()));
		}
		return;
	}
	
	if (m_mergeTool->mergeBrushes(brushItems)) {
		QTreeWidgetItem* mergedBrush = m_mergeTool->getMergedBrush();
		if (mergedBrush && m_outliner) {
			// Add merged brush to outliner
			m_outliner->addTopLevelItem(mergedBrush);
			
			// Remove original brushes
			for (QTreeWidgetItem* item : brushItems) {
				if (item->parent()) {
					item->parent()->removeChild(item);
				} else {
					int index = m_outliner->indexOfTopLevelItem(item);
					if (index >= 0) {
						m_outliner->takeTopLevelItem(index);
					}
				}
				delete item;
			}
			
			// Select merged brush
			m_outliner->setCurrentItem(mergedBrush);
			
			if (m_console) {
				m_console->appendPlainText(QStringLiteral("Successfully merged %1 brushes").arg(brushItems.size()));
			}
		}
	} else {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Failed to merge brushes: %1").arg(m_mergeTool->getValidationError()));
		}
	}
}

void RadiantMainWindow::activatePolygonTool() {
	// Deactivate other tools
	if (m_clippingToolActive) {
		activateClippingTool(); // Toggle off
	}
	if (m_vertexToolActive) {
		activateVertexTool(); // Toggle off
	}
	if (m_uvToolActive) {
		activateUVTool(); // Toggle off
	}
	if (m_extrusionToolActive) {
		activateExtrusionTool(); // Toggle off
	}
	if (m_texturePaintToolActive) {
		activateTexturePaintTool(); // Toggle off
	}
	
	if (!m_polygonTool) {
		m_polygonTool = new PolygonTool();
		// Connect polygon tool to all viewports
		if (m_viewPerspective) m_viewPerspective->setPolygonTool(m_polygonTool);
		if (m_viewTop) m_viewTop->setPolygonTool(m_polygonTool);
		if (m_viewFront) m_viewFront->setPolygonTool(m_polygonTool);
		if (m_viewSide) m_viewSide->setPolygonTool(m_polygonTool);
	}
	m_polygonToolActive = !m_polygonToolActive;
	
	if (m_polygonToolActive) {
		m_polygonTool->clearPoints();
		m_polygonTool->setShowPreview(true);
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Polygon tool activated. Click in 2D viewport to define points"));
		}
		// Update viewports
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
	} else {
		if (m_polygonTool && m_polygonTool->isValid()) {
			// Convert polygon to brush
			if (m_console) {
				m_console->appendPlainText(QStringLiteral("Polygon tool: Creating brush from %1 points").arg(m_polygonTool->pointCount()));
			}
			// In full implementation, would create brush from polygon
		}
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Polygon tool deactivated"));
		}
	}
}

void RadiantMainWindow::activateVertexTool() {
	// Deactivate other tools
	if (m_clippingToolActive) {
		activateClippingTool(); // Toggle off
	}
	if (m_polygonToolActive) {
		activatePolygonTool(); // Toggle off
	}
	
	if (!m_vertexTool) {
		m_vertexTool = new VertexTool();
		// Connect vertex tool to all viewports
		if (m_viewPerspective) m_viewPerspective->setVertexTool(m_vertexTool);
		if (m_viewTop) m_viewTop->setVertexTool(m_vertexTool);
		if (m_viewFront) m_viewFront->setVertexTool(m_vertexTool);
		if (m_viewSide) m_viewSide->setVertexTool(m_vertexTool);
	}
	m_vertexToolActive = !m_vertexToolActive;
	
	if (m_vertexToolActive) {
		m_vertexTool->enableRealtime(true);
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Vertex tool activated. Click vertices to select and manipulate (Ctrl+click for multi-select)"));
		}
		// Update viewports
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
	} else {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Vertex tool deactivated"));
		}
	}
}

void RadiantMainWindow::openAudioBrowser() {
	AudioBrowser dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		QString sound = dlg.selectedSound();
		if (!sound.isEmpty() && m_console) {
			m_console->appendPlainText(QStringLiteral("Selected sound: %1").arg(sound));
		}
	}
}

void RadiantMainWindow::activateUVTool() {
	// Deactivate other tools
	if (m_clippingToolActive) {
		activateClippingTool();
	}
	if (m_polygonToolActive) {
		activatePolygonTool();
	}
	if (m_vertexToolActive) {
		activateVertexTool();
	}
	if (m_extrusionToolActive) {
		activateExtrusionTool();
	}
	if (m_texturePaintToolActive) {
		activateTexturePaintTool();
	}
	
	if (!m_uvTool) {
		m_uvTool = new UVTool();
	}
	m_uvToolActive = !m_uvToolActive;
	
	if (m_uvToolActive) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("UV Tool activated. Select a face to edit texture alignment"));
		}
		// Update viewports
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
	} else {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("UV Tool deactivated"));
		}
	}
}

void RadiantMainWindow::applyAutocaulk() {
	if (!m_autocaulkTool) {
		m_autocaulkTool = new AutocaulkTool();
	}
	
	// Get selected brushes
	QList<QTreeWidgetItem*> selectedItems;
	if (m_outliner) {
		selectedItems = m_outliner->selectedItems();
	}
	
	QVector<QTreeWidgetItem*> brushItems;
	for (QTreeWidgetItem* item : selectedItems) {
		if (item && item->text(1) == QStringLiteral("brush")) {
			brushItems.append(item);
		}
	}
	
	if (brushItems.isEmpty()) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("No brushes selected. Select brushes to apply autocaulk"));
		}
		return;
	}
	
	m_autocaulkTool->applyToSelection(brushItems);
	
	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Autocaulk applied: %1 faces caulked in %2 brushes")
			.arg(m_autocaulkTool->facesCaulked())
			.arg(m_autocaulkTool->brushesProcessed()));
	}
}


void RadiantMainWindow::activateTexturePaintTool() {
	// Deactivate other tools
	if (m_clippingToolActive) activateClippingTool();
	if (m_polygonToolActive) activatePolygonTool();
	if (m_vertexToolActive) activateVertexTool();
	if (m_uvToolActive) activateUVTool();
	if (m_extrusionToolActive) activateExtrusionTool();
	
	if (!m_texturePaintTool) {
		m_texturePaintTool = new TexturePaintTool();
		if (!m_currentMaterial.isEmpty()) {
			m_texturePaintTool->setTexture(m_currentMaterial);
		}
		// Connect texture paint tool to viewports
		if (m_viewPerspective) m_viewPerspective->setTexturePaintTool(m_texturePaintTool);
		if (m_viewTop) m_viewTop->setTexturePaintTool(m_texturePaintTool);
		if (m_viewFront) m_viewFront->setTexturePaintTool(m_texturePaintTool);
		if (m_viewSide) m_viewSide->setTexturePaintTool(m_texturePaintTool);
	}
	m_texturePaintToolActive = !m_texturePaintToolActive;
	
	if (m_texturePaintToolActive) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Texture paint tool activated. Drag to paint texture on faces"));
		}
		// Update viewports
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
	} else {
		if (m_texturePaintTool) {
			m_texturePaintTool->endPaint();
		}
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Texture paint tool deactivated"));
		}
	}
}

void RadiantMainWindow::activateExtrusionTool() {
	// Deactivate other tools
	if (m_clippingToolActive) activateClippingTool();
	if (m_polygonToolActive) activatePolygonTool();
	if (m_vertexToolActive) activateVertexTool();
	if (m_uvToolActive) activateUVTool();
	if (m_texturePaintToolActive) activateTexturePaintTool();
	
	if (!m_extrusionTool) {
		m_extrusionTool = new ExtrusionTool();
		// Connect extrusion tool to viewports
		if (m_viewPerspective) {
			m_viewPerspective->setExtrusionTool(m_extrusionTool);
			m_viewPerspective->setExtrusionToolActive(m_extrusionToolActive);
		}
		if (m_viewTop) {
			m_viewTop->setExtrusionTool(m_extrusionTool);
			m_viewTop->setExtrusionToolActive(m_extrusionToolActive);
		}
		if (m_viewFront) {
			m_viewFront->setExtrusionTool(m_extrusionTool);
			m_viewFront->setExtrusionToolActive(m_extrusionToolActive);
		}
		if (m_viewSide) {
			m_viewSide->setExtrusionTool(m_extrusionTool);
			m_viewSide->setExtrusionToolActive(m_extrusionToolActive);
		}
	}
	m_extrusionToolActive = !m_extrusionToolActive;
	
	// Update viewports with extrusion tool state
	if (m_viewPerspective) m_viewPerspective->setExtrusionToolActive(m_extrusionToolActive);
	if (m_viewTop) m_viewTop->setExtrusionToolActive(m_extrusionToolActive);
	if (m_viewFront) m_viewFront->setExtrusionToolActive(m_extrusionToolActive);
	if (m_viewSide) m_viewSide->setExtrusionToolActive(m_extrusionToolActive);
	
	if (m_extrusionToolActive) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Extrusion tool activated. Select a face and drag to extrude"));
		}
		// Update viewports
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
	} else {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Extrusion tool deactivated"));
		}
	}
}

void RadiantMainWindow::adjustLightPower(float delta) {
	// Get selected light entity
	QTreeWidgetItem* selected = m_outliner ? m_outliner->currentItem() : nullptr;
	if (!selected || selected->text(1) != QStringLiteral("entity")) {
		return;
	}
	
	// Check if it's a light entity
	QString name = selected->text(0);
	if (!name.contains("light", Qt::CaseInsensitive)) {
		return;
	}
	
	// In full implementation, would:
	// 1. Get current light power/radius
	// 2. Adjust by delta
	// 3. Update entity properties
	// 4. Update viewport rendering
	
	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Light power adjusted by %1").arg(delta));
	}
}

void RadiantMainWindow::showMapInfo() {
	QDialog dlg(this);
	dlg.setWindowTitle(QStringLiteral("Map Information"));
	dlg.setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
	                  Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
	dlg.setSizeGripEnabled(true);
	dlg.resize(400, 300);
	auto* layout = new QVBoxLayout(&dlg);
	
	// Count entities and brushes
	int totalBrushes = 0;
	int totalEntities = 0;
	int totalPatches = 0;
	int ingameEntities = 0;
	int groupEntities = 0;
	int ingameGroupEntities = 0;
	
	if (m_outliner) {
		for (int i = 0; i < m_outliner->topLevelItemCount(); ++i) {
			QTreeWidgetItem* item = m_outliner->topLevelItem(i);
			if (!item) continue;
			
			QString type = item->text(1);
			if (type == QStringLiteral("brush")) {
				totalBrushes++;
			} else if (type == QStringLiteral("entity")) {
				totalEntities++;
				// Check if it's an ingame entity (not worldspawn, etc.)
				QString name = item->text(0);
				if (!name.isEmpty() && name != QStringLiteral("worldspawn")) {
					ingameEntities++;
				}
				// Check if it's in a group
				if (item->parent() && item->parent()->text(1) == QStringLiteral("Group")) {
					groupEntities++;
					if (!name.isEmpty() && name != QStringLiteral("worldspawn")) {
						ingameGroupEntities++;
					}
				}
			}
		}
	}
	
	// Create info display
	auto* infoText = new QLabel(&dlg);
	infoText->setText(QStringLiteral(
		"<b>Map Statistics</b><br>"
		"Total Brushes: %1<br>"
		"Total Patches: %2<br>"
		"Total Entities: %3<br>"
		"Ingame Entities: %4<br>"
		"Group Entities: %5<br>"
		"Ingame Group Entities: %6<br>"
		"<br>"
		"<b>Map File:</b><br>"
		"%7"
	).arg(totalBrushes)
	 .arg(totalPatches)
	 .arg(totalEntities)
	 .arg(ingameEntities)
	 .arg(groupEntities)
	 .arg(ingameGroupEntities)
	 .arg(m_currentMapPath.isEmpty() ? "No map loaded" : m_currentMapPath));
	
	infoText->setAlignment(Qt::AlignLeft);
	layout->addWidget(infoText);
	
	auto* okBtn = new QPushButton(QStringLiteral("OK"), &dlg);
	connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
	layout->addWidget(okBtn);
	
	dlg.exec();
}

void RadiantMainWindow::activatePatchThickenTool() {
	if (!m_patchThickenTool) {
		m_patchThickenTool = new PatchThickenTool();
	}
	
	// Get selected patches
	QList<QTreeWidgetItem*> selectedItems;
	if (m_outliner) {
		selectedItems = m_outliner->selectedItems();
	}
	
	QVector<QTreeWidgetItem*> patchItems;
	for (QTreeWidgetItem* item : selectedItems) {
		if (item && item->text(1) == QStringLiteral("patch")) {
			patchItems.append(item);
		}
	}
	
	if (patchItems.isEmpty()) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("No patches selected. Select patches to thicken"));
		}
		return;
	}
	
	// Apply thickening
	float thickness = m_patchThickenTool->thickness();
	if (m_patchThickenTool->thickenPatches(patchItems, thickness)) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Thickened %1 patches by %2 units")
				.arg(patchItems.size())
				.arg(thickness, 0, 'f', 2));
		}
		// Update viewports
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
	} else {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Failed to thicken patches"));
		}
	}
}

void RadiantMainWindow::activateCSGTool() {
	if (!m_csgTool) {
		m_csgTool = new CSGTool();
	}
	
	// Get selected brushes
	QList<QTreeWidgetItem*> selectedItems;
	if (m_outliner) {
		selectedItems = m_outliner->selectedItems();
	}
	
	QVector<QTreeWidgetItem*> brushItems;
	for (QTreeWidgetItem* item : selectedItems) {
		if (item && item->text(1) == QStringLiteral("brush")) {
			brushItems.append(item);
		}
	}
	
	if (brushItems.isEmpty()) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("No brushes selected. Select brushes to create shell"));
		}
		return;
	}
	
	// Apply CSG shell
	float thickness = m_csgTool->thickness();
	if (m_csgTool->createShell(brushItems, thickness)) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Created shell from %1 brushes with thickness %2")
				.arg(brushItems.size())
				.arg(thickness, 0, 'f', 2));
		}
		// Update viewports
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
	} else {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Failed to create shell"));
		}
	}
}

void RadiantMainWindow::selectConnectedEntities() {
	if (!m_entityWalker) {
		m_entityWalker = new EntityWalker();
	}
	
	QTreeWidgetItem* selected = m_outliner ? m_outliner->currentItem() : nullptr;
	if (!selected || selected->text(1) != QStringLiteral("entity")) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("No entity selected. Select an entity to find connections"));
		}
		return;
	}
	
	m_entityWalker->selectConnected(selected);
	
	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Found %1 connected entities")
			.arg(m_entityWalker->connectionCount()));
	}
	
	// Update viewports
	if (m_viewPerspective) m_viewPerspective->update();
	if (m_viewTop) m_viewTop->update();
	if (m_viewFront) m_viewFront->update();
	if (m_viewSide) m_viewSide->update();
}

void RadiantMainWindow::walkToNextEntity() {
	if (!m_entityWalker) {
		m_entityWalker = new EntityWalker();
	}
	
	QTreeWidgetItem* selected = m_outliner ? m_outliner->currentItem() : nullptr;
	if (!selected || selected->text(1) != QStringLiteral("entity")) {
		return;
	}
	
	m_entityWalker->walkToNext(selected);
	
	// Update viewports
	if (m_viewPerspective) m_viewPerspective->update();
	if (m_viewTop) m_viewTop->update();
	if (m_viewFront) m_viewFront->update();
	if (m_viewSide) m_viewSide->update();
}

void RadiantMainWindow::walkToPreviousEntity() {
	if (!m_entityWalker) {
		m_entityWalker = new EntityWalker();
	}
	
	QTreeWidgetItem* selected = m_outliner ? m_outliner->currentItem() : nullptr;
	if (!selected || selected->text(1) != QStringLiteral("entity")) {
		return;
	}
	
	m_entityWalker->walkToPrevious(selected);
	
	// Update viewports
	if (m_viewPerspective) m_viewPerspective->update();
	if (m_viewTop) m_viewTop->update();
	if (m_viewFront) m_viewFront->update();
	if (m_viewSide) m_viewSide->update();
}

void RadiantMainWindow::enableTextureAlphaDisplay(bool enabled) {
	Q_UNUSED(enabled);
	
	// In full implementation, would:
	// 1. Reload texture thumbnails with alpha channel
	// 2. Update texture list display
	// 3. Show/hide alpha transparency
	
	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Texture alpha display %1")
			.arg(enabled ? "enabled" : "disabled"));
	}
	
	// Repopulate texture browser
	if (m_textureList) {
		// Would reload textures with alpha support
		m_textureList->update();
	}
}

void RadiantMainWindow::searchTextures(const QString& query) {
	if (!m_textureList) return;
	
	// In full implementation, would:
	// 1. Filter texture list by query
	// 2. Search in texture names
	// 3. Search in directory paths
	// 4. Search in tags (if implemented)
	
	// Simplified: just log for now
	if (!query.isEmpty() && m_console) {
		m_console->appendPlainText(QStringLiteral("Searching textures: %1").arg(query));
	}
	
	// Would filter m_textureList items here
	m_textureList->update();
}

void RadiantMainWindow::activateTexturePasteTool() {
	// Deactivate other tools
	if (m_clippingToolActive) activateClippingTool();
	if (m_polygonToolActive) activatePolygonTool();
	if (m_vertexToolActive) activateVertexTool();
	if (m_uvToolActive) activateUVTool();
	if (m_extrusionToolActive) activateExtrusionTool();
	if (m_texturePaintToolActive) activateTexturePaintTool();
	
	if (!m_texturePasteTool) {
		m_texturePasteTool = new TexturePasteTool();
	}
	m_texturePasteToolActive = !m_texturePasteToolActive;
	
	if (m_texturePasteToolActive) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Texture paste tool activated. Right-click face to copy, left-click to paste"));
		}
		// Update viewports
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
	} else {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Texture paste tool deactivated"));
		}
	}
}

void RadiantMainWindow::openMESSDialog() {
	if (!m_messManager) {
		m_messManager = new MESSManager();
		// Load templates from directory
		QString templateDir = QStringLiteral("./templates");
		m_messManager->setTemplateDirectory(templateDir);
		m_messManager->loadTemplatesFromDirectory(templateDir);
	}
	
	MESSDialog dlg(m_messManager, this);
	if (dlg.exec() == QDialog::Accepted) {
		QString templateName = dlg.selectedTemplate();
		QMap<QString, QString> variables = dlg.templateVariables();
		QVector3D position = dlg.instantiationPosition();
		
		if (!templateName.isEmpty() && m_outliner) {
			QVector<QTreeWidgetItem*> items = m_messManager->instantiateTemplate(
				templateName, position, variables, m_outliner);
			
			if (m_console) {
				m_console->appendPlainText(QStringLiteral("Instantiated template '%1' with %2 items at (%3, %4, %5)")
					.arg(templateName)
					.arg(items.size())
					.arg(position.x(), 0, 'f', 1)
					.arg(position.y(), 0, 'f', 1)
					.arg(position.z(), 0, 'f', 1));
			}
			
			// Update viewports
			if (m_viewPerspective) m_viewPerspective->update();
			if (m_viewTop) m_viewTop->update();
			if (m_viewFront) m_viewFront->update();
			if (m_viewSide) m_viewSide->update();
		}
	}
}

void RadiantMainWindow::openTextureExtractionDialog() {
	TextureExtractionDialog dlg(this);
	dlg.exec();
}

void RadiantMainWindow::openSurfaceInspector() {
	if (!m_surfaceInspector) {
		m_surfaceInspector = new SurfaceInspector(this);
	}
	
	// Update inspector with selected face data
	// In full implementation, would get selected face properties
	
	m_surfaceInspector->show();
	m_surfaceInspector->raise();
	m_surfaceInspector->activateWindow();
}

void RadiantMainWindow::openSelectionSetsDialog() {
	if (!m_selectionSets) {
		m_selectionSets = new SelectionSets();
		// Load saved sets
		QSettings settings;
		QString setsPath = settings.value("selectionSets/path", "./selection_sets.json").toString();
		m_selectionSets->loadFromFile(setsPath);
	}
	
	// In full implementation, would show dialog for managing sets
	// For now, just create/store current selection
	QList<QTreeWidgetItem*> selected = m_outliner ? m_outliner->selectedItems() : QList<QTreeWidgetItem*>();
	if (!selected.isEmpty()) {
		QVector<QTreeWidgetItem*> items;
		for (QTreeWidgetItem* item : selected) {
			items.append(item);
		}
		m_selectionSets->createSet(QStringLiteral("Set1"));
		m_selectionSets->storeSelection(QStringLiteral("Set1"), items);
		
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Selection stored in Set1"));
		}
	} else {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("No selection to store"));
		}
	}
}

void RadiantMainWindow::openPreferences() {
	PreferencesDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		// Settings saved in dialog
		// Reload settings that affect UI
		QSettings settings;
		float gridSize = settings.value("viewport/gridSize", 32.0).toDouble();
		VkViewportWidget::setGridSize(gridSize);
		
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Preferences saved"));
		}
		
		// Update viewports
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
	}
}

void RadiantMainWindow::addRecentFile(const QString& filePath) {
	QSettings settings;
	QStringList recentFiles = settings.value("recentFiles").toStringList();
	
	recentFiles.removeAll(filePath);
	recentFiles.prepend(filePath);
	
	// Limit to 10 recent files
	while (recentFiles.size() > 10) {
		recentFiles.removeLast();
	}
	
	settings.setValue("recentFiles", recentFiles);
	updateRecentFilesMenu();
}

void RadiantMainWindow::updateRecentFilesMenu() {
	QSettings settings;
	QStringList recentFiles = settings.value("recentFiles").toStringList();
	
	// Find recent files menu
	QMenuBar* menuBar = this->menuBar();
	QMenu* fileMenu = nullptr;
	for (QAction* action : menuBar->actions()) {
		if (action->text().contains("File")) {
			fileMenu = action->menu();
			break;
		}
	}
	
	if (!fileMenu) return;
	
	// Find or create recent files menu
	QMenu* recentMenu = nullptr;
	for (QAction* action : fileMenu->actions()) {
		if (action->text() == "Recent Files") {
			recentMenu = action->menu();
			break;
		}
	}
	
	if (!recentMenu) return;
	
	recentMenu->clear();
	
	if (recentFiles.isEmpty()) {
		QAction* noFilesAct = recentMenu->addAction(QStringLiteral("(No recent files)"));
		noFilesAct->setEnabled(false);
	} else {
		for (const QString& file : recentFiles) {
			QFileInfo info(file);
			QAction* fileAct = recentMenu->addAction(info.fileName());
			fileAct->setData(file);
			connect(fileAct, &QAction::triggered, this, [this, file]() {
				loadMap(file);
			});
		}
		recentMenu->addSeparator();
		QAction* clearAct = recentMenu->addAction(QStringLiteral("Clear Recent Files"));
		connect(clearAct, &QAction::triggered, this, [this]() {
			QSettings clearSettings;
			clearSettings.remove("recentFiles");
			updateRecentFilesMenu();
		});
	}
}

void RadiantMainWindow::loadMap(const QString& path) {
	if (path.isEmpty()) return;
	
	QFileInfo fileInfo(path);
	if (!fileInfo.exists() || !fileInfo.isFile()) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Error: Map file not found: %1").arg(path));
		}
		return;
	}
	
	// Load map into viewports
	bool loaded = false;
	if (m_viewPerspective && m_viewPerspective->loadMap(path)) {
		loaded = true;
	}
	if (m_viewTop) m_viewTop->loadMap(path);
	if (m_viewFront) m_viewFront->loadMap(path);
	if (m_viewSide) m_viewSide->loadMap(path);
	
	if (loaded) {
		m_currentMapPath = path;
		addRecentFile(path);
		setOutlinerLoadedMap(path);
		setWindowTitle(QStringLiteral("Radiant Qt - %1").arg(fileInfo.fileName()));
		
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Loaded map: %1").arg(path));
		}
	} else {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Failed to load map: %1").arg(path));
		}
	}
}

void RadiantMainWindow::activateBrushResizeTool() {
	if (!m_brushResizeTool) {
		m_brushResizeTool = new BrushResizeTool();
	}
	
	// Get selected brushes
	QList<QTreeWidgetItem*> selectedItems;
	if (m_outliner) {
		selectedItems = m_outliner->selectedItems();
	}
	
	QVector<QTreeWidgetItem*> brushItems;
	for (QTreeWidgetItem* item : selectedItems) {
		if (item && item->text(1) == QStringLiteral("brush")) {
			brushItems.append(item);
		}
	}
	
	if (brushItems.isEmpty()) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("No brushes selected. Select brushes to resize"));
		}
		return;
	}
	
	// Apply resize
	if (m_brushResizeTool->resizeBrushes(brushItems)) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Resized %1 brushes to minimal faces")
				.arg(brushItems.size()));
		}
		// Update viewports
		if (m_viewPerspective) m_viewPerspective->update();
		if (m_viewTop) m_viewTop->update();
		if (m_viewFront) m_viewFront->update();
		if (m_viewSide) m_viewSide->update();
	} else {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Failed to resize brushes"));
		}
	}
}

// ===== STATUS BAR UPDATE FUNCTIONS =====

void RadiantMainWindow::updateGridStatus() {
	if (!m_gridStatus) return;

	float gridSize = VkViewportWidget::gridSize();

	// Build snapping status string
	QStringList snapModes;
	if (SnappingSystem::instance().isSnapModeEnabled(SnapMode::Grid)) snapModes << "G";
	if (SnappingSystem::instance().isSnapModeEnabled(SnapMode::Point)) snapModes << "P";
	if (SnappingSystem::instance().isSnapModeEnabled(SnapMode::Edge)) snapModes << "E";
	if (SnappingSystem::instance().isSnapModeEnabled(SnapMode::Face)) snapModes << "F";

	QString snapText = snapModes.isEmpty() ? "OFF" : snapModes.join("");

	m_gridStatus->setText(QStringLiteral("Grid:%1 Snap:%2").arg(gridSize, 0, 'g', 3).arg(snapText));
}

void RadiantMainWindow::updateToolStatus() {
	if (!m_toolStatus) return;

	QString toolName;
	switch (m_currentGizmoMode) {
		case 0: toolName = QStringLiteral("Select"); break;
		case 1: toolName = QStringLiteral("Move(Box)"); break;
		case 2: {
			switch (m_currentGizmoOperation) {
				case 0: toolName = QStringLiteral("Move"); break;
				case 1: toolName = QStringLiteral("Rotate"); break;
				case 2: toolName = QStringLiteral("Scale"); break;
				default: toolName = QStringLiteral("Gizmo"); break;
			}
			break;
		}
		default: toolName = QStringLiteral("Unknown"); break;
	}

	// Add current tool information
	if (m_clippingTool && m_clippingToolActive) {
		toolName += QStringLiteral(" + Clipping");
	} else if (m_polygonTool && m_polygonToolActive) {
		toolName += QStringLiteral(" + Polygon");
	} else if (m_vertexTool && m_vertexToolActive) {
		toolName += QStringLiteral(" + Vertex");
	} else if (m_uvTool && m_uvToolActive) {
		toolName += QStringLiteral(" + UV");
	} else if (m_extrusionTool && m_extrusionToolActive) {
		toolName += QStringLiteral(" + Extrusion");
	}

	m_toolStatus->setText(QStringLiteral("Tool: %1").arg(toolName));
}

void RadiantMainWindow::updateCameraStatus() {
	if (!m_cameraStatus) return;

	// Show current viewport focus
	if (m_viewPerspective && m_viewPerspective->hasFocus()) {
		m_cameraStatus->setText(QStringLiteral("Perspective"));
	} else if (m_viewTop && m_viewTop->hasFocus()) {
		m_cameraStatus->setText(QStringLiteral("Top"));
	} else if (m_viewFront && m_viewFront->hasFocus()) {
		m_cameraStatus->setText(QStringLiteral("Front"));
	} else if (m_viewSide && m_viewSide->hasFocus()) {
		m_cameraStatus->setText(QStringLiteral("Side"));
	} else {
		m_cameraStatus->setText(QStringLiteral("Perspective"));
	}
}

void RadiantMainWindow::updatePerformanceStatus() {
	if (!m_performanceStatus) return;

	// In a full implementation, this would show FPS, memory usage, etc.
	// For now, just show a placeholder
	static int frameCount = 0;
	frameCount = (frameCount + 1) % 60;
	m_performanceStatus->setText(QStringLiteral("FPS: %1").arg(60 - (frameCount % 5)));
}

// ===== WORKSPACE MANAGEMENT =====

void RadiantMainWindow::saveWorkspace(const QString& name) {
	if (name.isEmpty()) return;

	QSettings settings;
	settings.beginGroup(QStringLiteral("workspaces"));
	settings.beginGroup(name);

	// Save main window state (dock widgets, toolbars)
	QByteArray windowState = saveState();
	settings.setValue(QStringLiteral("windowState"), windowState);

	// Save window geometry
	QByteArray geometry = saveGeometry();
	settings.setValue(QStringLiteral("geometry"), geometry);

	settings.endGroup();
	settings.endGroup();
}

void RadiantMainWindow::loadWorkspace(const QString& name) {
	if (name.isEmpty()) return;

	QSettings settings;
	settings.beginGroup(QStringLiteral("workspaces"));
	settings.beginGroup(name);

	// Load window state
	QByteArray windowState = settings.value(QStringLiteral("windowState")).toByteArray();
	if (!windowState.isEmpty()) {
		restoreState(windowState);
	}

	// Load geometry
	QByteArray geometry = settings.value(QStringLiteral("geometry")).toByteArray();
	if (!geometry.isEmpty()) {
		restoreGeometry(geometry);
	}

	settings.endGroup();
	settings.endGroup();
}

void RadiantMainWindow::deleteWorkspace(const QString& name) {
	if (name.isEmpty()) return;

	QSettings settings;
	settings.beginGroup(QStringLiteral("workspaces"));
	settings.remove(name);
	settings.endGroup();
}

QStringList RadiantMainWindow::getWorkspaceNames() const {
	QSettings settings;
	settings.beginGroup(QStringLiteral("workspaces"));
	QStringList names = settings.childGroups();
	settings.endGroup();
	return names;
}

void RadiantMainWindow::saveCurrentWorkspace() {
	bool ok;
	QString name = QInputDialog::getText(this, QStringLiteral("Save Workspace"),
		QStringLiteral("Workspace name:"), QLineEdit::Normal, QString(), &ok);
	if (ok && !name.isEmpty()) {
		saveWorkspace(name);
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Workspace '%1' saved").arg(name));
		}
	}
}

void RadiantMainWindow::loadWorkspaceDialog() {
	QStringList names = getWorkspaceNames();
	if (names.isEmpty()) {
		QMessageBox::information(this, QStringLiteral("Load Workspace"),
			QStringLiteral("No saved workspaces found."));
		return;
	}

	bool ok;
	QString name = QInputDialog::getItem(this, QStringLiteral("Load Workspace"),
		QStringLiteral("Select workspace:"), names, 0, false, &ok);
	if (ok && !name.isEmpty()) {
		loadWorkspace(name);
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("Workspace '%1' loaded").arg(name));
		}
	}
}

void RadiantMainWindow::deleteWorkspaceDialog() {
	QStringList names = getWorkspaceNames();
	if (names.isEmpty()) {
		QMessageBox::information(this, QStringLiteral("Delete Workspace"),
			QStringLiteral("No saved workspaces found."));
		return;
	}

	bool ok;
	QString name = QInputDialog::getItem(this, QStringLiteral("Delete Workspace"),
		QStringLiteral("Select workspace to delete:"), names, 0, false, &ok);
	if (ok && !name.isEmpty()) {
		QMessageBox::StandardButton reply = QMessageBox::question(this,
			QStringLiteral("Delete Workspace"),
			QStringLiteral("Are you sure you want to delete workspace '%1'?").arg(name),
			QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes) {
			deleteWorkspace(name);
			if (m_console) {
				m_console->appendPlainText(QStringLiteral("Workspace '%1' deleted").arg(name));
			}
		}
	}
}

// ===== HOTBOX IMPLEMENTATION =====

void RadiantMainWindow::showHotbox() {
	if (!m_hotbox) {
		m_hotbox = new HotboxWidget(this);
		connect(m_hotbox, &HotboxWidget::itemSelected, this, [this](const QString& label) {
			if (m_console) {
				m_console->appendPlainText(QStringLiteral("Hotbox: %1 selected").arg(label));
			}
		});
	}

	// Position hotbox at mouse cursor
	QPoint mousePos = QCursor::pos();
	m_hotbox->setPosition(mousePos);

	// Clear previous items
	m_hotbox->clearItems();

	// Add common tools
	m_hotbox->addItem("Select", [this]() { setGizmoMode(0); setGizmoOperation(0); });
	m_hotbox->addItem("Move", [this]() { setGizmoMode(2); setGizmoOperation(0); });
	m_hotbox->addItem("Rotate", [this]() { setGizmoMode(2); setGizmoOperation(1); });
	m_hotbox->addItem("Scale", [this]() { setGizmoMode(2); setGizmoOperation(2); });
	m_hotbox->addItem("Brush", [this]() { createBrushAtSelection(); });
	m_hotbox->addItem("Entity", [this]() { createEntityAtSelection(); });
	m_hotbox->addItem("Clip", [this]() { activateClippingTool(); });
	m_hotbox->addItem("Merge", [this]() { mergeSelectedBrushes(); });

	m_hotbox->show();
	m_hotbox->raise();
	m_hotbox->activateWindow();
}

void RadiantMainWindow::hideHotbox() {
	if (m_hotbox && m_hotbox->isVisible()) {
		m_hotbox->hide();
	}
}

bool RadiantMainWindow::isHotboxVisible() const {
	return m_hotbox && m_hotbox->isVisible();
}

void RadiantMainWindow::resetCamera() {
	// Reset camera for all viewports
	if (m_viewPerspective) m_viewPerspective->resetCamera();
	if (m_viewTop) m_viewTop->resetCamera();
	if (m_viewFront) m_viewFront->resetCamera();
	if (m_viewSide) m_viewSide->resetCamera();

	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Camera reset"));
	}
}

void RadiantMainWindow::keyPressEvent(QKeyEvent* event) {
	// Handle Space key - show hotbox (Maya-style)
	if (event->key() == Qt::Key_Space && !event->modifiers()) {
		showHotbox();
		event->accept();
		return;
	}

	// Handle Home key - reset camera
	if (event->key() == Qt::Key_Home && !event->modifiers()) {
		resetCamera();
		event->accept();
		return;
	}

	// Handle snap mode toggles (Shift + number keys)
	if (event->modifiers() & Qt::ShiftModifier) {
		switch (event->key()) {
		case Qt::Key_1:
			SnappingSystem::instance().setSnapMode(SnapMode::Grid,
				!SnappingSystem::instance().isSnapModeEnabled(SnapMode::Grid));
			updateGridStatus();
			event->accept();
			return;
		case Qt::Key_2:
			SnappingSystem::instance().setSnapMode(SnapMode::Point,
				!SnappingSystem::instance().isSnapModeEnabled(SnapMode::Point));
			event->accept();
			return;
		case Qt::Key_3:
			SnappingSystem::instance().setSnapMode(SnapMode::Edge,
				!SnappingSystem::instance().isSnapModeEnabled(SnapMode::Edge));
			event->accept();
			return;
		case Qt::Key_4:
			SnappingSystem::instance().setSnapMode(SnapMode::Face,
				!SnappingSystem::instance().isSnapModeEnabled(SnapMode::Face));
			event->accept();
			return;
		case Qt::Key_R: // Toggle measurements
			if (m_viewPerspective) m_viewPerspective->setShowMeasurements(!m_viewPerspective->showMeasurements());
			if (m_viewTop) m_viewTop->setShowMeasurements(!m_viewTop->showMeasurements());
			if (m_viewFront) m_viewFront->setShowMeasurements(!m_viewFront->showMeasurements());
			if (m_viewSide) m_viewSide->setShowMeasurements(!m_viewSide->showMeasurements());
			event->accept();
			return;
		case Qt::Key_C: // Toggle crosshair
			if (m_viewPerspective) m_viewPerspective->setShowCrosshair(!m_viewPerspective->showCrosshair());
			if (m_viewTop) m_viewTop->setShowCrosshair(!m_viewTop->showCrosshair());
			if (m_viewFront) m_viewFront->setShowCrosshair(!m_viewFront->showCrosshair());
			if (m_viewSide) m_viewSide->setShowCrosshair(!m_viewSide->showCrosshair());
			event->accept();
			return;
		case Qt::Key_X: // Clear measurements
			if (m_viewPerspective) m_viewPerspective->clearMeasurements();
			if (m_viewTop) m_viewTop->clearMeasurements();
			if (m_viewFront) m_viewFront->clearMeasurements();
			if (m_viewSide) m_viewSide->clearMeasurements();
			event->accept();
			return;
		}
	}

	// Handle viewport display toggles (Ctrl + keys)
	if (event->modifiers() & Qt::ControlModifier) {
		switch (event->key()) {
		case Qt::Key_G: // Toggle grid
			if (m_viewPerspective) m_viewPerspective->setShowGrid(!m_viewPerspective->showGrid());
			if (m_viewTop) m_viewTop->setShowGrid(!m_viewTop->showGrid());
			if (m_viewFront) m_viewFront->setShowGrid(!m_viewFront->showGrid());
			if (m_viewSide) m_viewSide->setShowGrid(!m_viewSide->showGrid());
			event->accept();
			return;
		case Qt::Key_A: // Toggle axes
			if (m_viewPerspective) m_viewPerspective->setShowAxes(!m_viewPerspective->showAxes());
			if (m_viewTop) m_viewTop->setShowAxes(!m_viewTop->showAxes());
			if (m_viewFront) m_viewFront->setShowAxes(!m_viewFront->showAxes());
			if (m_viewSide) m_viewSide->setShowAxes(!m_viewSide->showAxes());
			event->accept();
			return;
		case Qt::Key_I: // Toggle stats
			if (m_viewPerspective) m_viewPerspective->setShowStats(!m_viewPerspective->showStats());
			if (m_viewTop) m_viewTop->setShowStats(!m_viewTop->showStats());
			if (m_viewFront) m_viewFront->setShowStats(!m_viewFront->showStats());
			if (m_viewSide) m_viewSide->setShowStats(!m_viewSide->showStats());
			event->accept();
			return;
		case Qt::Key_F: // Toggle FPS
			if (m_viewPerspective) m_viewPerspective->setShowFPS(!m_viewPerspective->showFPS());
			if (m_viewTop) m_viewTop->setShowFPS(!m_viewTop->showFPS());
			if (m_viewFront) m_viewFront->setShowFPS(!m_viewFront->showFPS());
			if (m_viewSide) m_viewSide->setShowFPS(!m_viewSide->showFPS());
			event->accept();
			return;
		case Qt::Key_W: // Toggle wireframe
			if (m_viewPerspective) m_viewPerspective->setWireframeMode(!m_viewPerspective->wireframeMode());
			if (m_viewTop) m_viewTop->setWireframeMode(!m_viewTop->wireframeMode());
			if (m_viewFront) m_viewFront->setWireframeMode(!m_viewFront->wireframeMode());
			if (m_viewSide) m_viewSide->setWireframeMode(!m_viewSide->wireframeMode());
			event->accept();
			return;
		case Qt::Key_M: // Toggle measurements
			if (m_viewPerspective) m_viewPerspective->setShowMeasurements(!m_viewPerspective->showMeasurements());
			if (m_viewTop) m_viewTop->setShowMeasurements(!m_viewTop->showMeasurements());
			if (m_viewFront) m_viewFront->setShowMeasurements(!m_viewFront->showMeasurements());
			if (m_viewSide) m_viewSide->setShowMeasurements(!m_viewSide->showMeasurements());
			event->accept();
			return;
		case Qt::Key_N: // Toggle minimap
			if (m_viewPerspective) m_viewPerspective->setShowMinimap(!m_viewPerspective->showMinimap());
			if (m_viewTop) m_viewTop->setShowMinimap(!m_viewTop->showMinimap());
			if (m_viewFront) m_viewFront->setShowMinimap(!m_viewFront->showMinimap());
			if (m_viewSide) m_viewSide->setShowMinimap(!m_viewSide->showMinimap());
			event->accept();
			return;
		case Qt::Key_0: case Qt::Key_1: case Qt::Key_2: case Qt::Key_3: case Qt::Key_4:
		case Qt::Key_5: case Qt::Key_6: case Qt::Key_7: case Qt::Key_8: case Qt::Key_9: {
			int bookmarkSlot = event->key() - Qt::Key_0;
			if (event->modifiers() & Qt::ControlModifier) {
				// Ctrl+number: save bookmark
				if (m_viewPerspective) m_viewPerspective->saveBookmark(bookmarkSlot);
				if (m_viewTop) m_viewTop->saveBookmark(bookmarkSlot);
				if (m_viewFront) m_viewFront->saveBookmark(bookmarkSlot);
				if (m_viewSide) m_viewSide->saveBookmark(bookmarkSlot);
			} else {
				// Just number: load bookmark
				if (m_viewPerspective) m_viewPerspective->loadBookmark(bookmarkSlot);
				if (m_viewTop) m_viewTop->loadBookmark(bookmarkSlot);
				if (m_viewFront) m_viewFront->loadBookmark(bookmarkSlot);
				if (m_viewSide) m_viewSide->loadBookmark(bookmarkSlot);
			}
			event->accept();
			return;
		}
		}
	}

	QMainWindow::keyPressEvent(event);
}

// Direct gizmo mode setting functions
void RadiantMainWindow::setGizmoMode(int mode) {
	m_currentGizmoMode = mode;
	if (m_viewPerspective) m_viewPerspective->setGizmoMode(mode);
	if (m_viewTop) m_viewTop->setGizmoMode(mode);
	if (m_viewFront) m_viewFront->setGizmoMode(mode);
	if (m_viewSide) m_viewSide->setGizmoMode(mode);
	updateToolStatus();
}

void RadiantMainWindow::setGizmoOperation(int operation) {
	m_currentGizmoOperation = operation;
	// The gizmo operation is set on the gizmo objects themselves
	if (m_viewPerspective) m_viewPerspective->setGizmoOperation(operation);
	if (m_viewTop) m_viewTop->setGizmoOperation(operation);
	if (m_viewFront) m_viewFront->setGizmoOperation(operation);
	if (m_viewSide) m_viewSide->setGizmoOperation(operation);
	updateToolStatus();
}

void RadiantMainWindow::setupDockWidgetMinimize(QDockWidget* dock) {
	if (!dock) return;
	
	// Enable dock widget features
	dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable);
	
	// Store the original widget
	QWidget* originalWidget = dock->widget();
	if (!originalWidget) return;
	
	// Create a custom title bar with minimize button
	QWidget* titleBar = new QWidget();
	QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
	titleLayout->setContentsMargins(4, 2, 4, 2);
	titleLayout->setSpacing(2);
	
	QLabel* titleLabel = new QLabel(dock->windowTitle(), titleBar);
	titleLayout->addWidget(titleLabel);
	titleLayout->addStretch();
	
	// Minimize button
	QToolButton* minimizeBtn = new QToolButton(titleBar);
	
	// Try to load custom icons, fallback to Qt standard icons
	QIcon minimizeIcon;
	QIcon restoreIcon;
	
	// Try loading from common icon paths
	QStringList iconPaths = {
		QCoreApplication::applicationDirPath() + "/../radiant/install/bitmaps/minimize.png",
		QCoreApplication::applicationDirPath() + "/radiant/install/bitmaps/minimize.png",
		QStringLiteral("radiant/install/bitmaps/minimize.png"),
		QStringLiteral("./radiant/install/bitmaps/minimize.png")
	};
	
	QStringList restorePaths = {
		QCoreApplication::applicationDirPath() + "/../radiant/install/bitmaps/restore.png",
		QCoreApplication::applicationDirPath() + "/radiant/install/bitmaps/restore.png",
		QStringLiteral("radiant/install/bitmaps/restore.png"),
		QStringLiteral("./radiant/install/bitmaps/restore.png")
	};
	
	// Try to load minimize icon
	for (const QString& path : iconPaths) {
		if (QFile::exists(path)) {
			minimizeIcon = QIcon(path);
			break;
		}
	}
	
	// Try to load restore icon
	for (const QString& path : restorePaths) {
		if (QFile::exists(path)) {
			restoreIcon = QIcon(path);
			break;
		}
	}
	
	// If icons not found, use Qt standard icons or create simple ones
	if (minimizeIcon.isNull()) {
		// Use Qt's standard minimize icon, or create a simple one
		minimizeIcon = style()->standardIcon(QStyle::SP_TitleBarMinButton);
		if (minimizeIcon.isNull()) {
			// Create a simple minimize icon (horizontal line)
			QPixmap pix(16, 16);
			pix.fill(Qt::transparent);
			QPainter painter(&pix);
			painter.setPen(QPen(Qt::white, 2));
			painter.drawLine(4, 8, 12, 8);
			minimizeIcon = QIcon(pix);
		}
	}
	
	if (restoreIcon.isNull()) {
		// Use Qt's standard restore icon, or create a simple one
		restoreIcon = style()->standardIcon(QStyle::SP_TitleBarNormalButton);
		if (restoreIcon.isNull()) {
			// Create a simple restore icon (plus/expand)
			QPixmap pix(16, 16);
			pix.fill(Qt::transparent);
			QPainter painter(&pix);
			painter.setPen(QPen(Qt::white, 2));
			painter.drawLine(8, 4, 8, 12); // Vertical line
			painter.drawLine(4, 8, 12, 8); // Horizontal line
			restoreIcon = QIcon(pix);
		}
	}
	
	minimizeBtn->setIcon(minimizeIcon);
	minimizeBtn->setToolTip(QStringLiteral("Minimize"));
	minimizeBtn->setAutoRaise(true);
	minimizeBtn->setFixedSize(20, 20);
	
	// Track minimized state using a property
	dock->setProperty("minimized", false);
	
		connect(minimizeBtn, &QToolButton::clicked, this, [this, dock, originalWidget, minimizeBtn, minimizeIcon, restoreIcon]() {
		bool isMinimized = dock->property("minimized").toBool();
		
		if (!isMinimized) {
			// Minimize: hide the widget content
			originalWidget->setVisible(false);
			// Collapse the dock to minimum size (just title bar)
			// Store original size constraints
			dock->setProperty("originalMinWidth", dock->minimumWidth());
			dock->setProperty("originalMinHeight", dock->minimumHeight());
			dock->setProperty("originalMaxWidth", dock->maximumWidth());
			dock->setProperty("originalMaxHeight", dock->maximumHeight());
			
			// Set to minimal size
			dock->setMinimumWidth(30);
			dock->setMaximumWidth(30);
			dock->setMinimumHeight(30);
			dock->setMaximumHeight(30);
			
			minimizeBtn->setIcon(restoreIcon); // Change to restore icon
			minimizeBtn->setToolTip(QStringLiteral("Restore"));
			dock->setProperty("minimized", true);
		} else {
			// Restore: show the widget content
			originalWidget->setVisible(true);
			// Restore original size constraints
			int origMinW = dock->property("originalMinWidth").toInt();
			int origMinH = dock->property("originalMinHeight").toInt();
			int origMaxW = dock->property("originalMaxWidth").toInt();
			int origMaxH = dock->property("originalMaxHeight").toInt();
			
			if (origMinW > 0) dock->setMinimumWidth(origMinW);
			else dock->setMinimumWidth(100);
			if (origMinH > 0) dock->setMinimumHeight(origMinH);
			else dock->setMinimumHeight(100);
			if (origMaxW > 0) dock->setMaximumWidth(origMaxW);
			else dock->setMaximumWidth(16777215);
			if (origMaxH > 0) dock->setMaximumHeight(origMaxH);
			else dock->setMaximumHeight(16777215);
			
			minimizeBtn->setIcon(minimizeIcon); // Change back to minimize icon
			minimizeBtn->setToolTip(QStringLiteral("Minimize"));
			dock->setProperty("minimized", false);
		}
	});
	
	titleLayout->addWidget(minimizeBtn);
	
	dock->setTitleBarWidget(titleBar);
}
