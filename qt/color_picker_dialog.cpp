#include "color_picker_dialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QColorDialog>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QWidget>
#include <QLayout>

ColorPickerDialog::ColorPickerDialog(QWidget* parent)
	: QDialog(parent)
	, m_color(Qt::white)
{
	setWindowTitle(QStringLiteral("Color Picker"));
	setModal(true);
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
	              Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
	setSizeGripEnabled(true);
	resize(500, 400);
	
	auto* mainLayout = new QVBoxLayout(this);
	
	// Color mode selector
	auto* modeLayout = new QHBoxLayout();
	modeLayout->addWidget(new QLabel(QStringLiteral("Color Mode:")));
	m_colorModeCombo = new QComboBox(this);
	m_colorModeCombo->addItems({QStringLiteral("RGB"), QStringLiteral("HSV")});
	modeLayout->addWidget(m_colorModeCombo);
	modeLayout->addStretch();
	mainLayout->addLayout(modeLayout);
	
	// Preview
	m_previewLabel = new QLabel(this);
	m_previewLabel->setMinimumHeight(60);
	m_previewLabel->setStyleSheet(QStringLiteral("border: 2px solid black; background-color: white;"));
	mainLayout->addWidget(m_previewLabel);
	
	// RGB controls
	auto* rgbGroup = new QGroupBox(QStringLiteral("RGB"), this);
	auto* rgbLayout = new QFormLayout(rgbGroup);
	
	m_redSlider = new QSlider(Qt::Horizontal, this);
	m_redSlider->setRange(0, 255);
	m_redSpin = new QSpinBox(this);
	m_redSpin->setRange(0, 255);
	auto* redLayout = new QHBoxLayout();
	redLayout->addWidget(m_redSlider);
	redLayout->addWidget(m_redSpin);
	m_redSpin->setMaximumWidth(80);
	rgbLayout->addRow(QStringLiteral("Red:"), createLayoutWidget(redLayout));
	
	m_greenSlider = new QSlider(Qt::Horizontal, this);
	m_greenSlider->setRange(0, 255);
	m_greenSpin = new QSpinBox(this);
	m_greenSpin->setRange(0, 255);
	auto* greenLayout = new QHBoxLayout();
	greenLayout->addWidget(m_greenSlider);
	greenLayout->addWidget(m_greenSpin);
	m_greenSpin->setMaximumWidth(80);
	rgbLayout->addRow(QStringLiteral("Green:"), createLayoutWidget(greenLayout));
	
	m_blueSlider = new QSlider(Qt::Horizontal, this);
	m_blueSlider->setRange(0, 255);
	m_blueSpin = new QSpinBox(this);
	m_blueSpin->setRange(0, 255);
	auto* blueLayout = new QHBoxLayout();
	blueLayout->addWidget(m_blueSlider);
	blueLayout->addWidget(m_blueSpin);
	m_blueSpin->setMaximumWidth(80);
	rgbLayout->addRow(QStringLiteral("Blue:"), createLayoutWidget(blueLayout));
	
	m_alphaSlider = new QSlider(Qt::Horizontal, this);
	m_alphaSlider->setRange(0, 255);
	m_alphaSpin = new QSpinBox(this);
	m_alphaSpin->setRange(0, 255);
	auto* alphaLayout = new QHBoxLayout();
	alphaLayout->addWidget(m_alphaSlider);
	alphaLayout->addWidget(m_alphaSpin);
	m_alphaSpin->setMaximumWidth(80);
	rgbLayout->addRow(QStringLiteral("Alpha:"), createLayoutWidget(alphaLayout));
	
	mainLayout->addWidget(rgbGroup);
	
	// HSV controls (initially hidden)
	auto* hsvGroup = new QGroupBox(QStringLiteral("HSV"), this);
	auto* hsvLayout = new QFormLayout(hsvGroup);
	
	m_hueSlider = new QSlider(Qt::Horizontal, this);
	m_hueSlider->setRange(0, 359);
	m_hueSpin = new QSpinBox(this);
	m_hueSpin->setRange(0, 359);
	auto* hueLayout = new QHBoxLayout();
	hueLayout->addWidget(m_hueSlider);
	hueLayout->addWidget(m_hueSpin);
	m_hueSpin->setMaximumWidth(80);
	hsvLayout->addRow(QStringLiteral("Hue:"), createLayoutWidget(hueLayout));
	
	m_saturationSlider = new QSlider(Qt::Horizontal, this);
	m_saturationSlider->setRange(0, 255);
	m_saturationSpin = new QSpinBox(this);
	m_saturationSpin->setRange(0, 255);
	auto* satLayout = new QHBoxLayout();
	satLayout->addWidget(m_saturationSlider);
	satLayout->addWidget(m_saturationSpin);
	m_saturationSpin->setMaximumWidth(80);
	hsvLayout->addRow(QStringLiteral("Saturation:"), createLayoutWidget(satLayout));
	
	m_valueSlider = new QSlider(Qt::Horizontal, this);
	m_valueSlider->setRange(0, 255);
	m_valueSpin = new QSpinBox(this);
	m_valueSpin->setRange(0, 255);
	auto* valueLayout = new QHBoxLayout();
	valueLayout->addWidget(m_valueSlider);
	valueLayout->addWidget(m_valueSpin);
	m_valueSpin->setMaximumWidth(80);
	hsvLayout->addRow(QStringLiteral("Value:"), createLayoutWidget(valueLayout));
	
	mainLayout->addWidget(hsvGroup);
	hsvGroup->setVisible(false);
	
	// Palette
	auto* paletteGroup = new QGroupBox(QStringLiteral("Palette"), this);
	auto* paletteLayout = new QVBoxLayout(paletteGroup);
	
	m_paletteList = new QListWidget(this);
	m_paletteList->setMaximumHeight(100);
	paletteLayout->addWidget(m_paletteList);
	
	auto* paletteBtnLayout = new QHBoxLayout();
	m_addPaletteBtn = new QPushButton(QStringLiteral("Add to Palette"), this);
	m_removePaletteBtn = new QPushButton(QStringLiteral("Remove"), this);
	paletteBtnLayout->addWidget(m_addPaletteBtn);
	paletteBtnLayout->addWidget(m_removePaletteBtn);
	paletteBtnLayout->addStretch();
	paletteLayout->addLayout(paletteBtnLayout);
	
	mainLayout->addWidget(paletteGroup);
	
	// Buttons
	auto* buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();
	auto* okBtn = new QPushButton(QStringLiteral("OK"), this);
	auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
	buttonLayout->addWidget(okBtn);
	buttonLayout->addWidget(cancelBtn);
	mainLayout->addLayout(buttonLayout);
	
	// Connections
	connect(m_colorModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColorPickerDialog::onColorModeChanged);
	connect(m_redSlider, &QSlider::valueChanged, this, &ColorPickerDialog::onRgbChanged);
	connect(m_greenSlider, &QSlider::valueChanged, this, &ColorPickerDialog::onRgbChanged);
	connect(m_blueSlider, &QSlider::valueChanged, this, &ColorPickerDialog::onRgbChanged);
	connect(m_alphaSlider, &QSlider::valueChanged, this, &ColorPickerDialog::onRgbChanged);
	connect(m_redSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerDialog::onRgbChanged);
	connect(m_greenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerDialog::onRgbChanged);
	connect(m_blueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerDialog::onRgbChanged);
	connect(m_alphaSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerDialog::onRgbChanged);
	
	connect(m_hueSlider, &QSlider::valueChanged, this, &ColorPickerDialog::onHsvChanged);
	connect(m_saturationSlider, &QSlider::valueChanged, this, &ColorPickerDialog::onHsvChanged);
	connect(m_valueSlider, &QSlider::valueChanged, this, &ColorPickerDialog::onHsvChanged);
	connect(m_hueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerDialog::onHsvChanged);
	connect(m_saturationSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerDialog::onHsvChanged);
	connect(m_valueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerDialog::onHsvChanged);
	
	connect(m_paletteList, &QListWidget::itemClicked, this, &ColorPickerDialog::onPaletteItemClicked);
	connect(m_addPaletteBtn, &QPushButton::clicked, this, &ColorPickerDialog::addToPalette);
	connect(m_removePaletteBtn, &QPushButton::clicked, this, &ColorPickerDialog::removeFromPalette);
	
	connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
	
	updatePreview();
}

QWidget* ColorPickerDialog::createLayoutWidget(QLayout* layout) {
	auto* widget = new QWidget(this);
	widget->setLayout(layout);
	return widget;
}

void ColorPickerDialog::setColor(const QColor& color) {
	m_color = color;
	updateRgbControls();
	updateHsvControls();
	updatePreview();
}

void ColorPickerDialog::onColorModeChanged(int index) {
	bool rgbMode = (index == 0);
	// Show/hide appropriate controls
	// For simplicity, we'll keep both visible but could hide one
	Q_UNUSED(rgbMode);
}

void ColorPickerDialog::onRgbChanged() {
	if (m_updating) return;
	m_updating = true;
	
	m_color.setRed(m_redSlider->value());
	m_color.setGreen(m_greenSlider->value());
	m_color.setBlue(m_blueSlider->value());
	m_color.setAlpha(m_alphaSlider->value());
	
	updateHsvControls();
	updatePreview();
	
	m_updating = false;
}

void ColorPickerDialog::onHsvChanged() {
	if (m_updating) return;
	m_updating = true;
	
	m_color.setHsv(m_hueSlider->value(), m_saturationSlider->value(), m_valueSlider->value());
	
	updateRgbControls();
	updatePreview();
	
	m_updating = false;
}

void ColorPickerDialog::updateRgbControls() {
	m_redSlider->setValue(m_color.red());
	m_greenSlider->setValue(m_color.green());
	m_blueSlider->setValue(m_color.blue());
	m_alphaSlider->setValue(m_color.alpha());
	m_redSpin->setValue(m_color.red());
	m_greenSpin->setValue(m_color.green());
	m_blueSpin->setValue(m_color.blue());
	m_alphaSpin->setValue(m_color.alpha());
}

void ColorPickerDialog::updateHsvControls() {
	m_hueSlider->setValue(m_color.hue());
	m_saturationSlider->setValue(m_color.saturation());
	m_valueSlider->setValue(m_color.value());
	m_hueSpin->setValue(m_color.hue());
	m_saturationSpin->setValue(m_color.saturation());
	m_valueSpin->setValue(m_color.value());
}

void ColorPickerDialog::updatePreview() {
	QString style = QStringLiteral("border: 2px solid black; background-color: %1;");
	m_previewLabel->setStyleSheet(style.arg(m_color.name()));
}

void ColorPickerDialog::onColorChanged() {
	// This slot is called when color changes - update preview is handled by RGB/HSV handlers
	updatePreview();
}

void ColorPickerDialog::onPaletteItemClicked(QListWidgetItem* item) {
	if (!item) return;
	QVariant colorVar = item->data(Qt::UserRole);
	if (colorVar.canConvert<QColor>()) {
		setColor(colorVar.value<QColor>());
	}
}

void ColorPickerDialog::addToPalette() {
	m_palette.append(m_color);
	auto* item = new QListWidgetItem(m_paletteList);
	item->setBackground(m_color);
	item->setData(Qt::UserRole, m_color);
	item->setText(QStringLiteral("Color %1").arg(m_palette.size()));
}

void ColorPickerDialog::removeFromPalette() {
	QListWidgetItem* item = m_paletteList->currentItem();
	if (item) {
		int row = m_paletteList->row(item);
		m_palette.removeAt(row);
		delete item;
	}
}

void ColorPickerDialog::savePalette(const QString& mapPath) {
	QSettings settings;
	QString key = QStringLiteral("palette_%1").arg(QFileInfo(mapPath).baseName());
	settings.beginWriteArray(key);
	for (int i = 0; i < m_palette.size(); ++i) {
		settings.setArrayIndex(i);
		settings.setValue("color", m_palette[i]);
	}
	settings.endArray();
}

void ColorPickerDialog::loadPalette(const QString& mapPath) {
	QSettings settings;
	QString key = QStringLiteral("palette_%1").arg(QFileInfo(mapPath).baseName());
	int size = settings.beginReadArray(key);
	m_palette.clear();
	m_paletteList->clear();
	for (int i = 0; i < size; ++i) {
		settings.setArrayIndex(i);
		QColor color = settings.value("color").value<QColor>();
		m_palette.append(color);
		auto* item = new QListWidgetItem(m_paletteList);
		item->setBackground(color);
		item->setData(Qt::UserRole, color);
		item->setText(QStringLiteral("Color %1").arg(i + 1));
	}
	settings.endArray();
}

QColor ColorPickerDialog::getColor(const QColor& initial, QWidget* parent) {
	ColorPickerDialog dlg(parent);
	dlg.setColor(initial);
	if (dlg.exec() == QDialog::Accepted) {
		return dlg.selectedColor();
	}
	return initial;
}
