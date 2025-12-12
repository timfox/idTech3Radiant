#include "mess_template.h"

#include <QTreeWidgetItem>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

MESSTemplate::MESSTemplate() {
}

MESSTemplate::MESSTemplate(const QString& name, const QString& description)
	: m_name(name), m_description(description) {
}

void MESSTemplate::addEntity(const QString& classname, const QMap<QString, QString>& properties) {
	TemplateEntity entity;
	entity.classname = classname;
	entity.properties = properties;
	m_entities.append(entity);
}

void MESSTemplate::addBrush(const QVector<QVector3D>& vertices, const QString& texture) {
	TemplateBrush brush;
	brush.vertices = vertices;
	brush.texture = texture;
	m_brushes.append(brush);
}

void MESSTemplate::addVariable(const QString& name, const QString& defaultValue) {
	m_variables[name] = defaultValue;
}

void MESSTemplate::setVariable(const QString& name, const QString& value) {
	m_variables[name] = value;
}

QString MESSTemplate::getVariable(const QString& name) const {
	return m_variables.value(name, QString());
}

QVector<QTreeWidgetItem*> MESSTemplate::instantiate(const QVector3D& position, const QMap<QString, QString>& overrides) const {
	QVector<QTreeWidgetItem*> items;
	
	// Merge overrides with template variables
	QMap<QString, QString> vars = m_variables;
	for (auto it = overrides.begin(); it != overrides.end(); ++it) {
		vars[it.key()] = it.value();
	}
	
	// Instantiate entities
	for (const TemplateEntity& entity : m_entities) {
		QTreeWidgetItem* item = new QTreeWidgetItem();
		item->setText(0, substituteVariables(entity.classname, vars));
		item->setText(1, QStringLiteral("entity"));
		
		// Apply position offset
		QVector3D entityPos = position;
		if (entity.properties.contains(QStringLiteral("origin"))) {
			QString origin = entity.properties[QStringLiteral("origin")];
			// Parse and offset origin
			QStringList parts = origin.split(' ');
			if (parts.size() >= 3) {
				entityPos += QVector3D(
					parts[0].toFloat(),
					parts[1].toFloat(),
					parts[2].toFloat()
				);
			}
		}
		
		item->setData(0, Qt::UserRole, QVariant::fromValue(entityPos));
		
		// Store properties
		QMap<QString, QString> props;
		for (auto it = entity.properties.begin(); it != entity.properties.end(); ++it) {
			props[it.key()] = substituteVariables(it.value(), vars);
		}
		item->setData(1, Qt::UserRole, QVariant::fromValue(props));
		
		items.append(item);
	}
	
	// Instantiate brushes
	for (const TemplateBrush& brush : m_brushes) {
		QTreeWidgetItem* item = new QTreeWidgetItem();
		item->setText(0, QStringLiteral("brush_%1").arg(items.size()));
		item->setText(1, QStringLiteral("brush"));
		
		// Offset brush vertices
		QVector<QVector3D> offsetVertices;
		for (const QVector3D& v : brush.vertices) {
			offsetVertices.append(v + position);
		}
		item->setData(0, Qt::UserRole, QVariant::fromValue(offsetVertices));
		item->setData(1, Qt::UserRole, brush.texture);
		
		items.append(item);
	}
	
	return items;
}

QString MESSTemplate::substituteVariables(const QString& text, const QMap<QString, QString>& overrides) const {
	QString result = text;
	
	// Substitute ${variable} or $variable
	QRegularExpression varRegex("\\$\\{([^}]+)\\}|\\$([a-zA-Z_][a-zA-Z0-9_]*)");
	QRegularExpressionMatchIterator it = varRegex.globalMatch(result);
	QStringList replacements;
	int offset = 0;
	
	while (it.hasNext()) {
		QRegularExpressionMatch match = it.next();
		QString varName = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
		QString varValue = overrides.value(varName, m_variables.value(varName, QString()));
		result.replace(match.capturedStart(0) + offset, match.capturedLength(0), varValue);
		offset += varValue.length() - match.capturedLength(0);
	}
	
	return result;
}

bool MESSTemplate::saveToFile(const QString& filePath) const {
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}
	
	QJsonObject root;
	root["name"] = m_name;
	root["description"] = m_description;
	root["category"] = m_category;
	root["script"] = m_script;
	
	// Variables
	QJsonObject vars;
	for (auto it = m_variables.begin(); it != m_variables.end(); ++it) {
		vars[it.key()] = it.value();
	}
	root["variables"] = vars;
	
	// Entities
	QJsonArray entities;
	for (const TemplateEntity& entity : m_entities) {
		QJsonObject e;
		e["classname"] = entity.classname;
		QJsonObject props;
		for (auto it = entity.properties.begin(); it != entity.properties.end(); ++it) {
			props[it.key()] = it.value();
		}
		e["properties"] = props;
		entities.append(e);
	}
	root["entities"] = entities;
	
	// Brushes
	QJsonArray brushes;
	for (const TemplateBrush& brush : m_brushes) {
		QJsonObject b;
		QJsonArray verts;
		for (const QVector3D& v : brush.vertices) {
			QJsonArray vert;
			vert.append(v.x());
			vert.append(v.y());
			vert.append(v.z());
			verts.append(vert);
		}
		b["vertices"] = verts;
		b["texture"] = brush.texture;
		brushes.append(b);
	}
	root["brushes"] = brushes;
	
	QJsonDocument doc(root);
	file.write(doc.toJson());
	file.close();
	
	return true;
}

bool MESSTemplate::loadFromFile(const QString& filePath) {
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
	}
	
	QByteArray data = file.readAll();
	file.close();
	
	QJsonDocument doc = QJsonDocument::fromJson(data);
	if (doc.isNull()) {
		return false;
	}
	
	QJsonObject root = doc.object();
	m_name = root["name"].toString();
	m_description = root["description"].toString();
	m_category = root["category"].toString();
	m_script = root["script"].toString();
	
	// Variables
	QJsonObject vars = root["variables"].toObject();
	for (auto it = vars.begin(); it != vars.end(); ++it) {
		m_variables[it.key()] = it.value().toString();
	}
	
	// Entities
	m_entities.clear();
	QJsonArray entities = root["entities"].toArray();
	for (const QJsonValue& val : entities) {
		QJsonObject e = val.toObject();
		TemplateEntity entity;
		entity.classname = e["classname"].toString();
		QJsonObject props = e["properties"].toObject();
		for (auto it = props.begin(); it != props.end(); ++it) {
			entity.properties[it.key()] = it.value().toString();
		}
		m_entities.append(entity);
	}
	
	// Brushes
	m_brushes.clear();
	QJsonArray brushes = root["brushes"].toArray();
	for (const QJsonValue& val : brushes) {
		QJsonObject b = val.toObject();
		TemplateBrush brush;
		QJsonArray verts = b["vertices"].toArray();
		for (const QJsonValue& v : verts) {
			QJsonArray vert = v.toArray();
			if (vert.size() >= 3) {
				brush.vertices.append(QVector3D(
					vert[0].toDouble(),
					vert[1].toDouble(),
					vert[2].toDouble()
				));
			}
		}
		brush.texture = b["texture"].toString();
		m_brushes.append(brush);
	}
	
	return true;
}
