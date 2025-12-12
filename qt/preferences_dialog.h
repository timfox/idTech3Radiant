#pragma once

#include <QDialog>

class QTabWidget;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QComboBox;

// Preferences/Settings Dialog (GTK Radiant feature)
class PreferencesDialog : public QDialog {
	Q_OBJECT
public:
	explicit PreferencesDialog(QWidget* parent = nullptr);
	
	void loadSettings();
	void saveSettings();

private Q_SLOTS:
	void onApply();
	void onOk();

private:
	void setupUI();
	
	QTabWidget* m_tabs = nullptr;
	
	// General settings
	QCheckBox* m_autosaveCheck = nullptr;
	QSpinBox* m_autosaveIntervalSpin = nullptr;
	QCheckBox* m_loadLastMapCheck = nullptr;
	QCheckBox* m_showGridCheck = nullptr;
	QCheckBox* m_snapToGridCheck = nullptr;
	
	// Viewport settings
	QDoubleSpinBox* m_gridSizeSpin = nullptr;
	QCheckBox* m_show3DGridCheck = nullptr;
	QSpinBox* m_fovSpin = nullptr;
	QCheckBox* m_showAxesCheck = nullptr;
	
	// Editor settings
	QComboBox* m_themeCombo = nullptr;
	QComboBox* m_fontCombo = nullptr;
	QSpinBox* m_fontSizeSpin = nullptr;
	QLineEdit* m_texturePathEdit = nullptr;
	QLineEdit* m_modelPathEdit = nullptr;
	
	// Build settings
	QLineEdit* m_q3map2PathEdit = nullptr;
	QLineEdit* m_compilerOptionsEdit = nullptr;
};
