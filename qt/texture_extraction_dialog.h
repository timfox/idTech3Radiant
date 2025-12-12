#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QComboBox;
class QPushButton;
class QTextEdit;
class QCheckBox;
class TextureExtractor;

// Texture extraction dialog
class TextureExtractionDialog : public QDialog {
	Q_OBJECT
public:
	explicit TextureExtractionDialog(QWidget* parent = nullptr);
	
	QString sourcePath() const;
	QString outputPath() const;
	QString outputFormat() const;
	bool recursive() const;

private Q_SLOTS:
	void onBrowseSource();
	void onBrowseOutput();
	void onExtract();
	void onFormatChanged(int index);

private:
	void setupUI();
	void updateExtractButton();
	
	QLineEdit* m_sourceEdit = nullptr;
	QLineEdit* m_outputEdit = nullptr;
	QComboBox* m_formatCombo = nullptr;
	QCheckBox* m_recursiveCheck = nullptr;
	QPushButton* m_extractBtn = nullptr;
	QTextEdit* m_logText = nullptr;
	
	TextureExtractor* m_extractor = nullptr;
};
