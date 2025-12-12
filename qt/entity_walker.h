#pragma once

#include <QString>
#include <QVector>

class QTreeWidgetItem;

// Connected entities selector/walker
class EntityWalker {
public:
	EntityWalker();
	
	// Find connected entities
	QVector<QTreeWidgetItem*> findConnected(const QTreeWidgetItem* entityItem);
	QVector<QTreeWidgetItem*> findConnectedByKey(const QTreeWidgetItem* entityItem, const QString& keyName);
	
	// Walk connections
	void selectConnected(const QTreeWidgetItem* entityItem);
	void walkToNext(const QTreeWidgetItem* entityItem);
	void walkToPrevious(const QTreeWidgetItem* entityItem);
	
	// Connection types
	enum ConnectionType {
		Target,      // target key
		Targetname,  // targetname key
		Killtarget,  // killtarget key
		Team,        // team key
		Custom       // Custom key
	};
	
	QVector<QTreeWidgetItem*> findConnectedByType(const QTreeWidgetItem* entityItem, ConnectionType type);
	
	// Statistics
	int connectionCount() const { return m_connectionCount; }
	void resetStats() { m_connectionCount = 0; }

private:
	int m_connectionCount{0};
	
	// Connection helpers
	QString getEntityKey(const QTreeWidgetItem* entityItem, const QString& keyName) const;
	QVector<QTreeWidgetItem*> findEntitiesByValue(const QString& keyName, const QString& value) const;
	ConnectionType getConnectionType(const QString& keyName) const;
};
