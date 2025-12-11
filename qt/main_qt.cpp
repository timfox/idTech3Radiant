// Minimal Qt entrypoint for Radiant Qt prototype.
#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QTextStream>
#include <QFile>
#include <QVBoxLayout>

#include "vk_viewport.h"

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

	auto *central = new QWidget();
	auto *layout = new QVBoxLayout(central);
	layout->setContentsMargins(0, 0, 0, 0);

	auto *info = new QLabel(QStringLiteral(
		"Radiant Qt prototype\n"
		"app: %1\n"
		"temp: %2\n"
		"games: %3").arg(env.appPath, env.tempPath, env.gamesPath));
	info->setAlignment(Qt::AlignLeft);
	info->setMargin(8);

	// Hook up the engine Vulkan renderer (if present) for a future WYSIWYG PBR viewport.
	QString rendererHint = env.appPath + QStringLiteral("/../build/idtech3_vulkan_x86_64.so");
	auto *viewport = new VkViewportWidget(rendererHint);

	layout->addWidget(info, /*stretch*/0);
	layout->addWidget(viewport, /*stretch*/1);

	window.setCentralWidget(central);
	window.resize(1280, 720);
	window.show();

	return app.exec();
}
