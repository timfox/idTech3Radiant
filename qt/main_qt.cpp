// Minimal Qt entrypoint for Radiant Qt prototype.
#include <QApplication>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QPalette>
#include <QStyleFactory>

#include "qt_env.h"
#include "main_window.h"

int main(int argc, char* argv[]) {
	// Avoid double titlebars on Wayland by disabling Qt's client-side decoration
	// Only apply automatically when running under Wayland; honor existing env if set.
	if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY") &&
	    !qEnvironmentVariableIsSet("QT_WAYLAND_DISABLE_WINDOWDECORATION")) {
		qputenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1");
	}

	QApplication app(argc, argv);

	// Dark theme
	QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
	QPalette dark;
	dark.setColor(QPalette::Window, QColor(37, 37, 38));
	dark.setColor(QPalette::WindowText, Qt::white);
	dark.setColor(QPalette::Base, QColor(30, 30, 30));
	dark.setColor(QPalette::AlternateBase, QColor(45, 45, 48));
	dark.setColor(QPalette::ToolTipBase, Qt::white);
	dark.setColor(QPalette::ToolTipText, Qt::white);
	dark.setColor(QPalette::Text, Qt::white);
	dark.setColor(QPalette::Button, QColor(45, 45, 48));
	dark.setColor(QPalette::ButtonText, Qt::white);
	dark.setColor(QPalette::BrightText, Qt::red);
	dark.setColor(QPalette::Link, QColor(90, 160, 255));
	dark.setColor(QPalette::Highlight, QColor(90, 160, 255));
	dark.setColor(QPalette::HighlightedText, Qt::black);
	app.setPalette(dark);

	const QtRadiantEnv env = InitQtRadiantEnv();

	RadiantMainWindow window(env);
	
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
			// Load the map file
			window.loadMap(mapFile);
		} else {
			QTextStream(stderr) << "Warning: Map file not found: " << mapFile << "\n";
		}
	}
	
	window.show();

	return app.exec();
}
