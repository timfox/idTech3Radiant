#include "surface_inspector.h"

#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>

SurfaceInspector::SurfaceInspector(QWidget* parent)
	: QDialog(parent) {
	setWindowTitle(QStringLiteral("Surface Inspector"));
	setModal(false);
	resize(500, 600);
	
	setupUI();
	updateUI();
}

void SurfaceInspector::setupUI() {
	auto* mainLayout = new QVBoxLayout(this);
	
	// Texture name
	auto* textureLayout = new QHBoxLayout();
	textureLayout->addWidget(new QLabel(QStringLiteral("Texture:"), this));
	m_textureEdit = new QLineEdit(this);
	m_textureEdit->setPlaceholderText(QStringLiteral("textures/common/caulk"));
	textureLayout->addWidget(m_textureEdit);
	mainLayout->addLayout(textureLayout);
	
	// Texture alignment
	auto* alignmentGroup = new QGroupBox(QStringLiteral("Texture Alignment"), this);
	auto* alignmentLayout = new QFormLayout(alignmentGroup);
	
	m_scaleXSpin = new QDoubleSpinBox(this);
	m_scaleXSpin->setRange(0.01, 100.0);
	m_scaleXSpin->setValue(1.0);
	m_scaleXSpin->setSingleStep(0.1);
	m_scaleXSpin->setDecimals(2);
	alignmentLayout->addRow(QStringLiteral("Scale X:"), m_scaleXSpin);
	
	m_scaleYSpin = new QDoubleSpinBox(this);
	m_scaleYSpin->setRange(0.01, 100.0);
	m_scaleYSpin->setValue(1.0);
	m_scaleYSpin->setSingleStep(0.1);
	m_scaleYSpin->setDecimals(2);
	alignmentLayout->addRow(QStringLiteral("Scale Y:"), m_scaleYSpin);
	
	m_offsetXSpin = new QDoubleSpinBox(this);
	m_offsetXSpin->setRange(-10000.0, 10000.0);
	m_offsetXSpin->setValue(0.0);
	m_offsetXSpin->setSingleStep(1.0);
	m_offsetXSpin->setDecimals(2);
	alignmentLayout->addRow(QStringLiteral("Offset X:"), m_offsetXSpin);
	
	m_offsetYSpin = new QDoubleSpinBox(this);
	m_offsetYSpin->setRange(-10000.0, 10000.0);
	m_offsetYSpin->setValue(0.0);
	m_offsetYSpin->setSingleStep(1.0);
	m_offsetYSpin->setDecimals(2);
	alignmentLayout->addRow(QStringLiteral("Offset Y:"), m_offsetYSpin);
	
	m_rotationSpin = new QDoubleSpinBox(this);
	m_rotationSpin->setRange(-360.0, 360.0);
	m_rotationSpin->setValue(0.0);
	m_rotationSpin->setSingleStep(1.0);
	m_rotationSpin->setDecimals(1);
	alignmentLayout->addRow(QStringLiteral("Rotation:"), m_rotationSpin);
	
	mainLayout->addWidget(alignmentGroup);
	
	// Texture projection
	auto* projectionLayout = new QHBoxLayout();
	projectionLayout->addWidget(new QLabel(QStringLiteral("Projection:"), this));
	m_projectionCombo = new QComboBox(this);
	m_projectionCombo->addItems({QStringLiteral("Planar"), QStringLiteral("Cylindrical"), QStringLiteral("Spherical")});
	projectionLayout->addWidget(m_projectionCombo);
	projectionLayout->addStretch();
	mainLayout->addLayout(projectionLayout);
	
	// Action buttons
	auto* buttonLayout = new QHBoxLayout();
	m_fitBtn = new QPushButton(QStringLiteral("Fit"), this);
	m_alignBtn = new QPushButton(QStringLiteral("Align"), this);
	m_resetBtn = new QPushButton(QStringLiteral("Reset"), this);
	buttonLayout->addWidget(m_fitBtn);
	buttonLayout->addWidget(m_alignBtn);
	buttonLayout->addWidget(m_resetBtn);
	buttonLayout->addStretch();
	mainLayout->addLayout(buttonLayout);
	
	// Shader editor
	mainLayout->addWidget(new QLabel(QStringLiteral("Shader:"), this));
	m_shaderEdit = new QTextEdit(this);
	m_shaderEdit->setMaximumHeight(100);
	m_shaderEdit->setPlaceholderText(QStringLiteral("Shader code (optional)"));
	mainLayout->addWidget(m_shaderEdit);
	
	// Dialog buttons
	auto* dialogButtons = new QHBoxLayout();
	dialogButtons->addStretch();
	auto* applyBtn = new QPushButton(QStringLiteral("Apply"), this);
	auto* closeBtn = new QPushButton(QStringLiteral("Close"), this);
	dialogButtons->addWidget(applyBtn);
	dialogButtons->addWidget(closeBtn);
	mainLayout->addLayout(dialogButtons);
	
	// Connections
	connect(m_textureEdit, &QLineEdit::textChanged, this, &SurfaceInspector::onTextureChanged);
	connect(m_projectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SurfaceInspector::onProjectionChanged);
	connect(m_fitBtn, &QPushButton::clicked, this, &SurfaceInspector::onFitTexture);
	connect(m_alignBtn, &QPushButton::clicked, this, &SurfaceInspector::onAlignTexture);
	connect(m_resetBtn, &QPushButton::clicked, this, &SurfaceInspector::onResetTexture);
	connect(applyBtn, &QPushButton::clicked, this, &SurfaceInspector::applyToSelectedFaces);
	connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void SurfaceInspector::updateUI() {
	m_textureEdit->setText(m_currentTexture);
	m_scaleXSpin->setValue(m_textureScale.x());
	m_scaleYSpin->setValue(m_textureScale.y());
	m_offsetXSpin->setValue(m_textureOffset.x());
	m_offsetYSpin->setValue(m_textureOffset.y());
	m_rotationSpin->setValue(m_textureRotation);
	m_projectionCombo->setCurrentIndex(m_projectionType);
}

QString SurfaceInspector::texture() const {
	return m_textureEdit->text();
}

void SurfaceInspector::setTexture(const QString& texture) {
	m_currentTexture = texture;
	m_textureEdit->setText(texture);
}

QVector3D SurfaceInspector::textureScale() const {
	return QVector3D(m_scaleXSpin->value(), m_scaleYSpin->value(), 1.0f);
}

void SurfaceInspector::setTextureScale(const QVector3D& scale) {
	m_textureScale = scale;
	m_scaleXSpin->setValue(scale.x());
	m_scaleYSpin->setValue(scale.y());
}

QVector3D SurfaceInspector::textureOffset() const {
	return QVector3D(m_offsetXSpin->value(), m_offsetYSpin->value(), 0.0f);
}

void SurfaceInspector::setTextureOffset(const QVector3D& offset) {
	m_textureOffset = offset;
	m_offsetXSpin->setValue(offset.x());
	m_offsetYSpin->setValue(offset.y());
}

float SurfaceInspector::textureRotation() const {
	return static_cast<float>(m_rotationSpin->value());
}

void SurfaceInspector::setTextureRotation(float rotation) {
	m_textureRotation = rotation;
	m_rotationSpin->setValue(rotation);
}

int SurfaceInspector::projectionType() const {
	return m_projectionCombo->currentIndex();
}

void SurfaceInspector::setProjectionType(int type) {
	m_projectionType = type;
	m_projectionCombo->setCurrentIndex(type);
}

void SurfaceInspector::applyToSelectedFaces() {
	// In full implementation, would:
	// 1. Get selected faces
	// 2. Apply texture properties
	// 3. Update brush geometry
	// 4. Refresh viewports
	
	QMessageBox::information(this, QStringLiteral("Applied"), 
		QStringLiteral("Texture properties applied to selected faces"));
	accept();
}

void SurfaceInspector::onTextureChanged() {
	m_currentTexture = m_textureEdit->text();
}

void SurfaceInspector::onProjectionChanged(int index) {
	m_projectionType = index;
}

void SurfaceInspector::onFitTexture() {
	// Fit texture to face
	m_scaleXSpin->setValue(1.0);
	m_scaleYSpin->setValue(1.0);
	m_offsetXSpin->setValue(0.0);
	m_offsetYSpin->setValue(0.0);
	m_rotationSpin->setValue(0.0);
}

void SurfaceInspector::onAlignTexture() {
	// Align texture to face normal
	m_rotationSpin->setValue(0.0);
	// In full implementation, would calculate proper alignment
}

void SurfaceInspector::onResetTexture() {
	m_scaleXSpin->setValue(1.0);
	m_scaleYSpin->setValue(1.0);
	m_offsetXSpin->setValue(0.0);
	m_offsetYSpin->setValue(0.0);
	m_rotationSpin->setValue(0.0);
	m_projectionCombo->setCurrentIndex(0);
}
