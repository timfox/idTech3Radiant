#pragma once

#include <QString>

struct QtRadiantEnv {
	QString appPath;
	QString tempPath;
	QString gamesPath;
	QString logFile;
};

// Initialize basic paths (unversioned ~/.radiant for temp/prefs) and ensure directories exist.
QtRadiantEnv InitQtRadiantEnv();
