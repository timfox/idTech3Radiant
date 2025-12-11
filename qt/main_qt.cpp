// Minimal Qt entrypoint for Radiant Qt prototype.
#include <QApplication>
#include <QTextStream>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>

#include "qt_env.h"
#include "main_window.h"

int main(int argc, char* argv[]) {
	QApplication app(argc, argv);

	// Maya/UE-style dark theme
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
	window.show();

	return app.exec();
}
