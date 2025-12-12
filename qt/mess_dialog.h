#pragma once

#include <QDialog>
#include <QMap>
#include <QVector3D>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QComboBox;
class QTableWidget;
class MESSManager;

// MESS Template Browser and Instantiation Dialog
class MESSDialog : public QDialog {
	Q_OBJECT
public:
	explicit MESSDialog(MESSManager* manager, QWidget* parent = nullptr);
	
	QString selectedTemplate() const;
	QMap<QString, QString> templateVariables() const;
	QVector3D instantiationPosition() const;

private Q_SLOTS:
	void onTemplateSelected(QTreeWidgetItem* item, int column);
	void onInstantiate();
	void onVariableChanged();
	void onCategoryChanged(int index);

private:
	void setupUI();
	void populateTemplates();
	void updateVariableEditor();
	
	MESSManager* m_manager;
	QTreeWidget* m_templateList;
	QComboBox* m_categoryCombo;
	QTableWidget* m_variableTable;
	QLineEdit* m_posXEdit;
	QLineEdit* m_posYEdit;
	QLineEdit* m_posZEdit;
	QString m_selectedTemplate;
};
