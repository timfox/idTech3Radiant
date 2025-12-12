#include "preferences_dialog.h"

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
	
	m_showGridCheck = new QCheckBox(generalTab);
	m_showGridCheck->setText(QStringLiteral("Show Grid"));
	generalLayout->addRow(m_showGridCheck);
	
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
	
	m_tabs->addTab(viewportTab, QStringLiteral("Viewport"));
	
	// Editor tab
	auto* editorTab = new QWidget(this);
	auto* editorLayout = new QFormLayout(editorTab);
	
	m_themeCombo = new QComboBox(editorTab);
	m_themeCombo->addItems({QStringLiteral("Dark"), QStringLiteral("Light"), QStringLiteral("Classic")});
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
	m_showGridCheck->setChecked(settings.value("viewport/showGrid", true).toBool());
	m_snapToGridCheck->setChecked(settings.value("viewport/snapToGrid", true).toBool());
	
	// Viewport
	m_gridSizeSpin->setValue(settings.value("viewport/gridSize", 32.0).toDouble());
	m_show3DGridCheck->setChecked(settings.value("viewport/show3DGrid", false).toBool());
	m_fovSpin->setValue(settings.value("viewport/fov", 75).toInt());
	m_showAxesCheck->setChecked(settings.value("viewport/showAxes", true).toBool());
	
	// Editor
	m_themeCombo->setCurrentText(settings.value("editor/theme", "Dark").toString());
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
	settings.setValue("viewport/showGrid", m_showGridCheck->isChecked());
	settings.setValue("viewport/snapToGrid", m_snapToGridCheck->isChecked());
	
	// Viewport
	settings.setValue("viewport/gridSize", m_gridSizeSpin->value());
	settings.setValue("viewport/show3DGrid", m_show3DGridCheck->isChecked());
	settings.setValue("viewport/fov", m_fovSpin->value());
	settings.setValue("viewport/showAxes", m_showAxesCheck->isChecked());
	
	// Editor
	settings.setValue("editor/theme", m_themeCombo->currentText());
	settings.setValue("editor/font", m_fontCombo->currentText());
	settings.setValue("editor/fontSize", m_fontSizeSpin->value());
	settings.setValue("editor/texturePath", m_texturePathEdit->text());
	settings.setValue("editor/modelPath", m_modelPathEdit->text());
	
	// Build
	settings.setValue("build/q3map2Path", m_q3map2PathEdit->text());
	settings.setValue("build/compilerOptions", m_compilerOptionsEdit->text());
}

void PreferencesDialog::onApply() {
	saveSettings();
}

void PreferencesDialog::onOk() {
	saveSettings();
	accept();
}
