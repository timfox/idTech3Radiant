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
#include <QSettings>
#include <QDir>

#include "qt_env.h"
#include "main_window.h"
#include "theme_manager.h"
#include "snapping_system.h"

int main(int argc, char* argv[]) {
	// Note: On Wayland, window decorations are handled by the compositor
	// We don't need to disable decorations here - let the window manager handle it

	// Set organization and application name for QSettings
	QCoreApplication::setOrganizationName("QtRadiant");
	QCoreApplication::setApplicationName("QtRadiant");

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
	// Try multiple possible paths
	QString themesDir;
	QStringList possiblePaths = {
		QCoreApplication::applicationDirPath() + "/../radiant/themes",
		QCoreApplication::applicationDirPath() + "/radiant/themes",
		QStringLiteral("radiant/themes"),
		QStringLiteral("./radiant/themes")
	};
	
	for (const QString& path : possiblePaths) {
		QDir dir(path);
		if (dir.exists() && dir.entryInfoList(QStringList() << "*.json", QDir::Files).size() > 0) {
			themesDir = path;
			break;
		}
	}
	
	if (!themesDir.isEmpty()) {
		ThemeManager::instance().loadThemesFromDirectory(themesDir);
	}

	// Load saved theme from settings, or use default
	QSettings settings;
	QString savedTheme = settings.value("editor/theme", "Default Dark").toString();
	
	// Verify the theme exists, fallback to default if not
	if (!ThemeManager::instance().availableThemes().contains(savedTheme)) {
		savedTheme = "Default Dark";
		if (!ThemeManager::instance().availableThemes().contains(savedTheme)) {
			// If Default Dark doesn't exist, use the first available theme
			QStringList themes = ThemeManager::instance().availableThemes();
			if (!themes.isEmpty()) {
				savedTheme = themes.first();
			}
		}
	}
	
	ThemeManager::instance().applyTheme(savedTheme);

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
