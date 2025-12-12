#include "mess_manager.h"
#include "mess_template.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSet>
#include <QStandardPaths>

MESSManager::MESSManager() {
	loadDefaultTemplates();
}

MESSManager::~MESSManager() {
	for (auto it = m_templates.begin(); it != m_templates.end(); ++it) {
		delete it.value();
	}
}

void MESSManager::loadTemplatesFromDirectory(const QString& directory) {
	QDir dir(directory);
	if (!dir.exists()) {
		return;
	}
	
	QDirIterator it(directory, QStringList() << "*.mess" << "*.json", QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		QString filePath = it.next();
		MESSTemplate* template_ = new MESSTemplate();
		if (template_->loadFromFile(filePath)) {
			m_templates[template_->name()] = template_;
		} else {
			delete template_;
		}
	}
}

void MESSManager::addTemplate(MESSTemplate* template_) {
	if (template_) {
		m_templates[template_->name()] = template_;
	}
}

MESSTemplate* MESSManager::getTemplate(const QString& name) const {
	return m_templates.value(name, nullptr);
}

QVector<MESSTemplate*> MESSManager::getTemplatesByCategory(const QString& category) const {
	QVector<MESSTemplate*> result;
	for (auto it = m_templates.begin(); it != m_templates.end(); ++it) {
		if (it.value()->category() == category) {
			result.append(it.value());
		}
	}
	return result;
}

QVector<QString> MESSManager::getCategories() const {
	QSet<QString> categories;
	for (auto it = m_templates.begin(); it != m_templates.end(); ++it) {
		categories.insert(it.value()->category());
	}
	return categories.values().toVector();
}

QVector<QTreeWidgetItem*> MESSManager::instantiateTemplate(const QString& templateName, const QVector3D& position,
	const QMap<QString, QString>& variables, QTreeWidget* targetWidget) {
	
	MESSTemplate* template_ = getTemplate(templateName);
	if (!template_) {
		return QVector<QTreeWidgetItem*>();
	}
	
	// Merge global variables
	QMap<QString, QString> allVars = m_globalVariables;
	for (auto it = variables.begin(); it != variables.end(); ++it) {
		allVars[it.key()] = it.value();
	}
	
	QVector<QTreeWidgetItem*> items = template_->instantiate(position, allVars);
	
	// Add to target widget if provided
	if (targetWidget) {
		for (QTreeWidgetItem* item : items) {
			targetWidget->addTopLevelItem(item);
		}
	}
	
	return items;
}

bool MESSManager::insertTemplateMap(const QString& mapPath, const QVector3D& position, QTreeWidget* targetWidget) {
	Q_UNUSED(mapPath);
	Q_UNUSED(position);
	Q_UNUSED(targetWidget);
	
	// In full implementation, would:
	// 1. Load map file
	// 2. Parse entities and brushes
	// 3. Offset by position
	// 4. Add to target widget
	
	return false; // Simplified
}

void MESSManager::setGlobalVariable(const QString& name, const QString& value) {
	m_globalVariables[name] = value;
}

QString MESSManager::getGlobalVariable(const QString& name) const {
	return m_globalVariables.value(name, QString());
}

void MESSManager::loadDefaultTemplates() {
	// Create some default Q3 templates
	
	// Trigger multiple template
	MESSTemplate* triggerMulti = new MESSTemplate("trigger_multiple", "Trigger that fires multiple times");
	triggerMulti->setCategory("Triggers");
	triggerMulti->addVariable("target", "");
	triggerMulti->addVariable("wait", "0");
	triggerMulti->addEntity("trigger_multiple", {
		{"target", "${target}"},
		{"wait", "${wait}"},
		{"spawnflags", "1"}
	});
	m_templates["trigger_multiple"] = triggerMulti;
	
	// Door template
	MESSTemplate* door = new MESSTemplate("func_door", "Basic door entity");
	door->setCategory("Doors");
	door->addVariable("targetname", "door1");
	door->addVariable("speed", "100");
	door->addEntity("func_door", {
		{"targetname", "${targetname}"},
		{"speed", "${speed}"},
		{"spawnflags", "1"}
	});
	m_templates["func_door"] = door;
	
	// Teleporter template
	MESSTemplate* teleporter = new MESSTemplate("trigger_teleport", "Teleporter trigger");
	teleporter->setCategory("Teleporters");
	teleporter->addVariable("target", "");
	teleporter->addVariable("targetname", "tele1");
	teleporter->addEntity("trigger_teleport", {
		{"targetname", "${targetname}"},
		{"target", "${target}"}
	});
	teleporter->addEntity("info_teleport_destination", {
		{"targetname", "${target}"},
		{"origin", "0 0 0"}
	});
	m_templates["trigger_teleport"] = teleporter;
}
