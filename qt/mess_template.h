#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <QVector3D>

class QTreeWidgetItem;

// MESS Template - represents a reusable template entity/brush setup
class MESSTemplate {
public:
	MESSTemplate();
	MESSTemplate(const QString& name, const QString& description);
	
	// Template properties
	QString name() const { return m_name; }
	QString description() const { return m_description; }
	QString category() const { return m_category; }
	void setCategory(const QString& category) { m_category = category; }
	
	// Template content
	void addEntity(const QString& classname, const QMap<QString, QString>& properties);
	void addBrush(const QVector<QVector3D>& vertices, const QString& texture);
	void addVariable(const QString& name, const QString& defaultValue);
	
	// Variables
	QMap<QString, QString> variables() const { return m_variables; }
	void setVariable(const QString& name, const QString& value);
	QString getVariable(const QString& name) const;
	
	// Instantiation
	QVector<QTreeWidgetItem*> instantiate(const QVector3D& position, const QMap<QString, QString>& overrides = QMap<QString, QString>()) const;
	
	// Serialization
	bool saveToFile(const QString& filePath) const;
	bool loadFromFile(const QString& filePath);
	
	// Scripting
	void setScript(const QString& script) { m_script = script; }
	QString script() const { return m_script; }

private:
	QString m_name;
	QString m_description;
	QString m_category;
	QString m_script;
	
	// Template content
	struct TemplateEntity {
		QString classname;
		QMap<QString, QString> properties;
	};
	struct TemplateBrush {
		QVector<QVector3D> vertices;
		QString texture;
	};
	
	QVector<TemplateEntity> m_entities;
	QVector<TemplateBrush> m_brushes;
	QMap<QString, QString> m_variables;
	
	// Variable substitution
	QString substituteVariables(const QString& text, const QMap<QString, QString>& overrides) const;
};
