#pragma once

#include <QMainWindow>

#include "qt_env.h"

class QPlainTextEdit;
class QLabel;
class VkViewportWidget;
class QTreeWidget;
class QListWidget;

class RadiantMainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit RadiantMainWindow(const QtRadiantEnv& env, QWidget* parent = nullptr);

private:
	void buildUi(const QtRadiantEnv& env);
	void appendLogInfo(const QtRadiantEnv& env);
	void populateOutliner();
	void populateInspector();
	void populateTextureBrowser(const QtRadiantEnv& env);
	void setOutlinerLoadedMap(const QString& path);

	QPlainTextEdit* m_console = nullptr;
	QLabel* m_status = nullptr;
	QTreeWidget* m_outliner = nullptr;
	QWidget* m_inspector = nullptr;
	QListWidget* m_textureList = nullptr;
};
