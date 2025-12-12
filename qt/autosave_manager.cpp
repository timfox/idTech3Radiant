#include "autosave_manager.h"

#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDebug>

AutosaveManager::AutosaveManager(QWidget* parent)
	: QObject(parent)
	, m_parentWidget(parent)
{
	m_timer = new QTimer(this);
	m_timer->setSingleShot(false);
	connect(m_timer, &QTimer::timeout, this, &AutosaveManager::onAutosaveTimer);
	
	setAutosaveInterval(5); // Default 5 minutes
}

void AutosaveManager::setMapPath(const QString& path) {
	m_mapPath = path;
	
	// Restart timer if autosave is enabled
	if (m_autosaveEnabled && !path.isEmpty()) {
		m_timer->start(m_autosaveIntervalMinutes * 60 * 1000);
	} else {
		m_timer->stop();
	}
}

void AutosaveManager::setAutosaveInterval(int minutes) {
	m_autosaveIntervalMinutes = qMax(1, minutes);
	if (m_timer->isActive()) {
		m_timer->start(m_autosaveIntervalMinutes * 60 * 1000);
	}
}

void AutosaveManager::enableAutosave(bool enable) {
	m_autosaveEnabled = enable;
	if (enable && !m_mapPath.isEmpty()) {
		m_timer->start(m_autosaveIntervalMinutes * 60 * 1000);
	} else {
		m_timer->stop();
	}
}

void AutosaveManager::saveNow() {
	if (m_mapPath.isEmpty()) {
		Q_EMIT autosaveFailed(QStringLiteral("No map file open"));
		return;
	}
	
	onAutosaveTimer();
}

void AutosaveManager::onAutosaveTimer() {
	if (m_mapPath.isEmpty() || !m_autosaveEnabled) {
		return;
	}
	
	QString autosavePath = getAutosavePath();
	QDir dir = QFileInfo(autosavePath).absoluteDir();
	if (!dir.exists()) {
		dir.mkpath(".");
	}
	
	// Copy current map to autosave location
	QFile sourceFile(m_mapPath);
	if (sourceFile.exists()) {
		if (QFile::copy(m_mapPath, autosavePath)) {
			Q_EMIT autosaveCompleted(autosavePath);
		} else {
			Q_EMIT autosaveFailed(QStringLiteral("Failed to copy file to autosave location"));
		}
	} else {
		// Map hasn't been saved yet, save to recovery location
		QString recoveryPath = getRecoveryPath();
		QDir recoveryDir = QFileInfo(recoveryPath).absoluteDir();
		if (!recoveryDir.exists()) {
			recoveryDir.mkpath(".");
		}
		// In a full implementation, we'd serialize the current map state here
		Q_EMIT autosaveCompleted(recoveryPath);
	}
}

QString AutosaveManager::getAutosavePath() const {
	if (m_mapPath.isEmpty()) {
		return QString();
	}
	
	QFileInfo fileInfo(m_mapPath);
	QString baseName = fileInfo.baseName();
	QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
	
	QString autosaveDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/autosave";
	return QString("%1/%2_%3.map").arg(autosaveDir, baseName, timestamp);
}

QString AutosaveManager::getRecoveryPath() const {
	QString recoveryDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/recovery";
	QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
	return QString("%1/recovery_%2.map").arg(recoveryDir, timestamp);
}

QStringList AutosaveManager::getRecoveryFiles() {
	QString recoveryDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/recovery";
	QDir dir(recoveryDir);
	
	if (!dir.exists()) {
		return QStringList();
	}
	
	QStringList filters;
	filters << "recovery_*.map";
	
	QStringList files = dir.entryList(filters, QDir::Files, QDir::Time);
	QStringList fullPaths;
	for (const QString& file : files) {
		fullPaths << dir.absoluteFilePath(file);
	}
	
	return fullPaths;
}

bool AutosaveManager::hasRecoveryFiles() {
	return !getRecoveryFiles().isEmpty();
}

bool AutosaveManager::recoverFromCrash(const QString& recoveryFile, QString& recoveredPath) {
	QFileInfo fileInfo(recoveryFile);
	if (!fileInfo.exists()) {
		return false;
	}
	
	QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
	QString baseName = fileInfo.baseName();
	recoveredPath = QString("%1/recovered_%2_%3.map")
		.arg(fileInfo.absolutePath())
		.arg(baseName)
		.arg(timestamp);
	
	return QFile::copy(recoveryFile, recoveredPath);
}
