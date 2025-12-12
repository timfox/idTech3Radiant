#include "main_window.h"

#include <QAction>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>

#include "vk_viewport.h"

RadiantMainWindow::RadiantMainWindow(const QtRadiantEnv& env, QWidget* parent)
	: QMainWindow(parent)
{
	buildUi(env);
	appendLogInfo(env);
}

void RadiantMainWindow::buildUi(const QtRadiantEnv& env){
	setWindowTitle(QStringLiteral("Radiant Qt (prototype)"));

	// Central viewport stack; try to locate renderer relative to install dir
	QString rendererHint = env.appPath + QStringLiteral("/../../idtech3_vulkan_x86_64.so");
	auto *viewport = new VkViewportWidget(rendererHint, this);
	setCentralWidget(viewport);

	// Console dock
	auto *consoleDock = new QDockWidget(QStringLiteral("Console"), this);
	consoleDock->setObjectName(QStringLiteral("ConsoleDock"));
	m_console = new QPlainTextEdit(consoleDock);
	m_console->setReadOnly(true);
	consoleDock->setWidget(m_console);
	addDockWidget(Qt::BottomDockWidgetArea, consoleDock);

	// Info dock
	auto *infoDock = new QDockWidget(QStringLiteral("Info"), this);
	infoDock->setObjectName(QStringLiteral("InfoDock"));
	auto *info = new QLabel(QStringLiteral(
		"Radiant Qt prototype (Maya-style dockable UI)\n"
		"app: %1\n"
		"temp: %2\n"
		"games: %3").arg(env.appPath, env.tempPath, env.gamesPath));
	info->setAlignment(Qt::AlignLeft);
	info->setMargin(8);
	infoDock->setWidget(info);
	addDockWidget(Qt::LeftDockWidgetArea, infoDock);

	// Outliner dock
	auto *outlinerDock = new QDockWidget(QStringLiteral("Outliner"), this);
	outlinerDock->setObjectName(QStringLiteral("OutlinerDock"));
	m_outliner = new QTreeWidget(outlinerDock);
	m_outliner->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Type")});
	populateOutliner();
	outlinerDock->setWidget(m_outliner);
	addDockWidget(Qt::LeftDockWidgetArea, outlinerDock);

	// Inspector dock
	auto *inspDock = new QDockWidget(QStringLiteral("Inspector"), this);
	inspDock->setObjectName(QStringLiteral("InspectorDock"));
	m_inspector = new QWidget(inspDock);
	populateInspector();
	inspDock->setWidget(m_inspector);
	addDockWidget(Qt::RightDockWidgetArea, inspDock);

	// Menu
	auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
	auto *openMapAct = new QAction(QStringLiteral("Open Map..."), this);
	auto *quitAct = new QAction(QStringLiteral("Quit"), this);
	connect(openMapAct, &QAction::triggered, this, [this](){
		QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Map or BSP"), QString(), QStringLiteral("Maps (*.map *.bsp);;All Files (*.*)"));
		if ( path.isEmpty() ) return;
		if ( auto *vp = qobject_cast<VkViewportWidget*>(centralWidget()) ) {
			if ( vp->loadMap(path) ) {
				m_console->appendPlainText(QStringLiteral("Loaded map: %1").arg(path));
				setOutlinerLoadedMap(path);
			} else {
				m_console->appendPlainText(QStringLiteral("Failed to load map: %1").arg(path));
			}
		}
	});
	connect(quitAct, &QAction::triggered, this, &QWidget::close);
	fileMenu->addAction(openMapAct);
	fileMenu->addAction(quitAct);

	// Toolbar stub
	auto *tb = addToolBar(QStringLiteral("Main"));
	tb->setMovable(true);
	tb->addAction(openMapAct);
	tb->addAction(QStringLiteral("Select"));
	tb->addAction(QStringLiteral("Move"));
	tb->addAction(QStringLiteral("Rotate"));
	tb->addAction(QStringLiteral("Scale"));
	tb->addSeparator();
	tb->addAction(quitAct);

	// Status bar
	m_status = new QLabel(QStringLiteral("Ready"));
	statusBar()->addPermanentWidget(m_status);

	resize(1280, 800);
}

void RadiantMainWindow::appendLogInfo(const QtRadiantEnv& env){
	if ( !m_console ) {
		return;
	}
	const QString line = QStringLiteral("Started Radiant Qt (app=%1 temp=%2 games=%3)")
		.arg(env.appPath, env.tempPath, env.gamesPath);
	m_console->appendPlainText(line);

	// Mirror to log file
	QFile log(env.logFile);
	if ( log.open(QIODevice::Append | QIODevice::Text) ) {
		QTextStream ts(&log);
		ts << line << "\n";
	}
	if ( m_status ) {
		m_status->setText(QStringLiteral("Log: %1").arg(env.logFile));
	}
}

void RadiantMainWindow::populateOutliner(){
	if ( !m_outliner ) return;
	m_outliner->clear();
	QStringList roots = {QStringLiteral("World"), QStringLiteral("Lights"), QStringLiteral("Props"), QStringLiteral("Volumes")};
	for ( const auto& rootName : roots ) {
		auto *root = new QTreeWidgetItem(m_outliner, {rootName, QStringLiteral("Group")});
		for (int i = 0; i < 3; ++i) {
			auto *child = new QTreeWidgetItem(root, {QStringLiteral("%1_%2").arg(rootName).arg(i), QStringLiteral("Entity")});
			child->addChild(new QTreeWidgetItem({QStringLiteral("mesh_%1").arg(i), QStringLiteral("Mesh")}));
		}
		m_outliner->addTopLevelItem(root);
	}
	m_outliner->expandAll();
}

void RadiantMainWindow::populateInspector(){
	if ( !m_inspector ) return;

	auto *layout = new QFormLayout(m_inspector);
	layout->setContentsMargins(8, 8, 8, 8);

	auto *nameEdit = new QLineEdit(QStringLiteral("selected_object"), m_inspector);
	layout->addRow(QStringLiteral("Name"), nameEdit);

	auto *typeCombo = new QComboBox(m_inspector);
	typeCombo->addItems({QStringLiteral("Entity"), QStringLiteral("Brush"), QStringLiteral("Light"), QStringLiteral("Volume")});
	layout->addRow(QStringLiteral("Type"), typeCombo);

	auto *posX = new QDoubleSpinBox(m_inspector); posX->setRange(-99999, 99999); posX->setValue(0.0);
	auto *posY = new QDoubleSpinBox(m_inspector); posY->setRange(-99999, 99999); posY->setValue(0.0);
	auto *posZ = new QDoubleSpinBox(m_inspector); posZ->setRange(-99999, 99999); posZ->setValue(0.0);
	layout->addRow(QStringLiteral("Position X"), posX);
	layout->addRow(QStringLiteral("Position Y"), posY);
	layout->addRow(QStringLiteral("Position Z"), posZ);

	auto *scale = new QDoubleSpinBox(m_inspector); scale->setRange(0.001, 1000.0); scale->setValue(1.0);
	layout->addRow(QStringLiteral("Uniform Scale"), scale);

	auto *matCombo = new QComboBox(m_inspector);
	matCombo->addItems({QStringLiteral("Default"), QStringLiteral("Metal"), QStringLiteral("Rough"), QStringLiteral("Emissive")});
	layout->addRow(QStringLiteral("Material"), matCombo);
}

void RadiantMainWindow::setOutlinerLoadedMap(const QString& path){
	if ( !m_outliner ) return;
	if ( m_outliner->topLevelItemCount() == 0 ) {
		populateOutliner();
	}
	auto *root = m_outliner->topLevelItem(0);
	if ( root ) {
		root->setText(0, QStringLiteral("Map: %1").arg(path));
	}
}
