#include "qt_env.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

QtRadiantEnv InitQtRadiantEnv() {
	QtRadiantEnv env{};

	// Application path (install root where binaries live)
	env.appPath = QCoreApplication::applicationDirPath();

	// Unversioned ~/.radiant/ for prefs/logs
	env.tempPath = QDir::homePath() + QStringLiteral("/.radiant/");
	QDir().mkpath(env.tempPath);

	// Games path under install root, consistent with existing layout
	env.gamesPath = env.appPath + QStringLiteral("/games/");
	QDir().mkpath(env.gamesPath);

	env.logFile = env.tempPath + QStringLiteral("radiant_qt.log");

	return env;
}
