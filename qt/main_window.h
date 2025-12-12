#pragma once

#include <QMainWindow>
#include <QVector3D>
#include <QTreeWidgetItem>

#include "qt_env.h"

class QPlainTextEdit;
class QLabel;
class VkViewportWidget;
class QTreeWidget;
class QListWidget;
class QDoubleSpinBox;
class QLineEdit;
class QComboBox;

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
	void createBrushAtSelection();
	void createEntityAtSelection();
	bool hasSelection() const;
	void populateInspectorForItem(QTreeWidgetItem* item);

	QPlainTextEdit* m_console = nullptr;
	QLabel* m_status = nullptr;
	QTreeWidget* m_outliner = nullptr;
	QWidget* m_inspector = nullptr;
	QListWidget* m_textureList = nullptr;
	VkViewportWidget* m_viewPerspective = nullptr;
	VkViewportWidget* m_viewTop = nullptr;
	VkViewportWidget* m_viewFront = nullptr;
	VkViewportWidget* m_viewSide = nullptr;
	QDoubleSpinBox* m_posX = nullptr;
	QDoubleSpinBox* m_posY = nullptr;
	QDoubleSpinBox* m_posZ = nullptr;
	QLineEdit* m_nameEdit = nullptr;
	QComboBox* m_typeCombo = nullptr;
	QVector3D m_lastSelection{0.0f, 0.0f, 0.0f};
	QString m_currentMaterial;
	QAction* m_createBrushAction = nullptr;
	QAction* m_createEntityAction = nullptr;
};
