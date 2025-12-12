#include "selection_sets.h"

#include <QTreeWidgetItem>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

SelectionSets::SelectionSets() {
}

void SelectionSets::createSet(const QString& name) {
	if (!m_sets.contains(name)) {
		m_sets[name] = QVector<QString>();
	}
}

void SelectionSets::deleteSet(const QString& name) {
	m_sets.remove(name);
}

void SelectionSets::renameSet(const QString& oldName, const QString& newName) {
	if (m_sets.contains(oldName) && !m_sets.contains(newName)) {
		m_sets[newName] = m_sets[oldName];
		m_sets.remove(oldName);
	}
}

QStringList SelectionSets::setNames() const {
	return m_sets.keys();
}

void SelectionSets::storeSelection(const QString& setName, const QVector<QTreeWidgetItem*>& items) {
	createSet(setName);
	m_sets[setName].clear();
	
	for (QTreeWidgetItem* item : items) {
		QString id = itemIdentifier(item);
		m_sets[setName].append(id);
		m_itemMap[id] = item;
	}
}

QVector<QTreeWidgetItem*> SelectionSets::getSelection(const QString& setName) const {
	QVector<QTreeWidgetItem*> items;
	
	if (!m_sets.contains(setName)) {
		return items;
	}
	
	for (const QString& id : m_sets[setName]) {
		if (m_itemMap.contains(id)) {
			items.append(m_itemMap[id]);
		}
	}
	
	return items;
}

void SelectionSets::selectSet(const QString& setName, QTreeWidgetItem* rootItem) {
	if (!m_sets.contains(setName)) {
		return;
	}
	
	// Clear current selection
	if (rootItem) {
		// Would clear selection in tree widget
	}
	
	// Select items in set
	for (const QString& id : m_sets[setName]) {
		QTreeWidgetItem* item = findItemByIdentifier(id, rootItem);
		if (item) {
			item->setSelected(true);
		}
	}
}

void SelectionSets::addToSet(const QString& setName, const QVector<QTreeWidgetItem*>& items) {
	createSet(setName);
	
	for (QTreeWidgetItem* item : items) {
		QString id = itemIdentifier(item);
		if (!m_sets[setName].contains(id)) {
			m_sets[setName].append(id);
			m_itemMap[id] = item;
		}
	}
}

void SelectionSets::removeFromSet(const QString& setName, const QVector<QTreeWidgetItem*>& items) {
	if (!m_sets.contains(setName)) {
		return;
	}
	
	for (QTreeWidgetItem* item : items) {
		QString id = itemIdentifier(item);
		m_sets[setName].removeAll(id);
		m_itemMap.remove(id);
	}
}

QString SelectionSets::itemIdentifier(QTreeWidgetItem* item) const {
	if (!item) return QString();
	
	// Create unique identifier from item path
	QStringList path;
	QTreeWidgetItem* current = item;
	while (current) {
		path.prepend(current->text(0));
		current = current->parent();
	}
	
	return path.join("/");
}

QTreeWidgetItem* SelectionSets::findItemByIdentifier(const QString& identifier, QTreeWidgetItem* root) const {
	if (!root) return nullptr;
	
	QStringList path = identifier.split("/");
	QTreeWidgetItem* current = root;
	
	for (const QString& name : path) {
		bool found = false;
		for (int i = 0; i < current->childCount(); ++i) {
			if (current->child(i)->text(0) == name) {
				current = current->child(i);
				found = true;
				break;
			}
		}
		if (!found) {
			return nullptr;
		}
	}
	
	return current;
}

bool SelectionSets::saveToFile(const QString& filePath) const {
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}
	
	QJsonObject root;
	QJsonObject sets;
	
	for (auto it = m_sets.begin(); it != m_sets.end(); ++it) {
		QJsonArray items;
		for (const QString& id : it.value()) {
			items.append(id);
		}
		sets[it.key()] = items;
	}
	
	root["sets"] = sets;
	
	QJsonDocument doc(root);
	file.write(doc.toJson());
	file.close();
	
	return true;
}

bool SelectionSets::loadFromFile(const QString& filePath) {
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
	QJsonObject sets = root["sets"].toObject();
	
	m_sets.clear();
	for (auto it = sets.begin(); it != sets.end(); ++it) {
		QJsonArray items = it.value().toArray();
		QVector<QString> itemList;
		for (const QJsonValue& val : items) {
			itemList.append(val.toString());
		}
		m_sets[it.key()] = itemList;
	}
	
	return true;
}
