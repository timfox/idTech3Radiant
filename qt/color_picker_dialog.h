#pragma once

#include <QDialog>
#include <QColor>
#include <QVector>

QT_BEGIN_NAMESPACE
class QSlider;
class QSpinBox;
class QPushButton;
class QLabel;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QHBoxLayout;
class QLayout;
QT_END_NAMESPACE

class ColorPickerDialog : public QDialog {
	Q_OBJECT
public:
	explicit ColorPickerDialog(QWidget* parent = nullptr);
	QColor selectedColor() const { return m_color; }
	void setColor(const QColor& color);
	
	// Palette management
	void savePalette(const QString& mapPath);
	void loadPalette(const QString& mapPath);
	
	static QColor getColor(const QColor& initial = Qt::white, QWidget* parent = nullptr);

private Q_SLOTS:
	void onColorModeChanged(int index);
	void onRgbChanged();
	void onHsvChanged();
	void onColorChanged();
	void onPaletteItemClicked(QListWidgetItem* item);
	void addToPalette();
	void removeFromPalette();

private:
	QWidget* createLayoutWidget(QLayout* layout);
	void updateColor();
	void updateRgbControls();
	void updateHsvControls();
	void updatePreview();
	
	QColor m_color;
	
	// RGB controls
	QSlider* m_redSlider = nullptr;
	QSlider* m_greenSlider = nullptr;
	QSlider* m_blueSlider = nullptr;
	QSlider* m_alphaSlider = nullptr;
	QSpinBox* m_redSpin = nullptr;
	QSpinBox* m_greenSpin = nullptr;
	QSpinBox* m_blueSpin = nullptr;
	QSpinBox* m_alphaSpin = nullptr;
	
	// HSV controls
	QSlider* m_hueSlider = nullptr;
	QSlider* m_saturationSlider = nullptr;
	QSlider* m_valueSlider = nullptr;
	QSpinBox* m_hueSpin = nullptr;
	QSpinBox* m_saturationSpin = nullptr;
	QSpinBox* m_valueSpin = nullptr;
	
	// UI elements
	QLabel* m_previewLabel = nullptr;
	QComboBox* m_colorModeCombo = nullptr;
	QListWidget* m_paletteList = nullptr;
	QPushButton* m_addPaletteBtn = nullptr;
	QPushButton* m_removePaletteBtn = nullptr;
	
	QVector<QColor> m_palette;
	bool m_updating = false;
};
