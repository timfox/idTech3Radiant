#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

class AutosaveManager : public QObject {
	Q_OBJECT
public:
	explicit AutosaveManager(QWidget* parent = nullptr);
	
	void setMapPath(const QString& path);
	QString mapPath() const { return m_mapPath; }
	
	void setAutosaveInterval(int minutes);
	int autosaveInterval() const { return m_autosaveIntervalMinutes; }
	
	void enableAutosave(bool enable);
	bool isAutosaveEnabled() const { return m_autosaveEnabled; }
	
	// Manual save trigger
	void saveNow();
	
	// Get recovery files
	static QStringList getRecoveryFiles();
	static bool hasRecoveryFiles();
	
	// Recover from crash
	static bool recoverFromCrash(const QString& recoveryFile, QString& recoveredPath);

Q_SIGNALS:
	void autosaveCompleted(const QString& path);
	void autosaveFailed(const QString& error);

private Q_SLOTS:
	void onAutosaveTimer();

private:
	QString getAutosavePath() const;
	QString getRecoveryPath() const;
	
	QTimer* m_timer = nullptr;
	QString m_mapPath;
	int m_autosaveIntervalMinutes = 5;
	bool m_autosaveEnabled = true;
	QWidget* m_parentWidget = nullptr;
};
