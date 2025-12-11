// Minimal Qt entrypoint for Radiant Qt prototype.
#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QTextStream>
#include <QFile>

#include "qt_env.h"

int main(int argc, char* argv[]) {
	QApplication app(argc, argv);

	const QtRadiantEnv env = InitQtRadiantEnv();

	// Basic log line to prove we can touch shared paths.
	QFile logFile(env.logFile);
	if ( logFile.open(QIODevice::Append | QIODevice::Text) ) {
		QTextStream ts(&logFile);
		ts << "Radiant Qt started (appPath=" << env.appPath
		   << ", tempPath=" << env.tempPath
		   << ", gamesPath=" << env.gamesPath << ")\n";
	}

	QMainWindow window;
	window.setWindowTitle(QStringLiteral("Radiant Qt (prototype)"));

	auto *label = new QLabel(QStringLiteral(
		"Radiant Qt prototype — UI TBD\n"
		"app: %1\n"
		"temp: %2\n"
		"games: %3").arg(env.appPath, env.tempPath, env.gamesPath));
	label->setAlignment(Qt::AlignCenter);
	window.setCentralWidget(label);

	window.resize(800, 600);
	window.show();

	return app.exec();
}
