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
	void updateColorButton(QPushButton* button, const QColor& color);
	
	QTabWidget* m_tabs = nullptr;
	
	// General settings
	QCheckBox* m_autosaveCheck = nullptr;
	QSpinBox* m_autosaveIntervalSpin = nullptr;
	QCheckBox* m_loadLastMapCheck = nullptr;
	QCheckBox* m_snapToGridCheck = nullptr;
	
	// Viewport settings
	QDoubleSpinBox* m_gridSizeSpin = nullptr;
	QCheckBox* m_show3DGridCheck = nullptr;
	QSpinBox* m_fovSpin = nullptr;
	QCheckBox* m_showAxesCheck = nullptr;

	// Snapping settings
	QCheckBox* m_snapToPointCheck = nullptr;
	QCheckBox* m_snapToEdgeCheck = nullptr;
	QCheckBox* m_snapToFaceCheck = nullptr;
	QDoubleSpinBox* m_snapThresholdSpin = nullptr;

	// Viewport display settings
	QCheckBox* m_showStatsCheck = nullptr;
	QCheckBox* m_showGridCheck = nullptr;
	QCheckBox* m_showFPSCheck = nullptr;
	QCheckBox* m_wireframeModeCheck = nullptr;
	QCheckBox* m_showMeasurementsCheck = nullptr;
	QCheckBox* m_showMinimapCheck = nullptr;
	QCheckBox* m_showPerformanceWarningsCheck = nullptr;
	
	// Editor settings
	QComboBox* m_themeCombo = nullptr;
	QComboBox* m_fontCombo = nullptr;
	QSpinBox* m_fontSizeSpin = nullptr;
	QLineEdit* m_texturePathEdit = nullptr;
	QLineEdit* m_modelPathEdit = nullptr;
	
	// Build settings
	QLineEdit* m_q3map2PathEdit = nullptr;
	QLineEdit* m_compilerOptionsEdit = nullptr;
	
	// Camera/Viewport movement settings
	QDoubleSpinBox* m_cameraMoveSpeedSpin = nullptr;
	QDoubleSpinBox* m_cameraRotateSpeedSpin = nullptr;
	QDoubleSpinBox* m_mouseSensitivitySpin = nullptr;
	QCheckBox* m_invertMouseYCheck = nullptr;
	QDoubleSpinBox* m_cameraAccelerationSpin = nullptr;
	QDoubleSpinBox* m_defaultCameraDistanceSpin = nullptr;
	
	// Selection appearance settings
	QCheckBox* m_showSelectionBoundsCheck = nullptr;
	QDoubleSpinBox* m_selectionOutlineWidthSpin = nullptr;
	
	// Grid appearance settings
	QDoubleSpinBox* m_gridOpacitySpin = nullptr;
	QSpinBox* m_gridSubdivisionsSpin = nullptr;
	QCheckBox* m_showGridInPerspectiveCheck = nullptr;
	
	// Display/Rendering settings
	QCheckBox* m_showTexturesCheck = nullptr;
	QCheckBox* m_showEntityModelsCheck = nullptr;
	QCheckBox* m_showEntityNamesCheck = nullptr;
	QCheckBox* m_showBrushWireframesCheck = nullptr;
	QCheckBox* m_showLightRadiusCheck = nullptr;
	QDoubleSpinBox* m_lightIntensitySpin = nullptr;
	QDoubleSpinBox* m_ambientLightSpin = nullptr;
	QComboBox* m_textureFilteringCombo = nullptr;
	
	// Editor behavior settings
	QSpinBox* m_undoLimitSpin = nullptr;
	QSpinBox* m_autosaveBackupCountSpin = nullptr;
	QCheckBox* m_confirmOnExitCheck = nullptr;
	QCheckBox* m_showTooltipsCheck = nullptr;
	QCheckBox* m_showCoordinateDisplayCheck = nullptr;
	QCheckBox* m_showCrosshairCheck = nullptr;
	
	// Keyboard/Mouse settings
	QDoubleSpinBox* m_mouseWheelZoomSpeedSpin = nullptr;
	QDoubleSpinBox* m_panSpeedSpin = nullptr;
	QDoubleSpinBox* m_rotationSpeedSpin = nullptr;
	
	// Additional file paths
	QLineEdit* m_mapPathEdit = nullptr;
	QLineEdit* m_gamePathEdit = nullptr;
	QLineEdit* m_shaderPathEdit = nullptr;
	QLineEdit* m_soundPathEdit = nullptr;
	
	// Color settings
	QPushButton* m_selectionColorBtn = nullptr;
	QPushButton* m_gridColorBtn = nullptr;
	QPushButton* m_gridMajorColorBtn = nullptr;
	QPushButton* m_backgroundColorBtn = nullptr;
	QColor m_selectionColor;
	QColor m_gridColor;
	QColor m_gridMajorColor;
	QColor m_backgroundColor;
	
	// Performance settings
	QComboBox* m_renderQualityCombo = nullptr;
	QSpinBox* m_maxFPSSpin = nullptr;
	QCheckBox* m_vsyncCheck = nullptr;
	QCheckBox* m_multisampleCheck = nullptr;
	QSpinBox* m_multisampleSamplesSpin = nullptr;
	QCheckBox* m_anisotropicFilteringCheck = nullptr;
	QSpinBox* m_anisotropicLevelSpin = nullptr;
	QCheckBox* m_useTextureCompressionCheck = nullptr;
	QSpinBox* m_textureCacheSizeSpin = nullptr;
};
