#pragma once

#include <QDialog>
#include <QKeySequence>
#include <QMap>

QT_BEGIN_NAMESPACE
class QTableWidget;
class QPushButton;
class QLineEdit;
class QLabel;
QT_END_NAMESPACE

struct KeybindAction {
	QString name;
	QString description;
	QKeySequence defaultSequence;
	QKeySequence currentSequence;
};

class KeybindEditor : public QDialog {
	Q_OBJECT
public:
	explicit KeybindEditor(QWidget* parent = nullptr);
	
	// Load/save keybinds
	void loadKeybinds();
	void saveKeybinds();
	QKeySequence getKeybind(const QString& actionName) const;
	void setKeybind(const QString& actionName, const QKeySequence& sequence);

private Q_SLOTS:
	void onItemSelectionChanged();
	void onAssignKeybind();
	void onResetKeybind();
	void onResetAll();
	void onFilterChanged(const QString& text);

private:
	void populateKeybinds();
	void updateKeybindDisplay();
	QString keySequenceToString(const QKeySequence& seq) const;
	
	QTableWidget* m_table = nullptr;
	QPushButton* m_assignBtn = nullptr;
	QPushButton* m_resetBtn = nullptr;
	QPushButton* m_resetAllBtn = nullptr;
	QLineEdit* m_filterEdit = nullptr;
	QLabel* m_statusLabel = nullptr;
	
	QMap<QString, KeybindAction> m_keybinds;
	QString m_currentAction;
};
