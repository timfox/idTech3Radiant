#include "texture_extraction_dialog.h"
#include "texture_extractor.h"

#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>

TextureExtractionDialog::TextureExtractionDialog(QWidget* parent)
	: QDialog(parent) {
	setWindowTitle(QStringLiteral("Texture Extraction Tool"));
	setModal(true);
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
	              Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
	setSizeGripEnabled(true);
	resize(600, 500);
	
	m_extractor = new TextureExtractor();
	setupUI();
}

void TextureExtractionDialog::setupUI() {
	auto* mainLayout = new QVBoxLayout(this);
	
	// Source path
	auto* sourceLayout = new QHBoxLayout();
	sourceLayout->addWidget(new QLabel(QStringLiteral("Source:"), this));
	m_sourceEdit = new QLineEdit(this);
	m_sourceEdit->setPlaceholderText(QStringLiteral("BSP, PK3, WAD file or directory"));
	auto* sourceBrowseBtn = new QPushButton(QStringLiteral("Browse..."), this);
	sourceLayout->addWidget(m_sourceEdit);
	sourceLayout->addWidget(sourceBrowseBtn);
	mainLayout->addLayout(sourceLayout);
	
	// Output path
	auto* outputLayout = new QHBoxLayout();
	outputLayout->addWidget(new QLabel(QStringLiteral("Output:"), this));
	m_outputEdit = new QLineEdit(this);
	m_outputEdit->setPlaceholderText(QStringLiteral("Output directory"));
	auto* outputBrowseBtn = new QPushButton(QStringLiteral("Browse..."), this);
	outputLayout->addWidget(m_outputEdit);
	outputLayout->addWidget(outputBrowseBtn);
	mainLayout->addLayout(outputLayout);
	
	// Format and options
	auto* optionsLayout = new QHBoxLayout();
	optionsLayout->addWidget(new QLabel(QStringLiteral("Format:"), this));
	m_formatCombo = new QComboBox(this);
	m_formatCombo->addItems({QStringLiteral("PNG"), QStringLiteral("TGA"), QStringLiteral("JPG")});
	m_formatCombo->setCurrentIndex(0);
	optionsLayout->addWidget(m_formatCombo);
	
	m_recursiveCheck = new QCheckBox(QStringLiteral("Recursive"), this);
	m_recursiveCheck->setChecked(true);
	optionsLayout->addWidget(m_recursiveCheck);
	optionsLayout->addStretch();
	mainLayout->addLayout(optionsLayout);
	
	// Log
	mainLayout->addWidget(new QLabel(QStringLiteral("Log:"), this));
	m_logText = new QTextEdit(this);
	m_logText->setReadOnly(true);
	m_logText->setFont(QFont(QStringLiteral("Courier"), 9));
	mainLayout->addWidget(m_logText);
	
	// Buttons
	auto* buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();
	m_extractBtn = new QPushButton(QStringLiteral("Extract"), this);
	auto* closeBtn = new QPushButton(QStringLiteral("Close"), this);
	buttonLayout->addWidget(m_extractBtn);
	buttonLayout->addWidget(closeBtn);
	mainLayout->addLayout(buttonLayout);
	
	// Connections
	connect(sourceBrowseBtn, &QPushButton::clicked, this, &TextureExtractionDialog::onBrowseSource);
	connect(outputBrowseBtn, &QPushButton::clicked, this, &TextureExtractionDialog::onBrowseOutput);
	connect(m_extractBtn, &QPushButton::clicked, this, &TextureExtractionDialog::onExtract);
	connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
	connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TextureExtractionDialog::onFormatChanged);
	connect(m_sourceEdit, &QLineEdit::textChanged, this, &TextureExtractionDialog::updateExtractButton);
	connect(m_outputEdit, &QLineEdit::textChanged, this, &TextureExtractionDialog::updateExtractButton);
	
	updateExtractButton();
}

void TextureExtractionDialog::onBrowseSource() {
	QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Select Source"),
		QString(), QStringLiteral("All Supported (*.bsp *.pk3 *.wad);;BSP Files (*.bsp);;PK3 Files (*.pk3);;WAD Files (*.wad);;All Files (*.*)"));
	
	if (!path.isEmpty()) {
		m_sourceEdit->setText(path);
	}
}

void TextureExtractionDialog::onBrowseOutput() {
	QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Output Directory"));
	if (!path.isEmpty()) {
		m_outputEdit->setText(path);
	}
}

void TextureExtractionDialog::onExtract() {
	QString source = sourcePath();
	QString output = outputPath();
	
	if (source.isEmpty() || output.isEmpty()) {
		QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Please specify both source and output paths"));
		return;
	}
	
	m_extractor->resetStats();
	m_extractor->setOutputFormat(outputFormat().toLower());
	m_extractor->setRecursive(recursive());
	
	m_logText->clear();
	m_logText->append(QStringLiteral("Starting extraction..."));
	m_logText->append(QStringLiteral("Source: %1").arg(source));
	m_logText->append(QStringLiteral("Output: %1").arg(output));
	m_logText->append(QStringLiteral("Format: %1").arg(outputFormat()));
	m_logText->append(QStringLiteral(""));
	
	bool success = false;
	QFileInfo sourceInfo(source);
	
	if (sourceInfo.isFile()) {
		QString ext = sourceInfo.suffix().toLower();
		if (ext == "bsp") {
			success = m_extractor->extractFromBSP(source, output);
		} else if (ext == "pk3") {
			success = m_extractor->extractFromPK3(source, output);
		} else if (ext == "wad") {
			success = m_extractor->extractFromWAD(source, output);
		}
	} else if (sourceInfo.isDir()) {
		success = m_extractor->extractFromDirectory(source, output, recursive());
	}
	
	if (success) {
		m_logText->append(QStringLiteral("Extraction complete!"));
		m_logText->append(QStringLiteral("Extracted: %1 textures").arg(m_extractor->extractedCount()));
		if (m_extractor->failedCount() > 0) {
			m_logText->append(QStringLiteral("Failed: %1 textures").arg(m_extractor->failedCount()));
		}
		
		QMessageBox::information(this, QStringLiteral("Success"), 
			QStringLiteral("Extracted %1 textures successfully").arg(m_extractor->extractedCount()));
	} else {
		m_logText->append(QStringLiteral("Extraction failed!"));
		QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("Failed to extract textures"));
	}
}

void TextureExtractionDialog::onFormatChanged(int index) {
	Q_UNUSED(index);
	// Format changed
}

QString TextureExtractionDialog::sourcePath() const {
	return m_sourceEdit->text();
}

QString TextureExtractionDialog::outputPath() const {
	return m_outputEdit->text();
}

QString TextureExtractionDialog::outputFormat() const {
	return m_formatCombo->currentText();
}

bool TextureExtractionDialog::recursive() const {
	return m_recursiveCheck->isChecked();
}

void TextureExtractionDialog::updateExtractButton() {
	bool enabled = !m_sourceEdit->text().isEmpty() && !m_outputEdit->text().isEmpty();
	m_extractBtn->setEnabled(enabled);
}
