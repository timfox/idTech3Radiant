#pragma once

#include <QDialog>
#include <QVector3D>
#include <QString>

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QTextEdit;

// Surface Inspector - detailed face editing (GTK Radiant feature)
class SurfaceInspector : public QDialog {
	Q_OBJECT
public:
	explicit SurfaceInspector(QWidget* parent = nullptr);
	
	// Face properties
	QString texture() const;
	void setTexture(const QString& texture);
	
	QVector3D textureScale() const;
	void setTextureScale(const QVector3D& scale);
	
	QVector3D textureOffset() const;
	void setTextureOffset(const QVector3D& offset);
	
	float textureRotation() const;
	void setTextureRotation(float rotation);
	
	// Texture projection
	int projectionType() const; // 0=Planar, 1=Cylindrical, 2=Spherical
	void setProjectionType(int type);
	
	// Apply changes
	void applyToSelectedFaces();

private Q_SLOTS:
	void onTextureChanged();
	void onProjectionChanged(int index);
	void onFitTexture();
	void onAlignTexture();
	void onResetTexture();

private:
	void setupUI();
	void updateUI();
	
	QLineEdit* m_textureEdit = nullptr;
	QDoubleSpinBox* m_scaleXSpin = nullptr;
	QDoubleSpinBox* m_scaleYSpin = nullptr;
	QDoubleSpinBox* m_offsetXSpin = nullptr;
	QDoubleSpinBox* m_offsetYSpin = nullptr;
	QDoubleSpinBox* m_rotationSpin = nullptr;
	QComboBox* m_projectionCombo = nullptr;
	QPushButton* m_fitBtn = nullptr;
	QPushButton* m_alignBtn = nullptr;
	QPushButton* m_resetBtn = nullptr;
	QTextEdit* m_shaderEdit = nullptr;
	
	QString m_currentTexture;
	QVector3D m_textureScale{1.0f, 1.0f, 1.0f};
	QVector3D m_textureOffset{0.0f, 0.0f, 0.0f};
	float m_textureRotation{0.0f};
	int m_projectionType{0};
};
