#include "preferences_dialog.h"
#include "snapping_system.h"
#include "theme_manager.h"

#include <QTabWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>
#include <QSettings>
#include <QScrollArea>
#include <QColorDialog>
#include <QPushButton>
#include <QPixmap>
#include <QFrame>

PreferencesDialog::PreferencesDialog(QWidget* parent)
	: QDialog(parent) {
	setWindowTitle(QStringLiteral("Preferences"));
	setModal(true);
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
	              Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
	setSizeGripEnabled(true);
	resize(700, 700);
	
	// Initialize default colors
	m_selectionColor = QColor(100, 150, 255);
	m_gridColor = QColor(70, 70, 70);
	m_gridMajorColor = QColor(100, 100, 100);
	m_backgroundColor = QColor(32, 32, 32);
	
	setupUI();
	loadSettings();
}

void PreferencesDialog::setupUI() {
	auto* mainLayout = new QVBoxLayout(this);
	
	m_tabs = new QTabWidget(this);
	
	// General tab - wrapped in scroll area
	auto* generalScroll = new QScrollArea();
	generalScroll->setWidgetResizable(true);
	generalScroll->setFrameShape(QFrame::NoFrame);
	auto* generalTab = new QWidget();
	auto* generalLayout = new QFormLayout(generalTab);
	
	m_autosaveCheck = new QCheckBox(generalTab);
	m_autosaveCheck->setText(QStringLiteral("Enable Autosave"));
	generalLayout->addRow(m_autosaveCheck);
	
	m_autosaveIntervalSpin = new QSpinBox(generalTab);
	m_autosaveIntervalSpin->setRange(1, 60);
	m_autosaveIntervalSpin->setSuffix(QStringLiteral(" minutes"));
	generalLayout->addRow(QStringLiteral("Autosave Interval:"), m_autosaveIntervalSpin);
	
	m_loadLastMapCheck = new QCheckBox(generalTab);
	m_loadLastMapCheck->setText(QStringLiteral("Load Last Map on Startup"));
	generalLayout->addRow(m_loadLastMapCheck);
	
	m_confirmOnExitCheck = new QCheckBox(generalTab);
	m_confirmOnExitCheck->setText(QStringLiteral("Confirm on Exit"));
	generalLayout->addRow(m_confirmOnExitCheck);
	
	m_showTooltipsCheck = new QCheckBox(generalTab);
	m_showTooltipsCheck->setText(QStringLiteral("Show Tooltips"));
	generalLayout->addRow(m_showTooltipsCheck);
	
	generalLayout->addRow(new QLabel(QStringLiteral("<b>Undo/Redo:</b>")));
	
	m_undoLimitSpin = new QSpinBox(generalTab);
	m_undoLimitSpin->setRange(10, 1000);
	m_undoLimitSpin->setValue(100);
	m_undoLimitSpin->setSuffix(QStringLiteral(" levels"));
	generalLayout->addRow(QStringLiteral("Undo Limit:"), m_undoLimitSpin);
	
	generalLayout->addRow(new QLabel(QStringLiteral("<b>Autosave Backups:</b>")));
	
	m_autosaveBackupCountSpin = new QSpinBox(generalTab);
	m_autosaveBackupCountSpin->setRange(1, 20);
	m_autosaveBackupCountSpin->setValue(5);
	generalLayout->addRow(QStringLiteral("Backup Count:"), m_autosaveBackupCountSpin);
	
	generalScroll->setWidget(generalTab);
	m_tabs->addTab(generalScroll, QStringLiteral("General"));
	
	// Viewport tab - wrapped in scroll area
	auto* viewportScroll = new QScrollArea();
	viewportScroll->setWidgetResizable(true);
	viewportScroll->setFrameShape(QFrame::NoFrame);
	auto* viewportTab = new QWidget();
	auto* viewportLayout = new QFormLayout(viewportTab);
	
	m_gridSizeSpin = new QDoubleSpinBox(viewportTab);
	m_gridSizeSpin->setRange(0.5, 1024.0);
	m_gridSizeSpin->setValue(32.0);
	m_gridSizeSpin->setSingleStep(1.0);
	m_gridSizeSpin->setDecimals(1);
	viewportLayout->addRow(QStringLiteral("Grid Size:"), m_gridSizeSpin);
	
	m_show3DGridCheck = new QCheckBox(viewportTab);
	m_show3DGridCheck->setText(QStringLiteral("Show 3D Grid"));
	viewportLayout->addRow(m_show3DGridCheck);
	
	m_fovSpin = new QSpinBox(viewportTab);
	m_fovSpin->setRange(30, 120);
	m_fovSpin->setValue(75);
	m_fovSpin->setSuffix(QStringLiteral(" degrees"));
	viewportLayout->addRow(QStringLiteral("Field of View:"), m_fovSpin);
	
	m_showAxesCheck = new QCheckBox(viewportTab);
	m_showAxesCheck->setText(QStringLiteral("Show Axes"));
	viewportLayout->addRow(m_showAxesCheck);

	// Viewport display settings
	viewportLayout->addRow(new QLabel(QStringLiteral("<b>Viewport Display:</b>")));

	m_showStatsCheck = new QCheckBox(viewportTab);
	m_showStatsCheck->setText(QStringLiteral("Show Viewport Stats"));
	viewportLayout->addRow(m_showStatsCheck);

	m_showGridCheck = new QCheckBox(viewportTab);
	m_showGridCheck->setText(QStringLiteral("Show Grid"));
	viewportLayout->addRow(m_showGridCheck);

	m_showFPSCheck = new QCheckBox(viewportTab);
	m_showFPSCheck->setText(QStringLiteral("Show FPS Counter"));
	viewportLayout->addRow(m_showFPSCheck);

	m_wireframeModeCheck = new QCheckBox(viewportTab);
	m_wireframeModeCheck->setText(QStringLiteral("Wireframe Mode"));
	viewportLayout->addRow(m_wireframeModeCheck);

	// Additional viewport features
	viewportLayout->addRow(new QLabel(QStringLiteral("<b>Advanced Features:</b>")));

	m_showMeasurementsCheck = new QCheckBox(viewportTab);
	m_showMeasurementsCheck->setText(QStringLiteral("Show Measurements"));
	viewportLayout->addRow(m_showMeasurementsCheck);

	m_showMinimapCheck = new QCheckBox(viewportTab);
	m_showMinimapCheck->setText(QStringLiteral("Show Navigation Minimap"));
	viewportLayout->addRow(m_showMinimapCheck);

	m_showPerformanceWarningsCheck = new QCheckBox(viewportTab);
	m_showPerformanceWarningsCheck->setText(QStringLiteral("Show Performance Warnings"));
	viewportLayout->addRow(m_showPerformanceWarningsCheck);

	// Snapping controls
	viewportLayout->addRow(new QLabel(QStringLiteral("<b>Snapping:</b>")));

	m_snapToGridCheck = new QCheckBox(viewportTab);
	m_snapToGridCheck->setText(QStringLiteral("Snap to Grid"));
	viewportLayout->addRow(m_snapToGridCheck);

	m_snapToPointCheck = new QCheckBox(viewportTab);
	m_snapToPointCheck->setText(QStringLiteral("Snap to Points"));
	viewportLayout->addRow(m_snapToPointCheck);

	m_snapToEdgeCheck = new QCheckBox(viewportTab);
	m_snapToEdgeCheck->setText(QStringLiteral("Snap to Edges"));
	viewportLayout->addRow(m_snapToEdgeCheck);

	m_snapToFaceCheck = new QCheckBox(viewportTab);
	m_snapToFaceCheck->setText(QStringLiteral("Snap to Faces"));
	viewportLayout->addRow(m_snapToFaceCheck);

	m_snapThresholdSpin = new QDoubleSpinBox(viewportTab);
	m_snapThresholdSpin->setRange(0.01, 10.0);
	m_snapThresholdSpin->setValue(0.5);
	m_snapThresholdSpin->setSingleStep(0.1);
	m_snapThresholdSpin->setSuffix(QStringLiteral(" units"));
	viewportLayout->addRow(QStringLiteral("Snap Threshold:"), m_snapThresholdSpin);
	
	// Camera/Viewport movement settings
	viewportLayout->addRow(new QLabel(QStringLiteral("<b>Camera Movement:</b>")));
	
	m_cameraMoveSpeedSpin = new QDoubleSpinBox(viewportTab);
	m_cameraMoveSpeedSpin->setRange(0.1, 10.0);
	m_cameraMoveSpeedSpin->setValue(1.0);
	m_cameraMoveSpeedSpin->setSingleStep(0.1);
	m_cameraMoveSpeedSpin->setDecimals(1);
	viewportLayout->addRow(QStringLiteral("Movement Speed:"), m_cameraMoveSpeedSpin);
	
	m_cameraRotateSpeedSpin = new QDoubleSpinBox(viewportTab);
	m_cameraRotateSpeedSpin->setRange(0.1, 5.0);
	m_cameraRotateSpeedSpin->setValue(1.0);
	m_cameraRotateSpeedSpin->setSingleStep(0.1);
	m_cameraRotateSpeedSpin->setDecimals(1);
	viewportLayout->addRow(QStringLiteral("Rotation Speed:"), m_cameraRotateSpeedSpin);
	
	m_mouseSensitivitySpin = new QDoubleSpinBox(viewportTab);
	m_mouseSensitivitySpin->setRange(0.1, 5.0);
	m_mouseSensitivitySpin->setValue(1.0);
	m_mouseSensitivitySpin->setSingleStep(0.1);
	m_mouseSensitivitySpin->setDecimals(1);
	viewportLayout->addRow(QStringLiteral("Mouse Sensitivity:"), m_mouseSensitivitySpin);
	
	m_invertMouseYCheck = new QCheckBox(viewportTab);
	m_invertMouseYCheck->setText(QStringLiteral("Invert Mouse Y Axis"));
	viewportLayout->addRow(m_invertMouseYCheck);
	
	m_cameraAccelerationSpin = new QDoubleSpinBox(viewportTab);
	m_cameraAccelerationSpin->setRange(0.0, 2.0);
	m_cameraAccelerationSpin->setValue(0.0);
	m_cameraAccelerationSpin->setSingleStep(0.1);
	m_cameraAccelerationSpin->setDecimals(1);
	viewportLayout->addRow(QStringLiteral("Camera Acceleration:"), m_cameraAccelerationSpin);
	
	m_defaultCameraDistanceSpin = new QDoubleSpinBox(viewportTab);
	m_defaultCameraDistanceSpin->setRange(10.0, 10000.0);
	m_defaultCameraDistanceSpin->setValue(512.0);
	m_defaultCameraDistanceSpin->setSingleStep(10.0);
	m_defaultCameraDistanceSpin->setDecimals(0);
	viewportLayout->addRow(QStringLiteral("Default Camera Distance:"), m_defaultCameraDistanceSpin);
	
	// Grid appearance settings
	viewportLayout->addRow(new QLabel(QStringLiteral("<b>Grid Appearance:</b>")));
	
	m_gridOpacitySpin = new QDoubleSpinBox(viewportTab);
	m_gridOpacitySpin->setRange(0.0, 1.0);
	m_gridOpacitySpin->setValue(0.5);
	m_gridOpacitySpin->setSingleStep(0.1);
	m_gridOpacitySpin->setDecimals(1);
	viewportLayout->addRow(QStringLiteral("Grid Opacity:"), m_gridOpacitySpin);
	
	m_gridSubdivisionsSpin = new QSpinBox(viewportTab);
	m_gridSubdivisionsSpin->setRange(1, 16);
	m_gridSubdivisionsSpin->setValue(4);
	viewportLayout->addRow(QStringLiteral("Grid Subdivisions:"), m_gridSubdivisionsSpin);
	
	m_showGridInPerspectiveCheck = new QCheckBox(viewportTab);
	m_showGridInPerspectiveCheck->setText(QStringLiteral("Show Grid in Perspective View"));
	viewportLayout->addRow(m_showGridInPerspectiveCheck);
	
	// Selection appearance settings
	viewportLayout->addRow(new QLabel(QStringLiteral("<b>Selection Display:</b>")));
	
	m_showSelectionBoundsCheck = new QCheckBox(viewportTab);
	m_showSelectionBoundsCheck->setText(QStringLiteral("Show Selection Bounds"));
	viewportLayout->addRow(m_showSelectionBoundsCheck);
	
	m_selectionOutlineWidthSpin = new QDoubleSpinBox(viewportTab);
	m_selectionOutlineWidthSpin->setRange(1.0, 10.0);
	m_selectionOutlineWidthSpin->setValue(2.0);
	m_selectionOutlineWidthSpin->setSingleStep(0.5);
	m_selectionOutlineWidthSpin->setDecimals(1);
	viewportLayout->addRow(QStringLiteral("Selection Outline Width:"), m_selectionOutlineWidthSpin);
	
	// Display/Rendering settings
	viewportLayout->addRow(new QLabel(QStringLiteral("<b>Rendering:</b>")));
	
	m_showTexturesCheck = new QCheckBox(viewportTab);
	m_showTexturesCheck->setText(QStringLiteral("Show Textures"));
	viewportLayout->addRow(m_showTexturesCheck);
	
	m_showEntityModelsCheck = new QCheckBox(viewportTab);
	m_showEntityModelsCheck->setText(QStringLiteral("Show Entity Models"));
	viewportLayout->addRow(m_showEntityModelsCheck);
	
	m_showEntityNamesCheck = new QCheckBox(viewportTab);
	m_showEntityNamesCheck->setText(QStringLiteral("Show Entity Names"));
	viewportLayout->addRow(m_showEntityNamesCheck);
	
	m_showBrushWireframesCheck = new QCheckBox(viewportTab);
	m_showBrushWireframesCheck->setText(QStringLiteral("Show Brush Wireframes"));
	viewportLayout->addRow(m_showBrushWireframesCheck);
	
	m_showLightRadiusCheck = new QCheckBox(viewportTab);
	m_showLightRadiusCheck->setText(QStringLiteral("Show Light Radius"));
	viewportLayout->addRow(m_showLightRadiusCheck);
	
	m_lightIntensitySpin = new QDoubleSpinBox(viewportTab);
	m_lightIntensitySpin->setRange(0.1, 5.0);
	m_lightIntensitySpin->setValue(1.0);
	m_lightIntensitySpin->setSingleStep(0.1);
	m_lightIntensitySpin->setDecimals(1);
	viewportLayout->addRow(QStringLiteral("Light Intensity:"), m_lightIntensitySpin);
	
	m_ambientLightSpin = new QDoubleSpinBox(viewportTab);
	m_ambientLightSpin->setRange(0.0, 1.0);
	m_ambientLightSpin->setValue(0.3);
	m_ambientLightSpin->setSingleStep(0.1);
	m_ambientLightSpin->setDecimals(1);
	viewportLayout->addRow(QStringLiteral("Ambient Light:"), m_ambientLightSpin);
	
	m_textureFilteringCombo = new QComboBox(viewportTab);
	m_textureFilteringCombo->addItems({QStringLiteral("Nearest"), QStringLiteral("Linear"), QStringLiteral("Bilinear"), QStringLiteral("Trilinear")});
	viewportLayout->addRow(QStringLiteral("Texture Filtering:"), m_textureFilteringCombo);
	
	// Keyboard/Mouse settings
	viewportLayout->addRow(new QLabel(QStringLiteral("<b>Mouse/Wheel:</b>")));
	
	m_mouseWheelZoomSpeedSpin = new QDoubleSpinBox(viewportTab);
	m_mouseWheelZoomSpeedSpin->setRange(0.1, 5.0);
	m_mouseWheelZoomSpeedSpin->setValue(1.0);
	m_mouseWheelZoomSpeedSpin->setSingleStep(0.1);
	m_mouseWheelZoomSpeedSpin->setDecimals(1);
	viewportLayout->addRow(QStringLiteral("Wheel Zoom Speed:"), m_mouseWheelZoomSpeedSpin);
	
	m_panSpeedSpin = new QDoubleSpinBox(viewportTab);
	m_panSpeedSpin->setRange(0.1, 5.0);
	m_panSpeedSpin->setValue(1.0);
	m_panSpeedSpin->setSingleStep(0.1);
	m_panSpeedSpin->setDecimals(1);
	viewportLayout->addRow(QStringLiteral("Pan Speed:"), m_panSpeedSpin);
	
	m_rotationSpeedSpin = new QDoubleSpinBox(viewportTab);
	m_rotationSpeedSpin->setRange(0.1, 5.0);
	m_rotationSpeedSpin->setValue(1.0);
	m_rotationSpeedSpin->setSingleStep(0.1);
	m_rotationSpeedSpin->setDecimals(1);
	viewportLayout->addRow(QStringLiteral("Rotation Speed:"), m_rotationSpeedSpin);

	viewportScroll->setWidget(viewportTab);
	m_tabs->addTab(viewportScroll, QStringLiteral("Viewport"));
	
	// Editor tab - wrapped in scroll area
	auto* editorScroll = new QScrollArea();
	editorScroll->setWidgetResizable(true);
	editorScroll->setFrameShape(QFrame::NoFrame);
	auto* editorTab = new QWidget();
	auto* editorLayout = new QFormLayout(editorTab);
	
	m_themeCombo = new QComboBox(editorTab);
	// Populate from ThemeManager
	QStringList themes = ThemeManager::instance().availableThemes();
	m_themeCombo->addItems(themes);
	editorLayout->addRow(QStringLiteral("Theme:"), m_themeCombo);
	
	m_fontCombo = new QComboBox(editorTab);
	m_fontCombo->addItems({QStringLiteral("Default"), QStringLiteral("Monospace"), QStringLiteral("Sans")});
	editorLayout->addRow(QStringLiteral("Font:"), m_fontCombo);
	
	m_fontSizeSpin = new QSpinBox(editorTab);
	m_fontSizeSpin->setRange(8, 24);
	m_fontSizeSpin->setValue(10);
	editorLayout->addRow(QStringLiteral("Font Size:"), m_fontSizeSpin);
	
	m_texturePathEdit = new QLineEdit(editorTab);
	auto* textureBrowseBtn = new QPushButton(QStringLiteral("Browse..."), editorTab);
	auto* textureLayout = new QHBoxLayout();
	textureLayout->addWidget(m_texturePathEdit);
	textureLayout->addWidget(textureBrowseBtn);
	editorLayout->addRow(QStringLiteral("Texture Path:"), textureLayout);
	
	m_modelPathEdit = new QLineEdit(editorTab);
	auto* modelBrowseBtn = new QPushButton(QStringLiteral("Browse..."), editorTab);
	auto* modelLayout = new QHBoxLayout();
	modelLayout->addWidget(m_modelPathEdit);
	modelLayout->addWidget(modelBrowseBtn);
	editorLayout->addRow(QStringLiteral("Model Path:"), modelLayout);
	
	// Editor display settings
	editorLayout->addRow(new QLabel(QStringLiteral("<b>Display Options:</b>")));
	
	m_showCoordinateDisplayCheck = new QCheckBox(editorTab);
	m_showCoordinateDisplayCheck->setText(QStringLiteral("Show Coordinate Display"));
	editorLayout->addRow(m_showCoordinateDisplayCheck);
	
	m_showCrosshairCheck = new QCheckBox(editorTab);
	m_showCrosshairCheck->setText(QStringLiteral("Show Crosshair"));
	editorLayout->addRow(m_showCrosshairCheck);
	
	// Additional file paths
	editorLayout->addRow(new QLabel(QStringLiteral("<b>Additional Paths:</b>")));
	
	m_mapPathEdit = new QLineEdit(editorTab);
	auto* mapBrowseBtn = new QPushButton(QStringLiteral("Browse..."), editorTab);
	auto* mapLayout = new QHBoxLayout();
	mapLayout->addWidget(m_mapPathEdit);
	mapLayout->addWidget(mapBrowseBtn);
	editorLayout->addRow(QStringLiteral("Map Path:"), mapLayout);
	
	m_gamePathEdit = new QLineEdit(editorTab);
	auto* gameBrowseBtn = new QPushButton(QStringLiteral("Browse..."), editorTab);
	auto* gameLayout = new QHBoxLayout();
	gameLayout->addWidget(m_gamePathEdit);
	gameLayout->addWidget(gameBrowseBtn);
	editorLayout->addRow(QStringLiteral("Game Path:"), gameLayout);
	
	m_shaderPathEdit = new QLineEdit(editorTab);
	auto* shaderBrowseBtn = new QPushButton(QStringLiteral("Browse..."), editorTab);
	auto* shaderLayout = new QHBoxLayout();
	shaderLayout->addWidget(m_shaderPathEdit);
	shaderLayout->addWidget(shaderBrowseBtn);
	editorLayout->addRow(QStringLiteral("Shader Path:"), shaderLayout);
	
	m_soundPathEdit = new QLineEdit(editorTab);
	auto* soundBrowseBtn = new QPushButton(QStringLiteral("Browse..."), editorTab);
	auto* soundLayout = new QHBoxLayout();
	soundLayout->addWidget(m_soundPathEdit);
	soundLayout->addWidget(soundBrowseBtn);
	editorLayout->addRow(QStringLiteral("Sound Path:"), soundLayout);
	
	connect(textureBrowseBtn, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Texture Directory"));
		if (!path.isEmpty()) {
			m_texturePathEdit->setText(path);
		}
	});
	
	connect(modelBrowseBtn, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Model Directory"));
		if (!path.isEmpty()) {
			m_modelPathEdit->setText(path);
		}
	});
	
	connect(mapBrowseBtn, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Map Directory"));
		if (!path.isEmpty()) {
			m_mapPathEdit->setText(path);
		}
	});
	
	connect(gameBrowseBtn, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Game Directory"));
		if (!path.isEmpty()) {
			m_gamePathEdit->setText(path);
		}
	});
	
	connect(shaderBrowseBtn, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Shader Directory"));
		if (!path.isEmpty()) {
			m_shaderPathEdit->setText(path);
		}
	});
	
	connect(soundBrowseBtn, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Sound Directory"));
		if (!path.isEmpty()) {
			m_soundPathEdit->setText(path);
		}
	});
	
	editorScroll->setWidget(editorTab);
	m_tabs->addTab(editorScroll, QStringLiteral("Editor"));
	
	// Build tab - wrapped in scroll area
	auto* buildScroll = new QScrollArea();
	buildScroll->setWidgetResizable(true);
	buildScroll->setFrameShape(QFrame::NoFrame);
	auto* buildTab = new QWidget();
	auto* buildLayout = new QFormLayout(buildTab);
	
	m_q3map2PathEdit = new QLineEdit(buildTab);
	auto* q3map2BrowseBtn = new QPushButton(QStringLiteral("Browse..."), buildTab);
	auto* q3map2Layout = new QHBoxLayout();
	q3map2Layout->addWidget(m_q3map2PathEdit);
	q3map2Layout->addWidget(q3map2BrowseBtn);
	buildLayout->addRow(QStringLiteral("Q3Map2 Path:"), q3map2Layout);
	
	m_compilerOptionsEdit = new QLineEdit(buildTab);
	m_compilerOptionsEdit->setPlaceholderText(QStringLiteral("-v -meta"));
	buildLayout->addRow(QStringLiteral("Compiler Options:"), m_compilerOptionsEdit);
	
	connect(q3map2BrowseBtn, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Select Q3Map2"), QString(), 
			QStringLiteral("Executables (*);;All Files (*.*)"));
		if (!path.isEmpty()) {
			m_q3map2PathEdit->setText(path);
		}
	});
	
	buildScroll->setWidget(buildTab);
	m_tabs->addTab(buildScroll, QStringLiteral("Build"));
	
	// Performance tab - wrapped in scroll area
	auto* performanceScroll = new QScrollArea();
	performanceScroll->setWidgetResizable(true);
	performanceScroll->setFrameShape(QFrame::NoFrame);
	auto* performanceTab = new QWidget();
	auto* performanceLayout = new QFormLayout(performanceTab);
	
	m_renderQualityCombo = new QComboBox(performanceTab);
	m_renderQualityCombo->addItems({QStringLiteral("Low"), QStringLiteral("Medium"), QStringLiteral("High"), QStringLiteral("Ultra")});
	performanceLayout->addRow(QStringLiteral("Render Quality:"), m_renderQualityCombo);
	
	m_maxFPSSpin = new QSpinBox(performanceTab);
	m_maxFPSSpin->setRange(30, 300);
	m_maxFPSSpin->setValue(60);
	m_maxFPSSpin->setSuffix(QStringLiteral(" FPS"));
	performanceLayout->addRow(QStringLiteral("Max FPS:"), m_maxFPSSpin);
	
	m_vsyncCheck = new QCheckBox(performanceTab);
	m_vsyncCheck->setText(QStringLiteral("Enable VSync"));
	performanceLayout->addRow(m_vsyncCheck);
	
	performanceLayout->addRow(new QLabel(QStringLiteral("<b>Anti-Aliasing:</b>")));
	
	m_multisampleCheck = new QCheckBox(performanceTab);
	m_multisampleCheck->setText(QStringLiteral("Enable Multisampling"));
	performanceLayout->addRow(m_multisampleCheck);
	
	m_multisampleSamplesSpin = new QSpinBox(performanceTab);
	m_multisampleSamplesSpin->setRange(2, 16);
	m_multisampleSamplesSpin->setValue(4);
	m_multisampleSamplesSpin->setSuffix(QStringLiteral("x"));
	performanceLayout->addRow(QStringLiteral("Multisample Samples:"), m_multisampleSamplesSpin);
	
	performanceLayout->addRow(new QLabel(QStringLiteral("<b>Texture Quality:</b>")));
	
	m_anisotropicFilteringCheck = new QCheckBox(performanceTab);
	m_anisotropicFilteringCheck->setText(QStringLiteral("Enable Anisotropic Filtering"));
	performanceLayout->addRow(m_anisotropicFilteringCheck);
	
	m_anisotropicLevelSpin = new QSpinBox(performanceTab);
	m_anisotropicLevelSpin->setRange(2, 16);
	m_anisotropicLevelSpin->setValue(8);
	m_anisotropicLevelSpin->setSuffix(QStringLiteral("x"));
	performanceLayout->addRow(QStringLiteral("Anisotropic Level:"), m_anisotropicLevelSpin);
	
	m_useTextureCompressionCheck = new QCheckBox(performanceTab);
	m_useTextureCompressionCheck->setText(QStringLiteral("Use Texture Compression"));
	performanceLayout->addRow(m_useTextureCompressionCheck);
	
	m_textureCacheSizeSpin = new QSpinBox(performanceTab);
	m_textureCacheSizeSpin->setRange(64, 2048);
	m_textureCacheSizeSpin->setValue(512);
	m_textureCacheSizeSpin->setSuffix(QStringLiteral(" MB"));
	performanceLayout->addRow(QStringLiteral("Texture Cache Size:"), m_textureCacheSizeSpin);
	
	performanceScroll->setWidget(performanceTab);
	m_tabs->addTab(performanceScroll, QStringLiteral("Performance"));
	
	// Colors tab - wrapped in scroll area
	auto* colorsScroll = new QScrollArea();
	colorsScroll->setWidgetResizable(true);
	colorsScroll->setFrameShape(QFrame::NoFrame);
	auto* colorsTab = new QWidget();
	auto* colorsLayout = new QFormLayout(colorsTab);
	
	// Selection color
	m_selectionColorBtn = new QPushButton(colorsTab);
	m_selectionColorBtn->setText(QStringLiteral("Selection Color"));
	m_selectionColorBtn->setMinimumHeight(30);
	colorsLayout->addRow(QStringLiteral("Selection:"), m_selectionColorBtn);
	
	// Grid color
	m_gridColorBtn = new QPushButton(colorsTab);
	m_gridColorBtn->setText(QStringLiteral("Grid Color"));
	m_gridColorBtn->setMinimumHeight(30);
	colorsLayout->addRow(QStringLiteral("Grid:"), m_gridColorBtn);
	
	// Major grid color
	m_gridMajorColorBtn = new QPushButton(colorsTab);
	m_gridMajorColorBtn->setText(QStringLiteral("Major Grid Color"));
	m_gridMajorColorBtn->setMinimumHeight(30);
	colorsLayout->addRow(QStringLiteral("Major Grid:"), m_gridMajorColorBtn);
	
	// Background color
	m_backgroundColorBtn = new QPushButton(colorsTab);
	m_backgroundColorBtn->setText(QStringLiteral("Background Color"));
	m_backgroundColorBtn->setMinimumHeight(30);
	colorsLayout->addRow(QStringLiteral("Background:"), m_backgroundColorBtn);
	
	// Connect color buttons
	connect(m_selectionColorBtn, &QPushButton::clicked, this, [this]() {
		QColor color = QColorDialog::getColor(m_selectionColor, this, QStringLiteral("Select Selection Color"));
		if (color.isValid()) {
			m_selectionColor = color;
			updateColorButton(m_selectionColorBtn, color);
		}
	});
	
	connect(m_gridColorBtn, &QPushButton::clicked, this, [this]() {
		QColor color = QColorDialog::getColor(m_gridColor, this, QStringLiteral("Select Grid Color"));
		if (color.isValid()) {
			m_gridColor = color;
			updateColorButton(m_gridColorBtn, color);
		}
	});
	
	connect(m_gridMajorColorBtn, &QPushButton::clicked, this, [this]() {
		QColor color = QColorDialog::getColor(m_gridMajorColor, this, QStringLiteral("Select Major Grid Color"));
		if (color.isValid()) {
			m_gridMajorColor = color;
			updateColorButton(m_gridMajorColorBtn, color);
		}
	});
	
	connect(m_backgroundColorBtn, &QPushButton::clicked, this, [this]() {
		QColor color = QColorDialog::getColor(m_backgroundColor, this, QStringLiteral("Select Background Color"));
		if (color.isValid()) {
			m_backgroundColor = color;
			updateColorButton(m_backgroundColorBtn, color);
		}
	});
	
	colorsScroll->setWidget(colorsTab);
	m_tabs->addTab(colorsScroll, QStringLiteral("Colors"));
	
	mainLayout->addWidget(m_tabs);
	
	// Buttons
	auto* buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();
	auto* applyBtn = new QPushButton(QStringLiteral("Apply"), this);
	auto* okBtn = new QPushButton(QStringLiteral("OK"), this);
	auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
	buttonLayout->addWidget(applyBtn);
	buttonLayout->addWidget(okBtn);
	buttonLayout->addWidget(cancelBtn);
	mainLayout->addLayout(buttonLayout);
	
	connect(applyBtn, &QPushButton::clicked, this, &PreferencesDialog::onApply);
	connect(okBtn, &QPushButton::clicked, this, &PreferencesDialog::onOk);
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void PreferencesDialog::loadSettings() {
	QSettings settings;
	
	// General
	m_autosaveCheck->setChecked(settings.value("autosave/enabled", true).toBool());
	m_autosaveIntervalSpin->setValue(settings.value("autosave/interval", 5).toInt());
	m_loadLastMapCheck->setChecked(settings.value("general/loadLastMap", true).toBool());
	m_snapToGridCheck->setChecked(settings.value("viewport/snapToGrid", true).toBool());
	m_confirmOnExitCheck->setChecked(settings.value("general/confirmOnExit", false).toBool());
	m_showTooltipsCheck->setChecked(settings.value("general/showTooltips", true).toBool());
	m_undoLimitSpin->setValue(settings.value("general/undoLimit", 100).toInt());
	m_autosaveBackupCountSpin->setValue(settings.value("autosave/backupCount", 5).toInt());
	
	// Viewport
	m_gridSizeSpin->setValue(settings.value("viewport/gridSize", 32.0).toDouble());
	m_show3DGridCheck->setChecked(settings.value("viewport/show3DGrid", false).toBool());
	m_fovSpin->setValue(settings.value("viewport/fov", 75).toInt());
	m_showAxesCheck->setChecked(settings.value("viewport/showAxes", true).toBool());
	m_showStatsCheck->setChecked(settings.value("viewport/showStats", true).toBool());
	m_showGridCheck->setChecked(settings.value("viewport/showGrid", true).toBool());
	m_showFPSCheck->setChecked(settings.value("viewport/showFPS", true).toBool());
	m_wireframeModeCheck->setChecked(settings.value("viewport/wireframeMode", false).toBool());
	m_showMeasurementsCheck->setChecked(settings.value("viewport/showMeasurements", false).toBool());
	m_showMinimapCheck->setChecked(settings.value("viewport/showMinimap", false).toBool());
	m_showPerformanceWarningsCheck->setChecked(settings.value("viewport/showPerformanceWarnings", true).toBool());

	// Snapping
	m_snapToPointCheck->setChecked(settings.value("snapping/pointSnap", false).toBool());
	m_snapToEdgeCheck->setChecked(settings.value("snapping/edgeSnap", false).toBool());
	m_snapToFaceCheck->setChecked(settings.value("snapping/faceSnap", false).toBool());
	m_snapThresholdSpin->setValue(settings.value("snapping/threshold", 0.5).toDouble());
	
	// Camera/Viewport movement
	m_cameraMoveSpeedSpin->setValue(settings.value("camera/moveSpeed", 1.0).toDouble());
	m_cameraRotateSpeedSpin->setValue(settings.value("camera/rotateSpeed", 1.0).toDouble());
	m_mouseSensitivitySpin->setValue(settings.value("camera/mouseSensitivity", 1.0).toDouble());
	m_invertMouseYCheck->setChecked(settings.value("camera/invertMouseY", false).toBool());
	m_cameraAccelerationSpin->setValue(settings.value("camera/acceleration", 0.0).toDouble());
	m_defaultCameraDistanceSpin->setValue(settings.value("camera/defaultDistance", 512.0).toDouble());
	
	// Grid appearance
	m_gridOpacitySpin->setValue(settings.value("grid/opacity", 0.5).toDouble());
	m_gridSubdivisionsSpin->setValue(settings.value("grid/subdivisions", 4).toInt());
	m_showGridInPerspectiveCheck->setChecked(settings.value("grid/showInPerspective", false).toBool());
	
	// Selection appearance
	m_showSelectionBoundsCheck->setChecked(settings.value("selection/showBounds", true).toBool());
	m_selectionOutlineWidthSpin->setValue(settings.value("selection/outlineWidth", 2.0).toDouble());
	
	// Display/Rendering
	m_showTexturesCheck->setChecked(settings.value("display/showTextures", true).toBool());
	m_showEntityModelsCheck->setChecked(settings.value("display/showEntityModels", true).toBool());
	m_showEntityNamesCheck->setChecked(settings.value("display/showEntityNames", false).toBool());
	m_showBrushWireframesCheck->setChecked(settings.value("display/showBrushWireframes", false).toBool());
	m_showLightRadiusCheck->setChecked(settings.value("display/showLightRadius", true).toBool());
	m_lightIntensitySpin->setValue(settings.value("display/lightIntensity", 1.0).toDouble());
	m_ambientLightSpin->setValue(settings.value("display/ambientLight", 0.3).toDouble());
	m_textureFilteringCombo->setCurrentText(settings.value("display/textureFiltering", "Linear").toString());
	
	// Keyboard/Mouse
	m_mouseWheelZoomSpeedSpin->setValue(settings.value("mouse/wheelZoomSpeed", 1.0).toDouble());
	m_panSpeedSpin->setValue(settings.value("mouse/panSpeed", 1.0).toDouble());
	m_rotationSpeedSpin->setValue(settings.value("mouse/rotationSpeed", 1.0).toDouble());

	// Editor
	QString savedTheme = settings.value("editor/theme", "Default Dark").toString();
	// Make sure the saved theme exists, fallback to a valid theme if not
	QStringList availableThemes = ThemeManager::instance().availableThemes();
	if (!availableThemes.contains(savedTheme)) {
		// Try Default Dark first
		if (availableThemes.contains("Default Dark")) {
			savedTheme = "Default Dark";
		} else if (!availableThemes.isEmpty()) {
			// Use first available theme as fallback
			savedTheme = availableThemes.first();
		}
	}
	m_themeCombo->setCurrentText(savedTheme);
	m_fontCombo->setCurrentText(settings.value("editor/font", "Default").toString());
	m_fontSizeSpin->setValue(settings.value("editor/fontSize", 10).toInt());
	m_texturePathEdit->setText(settings.value("editor/texturePath", "").toString());
	m_modelPathEdit->setText(settings.value("editor/modelPath", "").toString());
	m_showCoordinateDisplayCheck->setChecked(settings.value("editor/showCoordinateDisplay", true).toBool());
	m_showCrosshairCheck->setChecked(settings.value("editor/showCrosshair", true).toBool());
	m_mapPathEdit->setText(settings.value("editor/mapPath", "").toString());
	m_gamePathEdit->setText(settings.value("editor/gamePath", "").toString());
	m_shaderPathEdit->setText(settings.value("editor/shaderPath", "").toString());
	m_soundPathEdit->setText(settings.value("editor/soundPath", "").toString());
	
	// Colors
	m_selectionColor = settings.value("colors/selection", QColor(100, 150, 255)).value<QColor>();
	m_gridColor = settings.value("colors/grid", QColor(70, 70, 70)).value<QColor>();
	m_gridMajorColor = settings.value("colors/gridMajor", QColor(100, 100, 100)).value<QColor>();
	m_backgroundColor = settings.value("colors/background", QColor(32, 32, 32)).value<QColor>();
	updateColorButton(m_selectionColorBtn, m_selectionColor);
	updateColorButton(m_gridColorBtn, m_gridColor);
	updateColorButton(m_gridMajorColorBtn, m_gridMajorColor);
	updateColorButton(m_backgroundColorBtn, m_backgroundColor);
	
	// Performance
	m_renderQualityCombo->setCurrentText(settings.value("performance/renderQuality", "High").toString());
	m_maxFPSSpin->setValue(settings.value("performance/maxFPS", 60).toInt());
	m_vsyncCheck->setChecked(settings.value("performance/vsync", false).toBool());
	m_multisampleCheck->setChecked(settings.value("performance/multisample", true).toBool());
	m_multisampleSamplesSpin->setValue(settings.value("performance/multisampleSamples", 4).toInt());
	m_anisotropicFilteringCheck->setChecked(settings.value("performance/anisotropicFiltering", true).toBool());
	m_anisotropicLevelSpin->setValue(settings.value("performance/anisotropicLevel", 8).toInt());
	m_useTextureCompressionCheck->setChecked(settings.value("performance/useTextureCompression", true).toBool());
	m_textureCacheSizeSpin->setValue(settings.value("performance/textureCacheSize", 512).toInt());
	
	// Build
	m_q3map2PathEdit->setText(settings.value("build/q3map2Path", "").toString());
	m_compilerOptionsEdit->setText(settings.value("build/compilerOptions", "-v -meta").toString());
}

void PreferencesDialog::saveSettings() {
	QSettings settings;
	
	// General
	settings.setValue("autosave/enabled", m_autosaveCheck->isChecked());
	settings.setValue("autosave/interval", m_autosaveIntervalSpin->value());
	settings.setValue("autosave/backupCount", m_autosaveBackupCountSpin->value());
	settings.setValue("general/loadLastMap", m_loadLastMapCheck->isChecked());
	settings.setValue("general/confirmOnExit", m_confirmOnExitCheck->isChecked());
	settings.setValue("general/showTooltips", m_showTooltipsCheck->isChecked());
	settings.setValue("general/undoLimit", m_undoLimitSpin->value());
	settings.setValue("viewport/snapToGrid", m_snapToGridCheck->isChecked());
	
	// Viewport
	settings.setValue("viewport/gridSize", m_gridSizeSpin->value());
	settings.setValue("viewport/show3DGrid", m_show3DGridCheck->isChecked());
	settings.setValue("viewport/fov", m_fovSpin->value());
	settings.setValue("viewport/showAxes", m_showAxesCheck->isChecked());
	settings.setValue("viewport/showStats", m_showStatsCheck->isChecked());
	settings.setValue("viewport/showGrid", m_showGridCheck->isChecked());
	settings.setValue("viewport/showFPS", m_showFPSCheck->isChecked());
	settings.setValue("viewport/wireframeMode", m_wireframeModeCheck->isChecked());
	settings.setValue("viewport/showMeasurements", m_showMeasurementsCheck->isChecked());
	settings.setValue("viewport/showMinimap", m_showMinimapCheck->isChecked());
	settings.setValue("viewport/showPerformanceWarnings", m_showPerformanceWarningsCheck->isChecked());

	// Snapping
	settings.setValue("snapping/pointSnap", m_snapToPointCheck->isChecked());
	settings.setValue("snapping/edgeSnap", m_snapToEdgeCheck->isChecked());
	settings.setValue("snapping/faceSnap", m_snapToFaceCheck->isChecked());
	settings.setValue("snapping/threshold", m_snapThresholdSpin->value());
	
	// Camera/Viewport movement
	settings.setValue("camera/moveSpeed", m_cameraMoveSpeedSpin->value());
	settings.setValue("camera/rotateSpeed", m_cameraRotateSpeedSpin->value());
	settings.setValue("camera/mouseSensitivity", m_mouseSensitivitySpin->value());
	settings.setValue("camera/invertMouseY", m_invertMouseYCheck->isChecked());
	settings.setValue("camera/acceleration", m_cameraAccelerationSpin->value());
	settings.setValue("camera/defaultDistance", m_defaultCameraDistanceSpin->value());
	
	// Grid appearance
	settings.setValue("grid/opacity", m_gridOpacitySpin->value());
	settings.setValue("grid/subdivisions", m_gridSubdivisionsSpin->value());
	settings.setValue("grid/showInPerspective", m_showGridInPerspectiveCheck->isChecked());
	
	// Selection appearance
	settings.setValue("selection/showBounds", m_showSelectionBoundsCheck->isChecked());
	settings.setValue("selection/outlineWidth", m_selectionOutlineWidthSpin->value());
	
	// Display/Rendering
	settings.setValue("display/showTextures", m_showTexturesCheck->isChecked());
	settings.setValue("display/showEntityModels", m_showEntityModelsCheck->isChecked());
	settings.setValue("display/showEntityNames", m_showEntityNamesCheck->isChecked());
	settings.setValue("display/showBrushWireframes", m_showBrushWireframesCheck->isChecked());
	settings.setValue("display/showLightRadius", m_showLightRadiusCheck->isChecked());
	settings.setValue("display/lightIntensity", m_lightIntensitySpin->value());
	settings.setValue("display/ambientLight", m_ambientLightSpin->value());
	settings.setValue("display/textureFiltering", m_textureFilteringCombo->currentText());
	
	// Keyboard/Mouse
	settings.setValue("mouse/wheelZoomSpeed", m_mouseWheelZoomSpeedSpin->value());
	settings.setValue("mouse/panSpeed", m_panSpeedSpin->value());
	settings.setValue("mouse/rotationSpeed", m_rotationSpeedSpin->value());
	
	// Editor
	QString selectedTheme = m_themeCombo->currentText();
	settings.setValue("editor/theme", selectedTheme);
	// Apply the theme immediately
	ThemeManager::instance().applyTheme(selectedTheme);
	settings.setValue("editor/font", m_fontCombo->currentText());
	settings.setValue("editor/fontSize", m_fontSizeSpin->value());
	settings.setValue("editor/texturePath", m_texturePathEdit->text());
	settings.setValue("editor/modelPath", m_modelPathEdit->text());
	settings.setValue("editor/showCoordinateDisplay", m_showCoordinateDisplayCheck->isChecked());
	settings.setValue("editor/showCrosshair", m_showCrosshairCheck->isChecked());
	settings.setValue("editor/mapPath", m_mapPathEdit->text());
	settings.setValue("editor/gamePath", m_gamePathEdit->text());
	settings.setValue("editor/shaderPath", m_shaderPathEdit->text());
	settings.setValue("editor/soundPath", m_soundPathEdit->text());
	
	// Colors
	settings.setValue("colors/selection", m_selectionColor);
	settings.setValue("colors/grid", m_gridColor);
	settings.setValue("colors/gridMajor", m_gridMajorColor);
	settings.setValue("colors/background", m_backgroundColor);
	
	// Performance
	settings.setValue("performance/renderQuality", m_renderQualityCombo->currentText());
	settings.setValue("performance/maxFPS", m_maxFPSSpin->value());
	settings.setValue("performance/vsync", m_vsyncCheck->isChecked());
	settings.setValue("performance/multisample", m_multisampleCheck->isChecked());
	settings.setValue("performance/multisampleSamples", m_multisampleSamplesSpin->value());
	settings.setValue("performance/anisotropicFiltering", m_anisotropicFilteringCheck->isChecked());
	settings.setValue("performance/anisotropicLevel", m_anisotropicLevelSpin->value());
	settings.setValue("performance/useTextureCompression", m_useTextureCompressionCheck->isChecked());
	settings.setValue("performance/textureCacheSize", m_textureCacheSizeSpin->value());
	
	// Build
	settings.setValue("build/q3map2Path", m_q3map2PathEdit->text());
	settings.setValue("build/compilerOptions", m_compilerOptionsEdit->text());

	// Update snapping system
	SnappingSystem::instance().setSnapMode(SnapMode::Grid, m_snapToGridCheck->isChecked());
	SnappingSystem::instance().setSnapMode(SnapMode::Point, m_snapToPointCheck->isChecked());
	SnappingSystem::instance().setSnapMode(SnapMode::Edge, m_snapToEdgeCheck->isChecked());
	SnappingSystem::instance().setSnapMode(SnapMode::Face, m_snapToFaceCheck->isChecked());
	SnappingSystem::instance().setSnapThreshold(m_snapThresholdSpin->value());

	// Update viewport settings - signal to main window to apply to all viewports
	// This would need to be implemented with signals/slots in a full implementation
	
	// Ensure settings are written to disk immediately
	settings.sync();
}

void PreferencesDialog::onApply() {
	saveSettings();
}

void PreferencesDialog::onOk() {
	saveSettings();
	accept();
}

void PreferencesDialog::updateColorButton(QPushButton* button, const QColor& color) {
	QPixmap pixmap(20, 20);
	pixmap.fill(color);
	button->setIcon(QIcon(pixmap));
	button->setText(QStringLiteral("RGB(%1, %2, %3)").arg(color.red()).arg(color.green()).arg(color.blue()));
}
