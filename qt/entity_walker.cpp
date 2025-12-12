#include "entity_walker.h"

#include <QTreeWidgetItem>

EntityWalker::EntityWalker() {
}

QVector<QTreeWidgetItem*> EntityWalker::findConnected(const QTreeWidgetItem* entityItem) {
	if (!entityItem) return QVector<QTreeWidgetItem*>();
	
	QVector<QTreeWidgetItem*> connected;
	
	// Find by target
	QString target = getEntityKey(entityItem, QStringLiteral("target"));
	if (!target.isEmpty()) {
		QVector<QTreeWidgetItem*> byTarget = findEntitiesByValue(QStringLiteral("targetname"), target);
		connected.append(byTarget);
	}
	
	// Find by targetname (entities that target this one)
	QString targetname = getEntityKey(entityItem, QStringLiteral("targetname"));
	if (!targetname.isEmpty()) {
		QVector<QTreeWidgetItem*> byTargetname = findEntitiesByValue(QStringLiteral("target"), targetname);
		connected.append(byTargetname);
	}
	
	// Find by killtarget
	QString killtarget = getEntityKey(entityItem, QStringLiteral("killtarget"));
	if (!killtarget.isEmpty()) {
		QVector<QTreeWidgetItem*> byKilltarget = findEntitiesByValue(QStringLiteral("targetname"), killtarget);
		connected.append(byKilltarget);
	}
	
	m_connectionCount = connected.size();
	return connected;
}

QVector<QTreeWidgetItem*> EntityWalker::findConnectedByKey(const QTreeWidgetItem* entityItem, const QString& keyName) {
	if (!entityItem) return QVector<QTreeWidgetItem*>();
	
	QString value = getEntityKey(entityItem, keyName);
	if (value.isEmpty()) return QVector<QTreeWidgetItem*>();
	
	// Determine target key based on connection type
	QString targetKey;
	ConnectionType type = getConnectionType(keyName);
	
	switch (type) {
		case Target:
			targetKey = QStringLiteral("targetname");
			break;
		case Targetname:
			targetKey = QStringLiteral("target");
			break;
		case Killtarget:
			targetKey = QStringLiteral("targetname");
			break;
		default:
			targetKey = keyName; // For custom keys, use same key
			break;
	}
	
	return findEntitiesByValue(targetKey, value);
}

void EntityWalker::selectConnected(const QTreeWidgetItem* entityItem) {
	QVector<QTreeWidgetItem*> connected = findConnected(entityItem);
	
	// In full implementation, would:
	// 1. Clear current selection
	// 2. Select all connected entities
	// 3. Update viewport highlighting
	
	m_connectionCount = connected.size();
}

void EntityWalker::walkToNext(const QTreeWidgetItem* entityItem) {
	QVector<QTreeWidgetItem*> connected = findConnected(entityItem);
	if (connected.isEmpty()) return;
	
	// In full implementation, would:
	// 1. Find current position in connection list
	// 2. Move to next entity
	// 3. Select and focus on it
}

void EntityWalker::walkToPrevious(const QTreeWidgetItem* entityItem) {
	QVector<QTreeWidgetItem*> connected = findConnected(entityItem);
	if (connected.isEmpty()) return;
	
	// In full implementation, would:
	// 1. Find current position in connection list
	// 2. Move to previous entity
	// 3. Select and focus on it
}

QVector<QTreeWidgetItem*> EntityWalker::findConnectedByType(const QTreeWidgetItem* entityItem, ConnectionType type) {
	if (!entityItem) return QVector<QTreeWidgetItem*>();
	
	QString keyName;
	switch (type) {
		case Target:
			keyName = QStringLiteral("target");
			break;
		case Targetname:
			keyName = QStringLiteral("targetname");
			break;
		case Killtarget:
			keyName = QStringLiteral("killtarget");
			break;
		case Team:
			keyName = QStringLiteral("team");
			break;
		default:
			return QVector<QTreeWidgetItem*>();
	}
	
	return findConnectedByKey(entityItem, keyName);
}

QString EntityWalker::getEntityKey(const QTreeWidgetItem* entityItem, const QString& keyName) const {
	Q_UNUSED(keyName);
	if (!entityItem) return QString();
	
	// In full implementation, would:
	// 1. Get entity data from item
	// 2. Look up key in entity properties
	// 3. Return value
	
	// For now, simplified - would need access to entity data structure
	return QString();
}

QVector<QTreeWidgetItem*> EntityWalker::findEntitiesByValue(const QString& keyName, const QString& value) const {
	Q_UNUSED(keyName);
	Q_UNUSED(value);
	
	// In full implementation, would:
	// 1. Iterate through all entities in outliner
	// 2. Check entity key value
	// 3. Return matching entities
	
	return QVector<QTreeWidgetItem*>();
}

EntityWalker::ConnectionType EntityWalker::getConnectionType(const QString& keyName) const {
	if (keyName == QStringLiteral("target")) return Target;
	if (keyName == QStringLiteral("targetname")) return Targetname;
	if (keyName == QStringLiteral("killtarget")) return Killtarget;
	if (keyName == QStringLiteral("team")) return Team;
	return Custom;
}
