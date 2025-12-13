#include "shader_editor.h"

#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>

ShaderEditor::ShaderEditor(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(QStringLiteral("Shader Editor"));
	setModal(true);
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
	              Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
	setSizeGripEnabled(true);
	resize(800, 600);
	
	setupUI();
	populateShaderTemplates();
}

void ShaderEditor::setupUI() {
	auto* mainLayout = new QVBoxLayout(this);
	
	// Shader name
	auto* nameLayout = new QHBoxLayout();
	nameLayout->addWidget(new QLabel(QStringLiteral("Shader Name:"), this));
	m_nameEdit = new QLineEdit(this);
	m_nameEdit->setPlaceholderText(QStringLiteral("textures/common/caulk"));
	nameLayout->addWidget(m_nameEdit);
	mainLayout->addLayout(nameLayout);
	
	// Template selector
	auto* templateLayout = new QHBoxLayout();
	templateLayout->addWidget(new QLabel(QStringLiteral("Template:"), this));
	m_templateCombo = new QComboBox(this);
	m_templateCombo->addItem(QStringLiteral("Custom"));
	m_templateCombo->addItem(QStringLiteral("Basic"));
	m_templateCombo->addItem(QStringLiteral("Transparent"));
	m_templateCombo->addItem(QStringLiteral("Sky"));
	m_templateCombo->addItem(QStringLiteral("Water"));
	templateLayout->addWidget(m_templateCombo);
	mainLayout->addLayout(templateLayout);
	
	connect(m_templateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		if (index == 0) return; // Custom
		
		QString templateContent;
		switch (index) {
			case 1: // Basic
				templateContent = QStringLiteral(
					"{\n"
					"  map $lightmap\n"
					"  rgbGen identity\n"
					"}\n"
					"{\n"
					"  map $basetexture\n"
					"  blendFunc filter\n"
					"  rgbGen identity\n"
					"}\n"
				);
				break;
			case 2: // Transparent
				templateContent = QStringLiteral(
					"{\n"
					"  map $basetexture\n"
					"  blendFunc blend\n"
					"  rgbGen identity\n"
					"  alphaGen vertex\n"
					"}\n"
				);
				break;
			case 3: // Sky
				templateContent = QStringLiteral(
					"{\n"
					"  map $basetexture\n"
					"  tcMod scroll 0.01 0.01\n"
					"  tcMod scale 1 1\n"
					"}\n"
				);
				break;
			case 4: // Water
				templateContent = QStringLiteral(
					"{\n"
					"  map $basetexture\n"
					"  tcMod scroll 0.01 0.01\n"
					"  blendFunc blend\n"
					"  rgbGen identity\n"
					"  alphaGen vertex\n"
					"}\n"
				);
				break;
		}
		m_contentEdit->setPlainText(templateContent);
	});
	
	// Shader content
	mainLayout->addWidget(new QLabel(QStringLiteral("Shader Content:"), this));
	m_contentEdit = new QTextEdit(this);
	m_contentEdit->setFont(QFont(QStringLiteral("Courier"), 10));
	mainLayout->addWidget(m_contentEdit);
	
	// Buttons
	auto* buttonLayout = new QHBoxLayout();
	
	auto* loadBtn = new QPushButton(QStringLiteral("Load..."), this);
	auto* saveBtn = new QPushButton(QStringLiteral("Save..."), this);
	auto* syntaxBtn = new QPushButton(QStringLiteral("Check Syntax"), this);
	auto* applyBtn = new QPushButton(QStringLiteral("Apply"), this);
	auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
	
	buttonLayout->addWidget(loadBtn);
	buttonLayout->addWidget(saveBtn);
	buttonLayout->addWidget(syntaxBtn);
	buttonLayout->addStretch();
	buttonLayout->addWidget(applyBtn);
	buttonLayout->addWidget(cancelBtn);
	
	mainLayout->addLayout(buttonLayout);
	
	connect(loadBtn, &QPushButton::clicked, this, &ShaderEditor::onLoad);
	connect(saveBtn, &QPushButton::clicked, this, &ShaderEditor::onSave);
	connect(syntaxBtn, &QPushButton::clicked, this, &ShaderEditor::onSyntaxCheck);
	connect(applyBtn, &QPushButton::clicked, this, &ShaderEditor::onApply);
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ShaderEditor::populateShaderTemplates() {
	// Templates are already added in setupUI
}

QString ShaderEditor::shaderName() const {
	return m_nameEdit->text();
}

void ShaderEditor::setShaderName(const QString& name) {
	m_nameEdit->setText(name);
}

QString ShaderEditor::shaderContent() const {
	return m_contentEdit->toPlainText();
}

void ShaderEditor::setShaderContent(const QString& content) {
	m_contentEdit->setPlainText(content);
}

bool ShaderEditor::loadShader(const QString& filePath) {
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
	}
	
	QTextStream in(&file);
	QString content = in.readAll();
	file.close();
	
	m_contentEdit->setPlainText(content);
	m_shaderPath = filePath;
	
	// Extract shader name from path
	QFileInfo info(filePath);
	QString name = info.baseName();
	if (filePath.contains(QStringLiteral("scripts/"))) {
		// Try to extract full shader path
		int scriptsIdx = filePath.indexOf(QStringLiteral("scripts/"));
		QString relPath = filePath.mid(scriptsIdx + 8); // +8 for "scripts/"
		name = relPath.left(relPath.length() - 4); // Remove .shader extension
	}
	m_nameEdit->setText(name);
	
	return true;
}

bool ShaderEditor::saveShader(const QString& filePath) {
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}
	
	QTextStream out(&file);
	out << m_contentEdit->toPlainText();
	file.close();
	
	m_shaderPath = filePath;
	return true;
}

void ShaderEditor::onSave() {
	QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Shader"),
		m_shaderPath, QStringLiteral("Shader Files (*.shader);;All Files (*.*)"));
	if (path.isEmpty()) return;
	
	if (saveShader(path)) {
		QMessageBox::information(this, QStringLiteral("Success"), QStringLiteral("Shader saved successfully"));
	} else {
		QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Failed to save shader"));
	}
}

void ShaderEditor::onLoad() {
	QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Load Shader"),
		QString(), QStringLiteral("Shader Files (*.shader);;All Files (*.*)"));
	if (path.isEmpty()) return;
	
	if (loadShader(path)) {
		QMessageBox::information(this, QStringLiteral("Success"), QStringLiteral("Shader loaded successfully"));
	} else {
		QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Failed to load shader"));
	}
}

void ShaderEditor::onApply() {
	if (shaderName().isEmpty()) {
		QMessageBox::warning(this, QStringLiteral("Warning"), QStringLiteral("Please enter a shader name"));
		return;
	}
	
	accept();
}

void ShaderEditor::onSyntaxCheck() {
	QString content = m_contentEdit->toPlainText();
	
	// Basic syntax checking
	QStringList errors;
	
	// Check for balanced braces
	int openBraces = content.count('{');
	int closeBraces = content.count('}');
	if (openBraces != closeBraces) {
		errors.append(QStringLiteral("Unbalanced braces: %1 open, %2 close").arg(openBraces).arg(closeBraces));
	}
	
	// Check for basic structure
	if (!content.contains(QStringLiteral("{")) && !content.contains(QStringLiteral("}"))) {
		errors.append(QStringLiteral("Shader appears to be empty or invalid"));
	}
	
	if (errors.isEmpty()) {
		QMessageBox::information(this, QStringLiteral("Syntax Check"), QStringLiteral("No syntax errors found"));
	} else {
		QMessageBox::warning(this, QStringLiteral("Syntax Errors"), errors.join(QStringLiteral("\n")));
	}
}
