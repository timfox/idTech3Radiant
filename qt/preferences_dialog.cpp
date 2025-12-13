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

PreferencesDialog::PreferencesDialog(QWidget* parent)
	: QDialog(parent) {
	setWindowTitle(QStringLiteral("Preferences"));
	setModal(true);
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
	              Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
	setSizeGripEnabled(true);
	resize(600, 500);
	
	setupUI();
	loadSettings();
}

void PreferencesDialog::setupUI() {
	auto* mainLayout = new QVBoxLayout(this);
	
	m_tabs = new QTabWidget(this);
	
	// General tab
	auto* generalTab = new QWidget(this);
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
	
	
	m_snapToGridCheck = new QCheckBox(generalTab);
	m_snapToGridCheck->setText(QStringLiteral("Snap to Grid"));
	generalLayout->addRow(m_snapToGridCheck);
	
	m_tabs->addTab(generalTab, QStringLiteral("General"));
	
	// Viewport tab
	auto* viewportTab = new QWidget(this);
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

	m_tabs->addTab(viewportTab, QStringLiteral("Viewport"));
	
	// Editor tab
	auto* editorTab = new QWidget(this);
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
	
	m_tabs->addTab(editorTab, QStringLiteral("Editor"));
	
	// Build tab
	auto* buildTab = new QWidget(this);
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
	
	m_tabs->addTab(buildTab, QStringLiteral("Build"));
	
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
	
	// Build
	m_q3map2PathEdit->setText(settings.value("build/q3map2Path", "").toString());
	m_compilerOptionsEdit->setText(settings.value("build/compilerOptions", "-v -meta").toString());
}

void PreferencesDialog::saveSettings() {
	QSettings settings;
	
	// General
	settings.setValue("autosave/enabled", m_autosaveCheck->isChecked());
	settings.setValue("autosave/interval", m_autosaveIntervalSpin->value());
	settings.setValue("general/loadLastMap", m_loadLastMapCheck->isChecked());
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
	
	// Editor
	QString selectedTheme = m_themeCombo->currentText();
	settings.setValue("editor/theme", selectedTheme);
	// Apply the theme immediately
	ThemeManager::instance().applyTheme(selectedTheme);
	settings.setValue("editor/font", m_fontCombo->currentText());
	settings.setValue("editor/fontSize", m_fontSizeSpin->value());
	settings.setValue("editor/texturePath", m_texturePathEdit->text());
	settings.setValue("editor/modelPath", m_modelPathEdit->text());
	
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
