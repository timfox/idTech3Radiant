#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>

class QTreeWidgetItem;

// Selection Sets - group selections for quick access (GTK Radiant feature)
class SelectionSets {
public:
	SelectionSets();
	
	// Set management
	void createSet(const QString& name);
	void deleteSet(const QString& name);
	void renameSet(const QString& oldName, const QString& newName);
	QStringList setNames() const;
	
	// Selection storage
	void storeSelection(const QString& setName, const QVector<QTreeWidgetItem*>& items);
	QVector<QTreeWidgetItem*> getSelection(const QString& setName) const;
	
	// Selection operations
	void selectSet(const QString& setName, QTreeWidgetItem* rootItem);
	void addToSet(const QString& setName, const QVector<QTreeWidgetItem*>& items);
	void removeFromSet(const QString& setName, const QVector<QTreeWidgetItem*>& items);
	
	// Persistence
	bool saveToFile(const QString& filePath) const;
	bool loadFromFile(const QString& filePath);

private:
	QMap<QString, QVector<QString>> m_sets; // Set name -> List of item identifiers
	QMap<QString, QTreeWidgetItem*> m_itemMap; // Item identifier -> Item pointer
	
	QString itemIdentifier(QTreeWidgetItem* item) const;
	QTreeWidgetItem* findItemByIdentifier(const QString& identifier, QTreeWidgetItem* root) const;
};
