#pragma once

#include <QString>
#include <QMap>
#include <QVector>
#include <QVector3D>

class MESSTemplate;
class QTreeWidgetItem;
class QTreeWidget;

// Forward declare for access
class MESSManager;

// MESS Manager - manages template library and instantiation
class MESSManager {
public:
	MESSManager();
	~MESSManager();
	
	// Template management
	void loadTemplatesFromDirectory(const QString& directory);
	void addTemplate(MESSTemplate* template_);
	MESSTemplate* getTemplate(const QString& name) const;
	QVector<MESSTemplate*> getTemplatesByCategory(const QString& category) const;
	QVector<QString> getCategories() const;
	
	// Instantiation
	QVector<QTreeWidgetItem*> instantiateTemplate(const QString& templateName, const QVector3D& position, 
		const QMap<QString, QString>& variables = QMap<QString, QString>(), QTreeWidget* targetWidget = nullptr);
	
	// Template map insertion
	bool insertTemplateMap(const QString& mapPath, const QVector3D& position, QTreeWidget* targetWidget);
	
	// Variable management
	void setGlobalVariable(const QString& name, const QString& value);
	QString getGlobalVariable(const QString& name) const;
	QMap<QString, QString> globalVariables() const { return m_globalVariables; }
	
	// Template directory
	void setTemplateDirectory(const QString& directory) { m_templateDirectory = directory; }
	QString templateDirectory() const { return m_templateDirectory; }
	
	// Get all templates (for iteration)
	QMap<QString, MESSTemplate*> templates() const { return m_templates; }

private:
	QMap<QString, MESSTemplate*> m_templates;
	QMap<QString, QString> m_globalVariables;
	QString m_templateDirectory;
	
	void loadDefaultTemplates();
};
