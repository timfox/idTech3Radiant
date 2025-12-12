#pragma once

#include <QDialog>
#include <QString>

QT_BEGIN_NAMESPACE
class QTextEdit;
class QLineEdit;
class QComboBox;
QT_END_NAMESPACE

// Simple shader editor for Q3 shaders
class ShaderEditor : public QDialog {
	Q_OBJECT
public:
	explicit ShaderEditor(QWidget* parent = nullptr);
	
	// Get/set shader name
	QString shaderName() const;
	void setShaderName(const QString& name);
	
	// Get/set shader content
	QString shaderContent() const;
	void setShaderContent(const QString& content);
	
	// Load shader from file
	bool loadShader(const QString& filePath);
	
	// Save shader to file
	bool saveShader(const QString& filePath);

private Q_SLOTS:
	void onSave();
	void onLoad();
	void onApply();
	void onSyntaxCheck();

private:
	void setupUI();
	void populateShaderTemplates();
	
	QLineEdit* m_nameEdit = nullptr;
	QTextEdit* m_contentEdit = nullptr;
	QComboBox* m_templateCombo = nullptr;
	QString m_shaderPath;
};
