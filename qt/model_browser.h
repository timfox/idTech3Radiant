#pragma once

#include <QDialog>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QListWidget;
class QListWidgetItem;
class QTabWidget;
class QLineEdit;
class QLabel;
class QTextEdit;
QT_END_NAMESPACE

class ModelBrowser : public QDialog {
	Q_OBJECT
public:
	explicit ModelBrowser(QWidget* parent = nullptr);
	QString selectedModel() const { return m_selectedModel; }
	
	static QString getModel(const QString& initial = QString(), QWidget* parent = nullptr);

public Q_SLOTS:
	void onModelSelected(QListWidgetItem* item);
	void onFilterChanged(const QString& text);
	void onTabChanged(int index);

private:
	void populateModels();
	void updateModelList();
	void updateModelInfo(const QString& modelPath);
	QStringList filterModels(const QStringList& models, const QString& filter) const;
	
	QTabWidget* m_tabs = nullptr;
	QListWidget* m_modelList = nullptr;
	QLineEdit* m_filterEdit = nullptr;
	QLabel* m_modelInfoLabel = nullptr;
	QTextEdit* m_attachmentsText = nullptr;
	
	QStringList m_allModels;
	QString m_selectedModel;
};
