// Minimal Qt entrypoint for Radiant Qt prototype.
#include <QApplication>
#include <QSplashScreen>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QPalette>
#include <QStyleFactory>
#include <QPixmap>
#include <QString>

#include "qt_env.h"
#include "main_window.h"
#include "theme_manager.h"
#include "snapping_system.h"

int main(int argc, char* argv[]) {
	// Avoid double titlebars on Wayland by disabling Qt's client-side decoration
	// Only apply automatically when running under Wayland; honor existing env if set.
	if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY") &&
	    !qEnvironmentVariableIsSet("QT_WAYLAND_DISABLE_WINDOWDECORATION")) {
		qputenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1");
	}

	QApplication app(argc, argv);

	// Show splash screen
	QPixmap splashPixmap(QCoreApplication::applicationDirPath() + "/../radiant/install/bitmaps/splash.png");
	QSplashScreen splash(splashPixmap);
	splash.show();
	splash.showMessage(QString("Qt Radiant v0.1 - Qt %1").arg(QT_VERSION_STR), Qt::AlignBottom | Qt::AlignLeft, Qt::white);
	app.processEvents(); // Allow splash to show

	// Initialize theme system
	splash.showMessage("Loading themes...", Qt::AlignBottom | Qt::AlignLeft, Qt::white);
	QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

	// Load themes from themes directory
	QString themesDir = QCoreApplication::applicationDirPath() + "/../radiant/themes";
	ThemeManager::instance().loadThemesFromDirectory(themesDir);

	// Apply default theme (Default Dark)
	ThemeManager::instance().applyTheme("Default Dark");

	splash.showMessage("Loading environment...", Qt::AlignBottom | Qt::AlignLeft, Qt::white);
	const QtRadiantEnv env = InitQtRadiantEnv();

	splash.showMessage("Creating main window...", Qt::AlignBottom | Qt::AlignLeft, Qt::white);
	RadiantMainWindow window(env);

	splash.showMessage("Starting up...", Qt::AlignBottom | Qt::AlignLeft, Qt::white);
	
	// Check for command line map file argument
	// Support: radiant_qt mapfile.map
	// Also support: radiant_qt --map mapfile.map
	QString mapFile;
	for (int i = 1; i < argc; ++i) {
		QString arg = QString::fromLocal8Bit(argv[i]);
		if (arg == QStringLiteral("--map") && i + 1 < argc) {
			mapFile = QString::fromLocal8Bit(argv[++i]);
			break;
		} else if (arg.endsWith(QStringLiteral(".map"), Qt::CaseInsensitive)) {
			mapFile = arg;
			break;
		}
	}
	
	// Load map if provided
	if (!mapFile.isEmpty()) {
		QFileInfo fileInfo(mapFile);
		if (fileInfo.exists() && fileInfo.isFile()) {
			splash.showMessage(QString("Loading map: %1").arg(fileInfo.fileName()), Qt::AlignBottom | Qt::AlignLeft, Qt::white);
			// Load the map file
			window.loadMap(mapFile);
		} else {
			QTextStream(stderr) << "Warning: Map file not found: " << mapFile << "\n";
		}
	}
	
	window.show();

	// Allow window to render before hiding splash
	app.processEvents();

	// Hide splash screen now that main window is visible
	splash.hide();

	return app.exec();
}
