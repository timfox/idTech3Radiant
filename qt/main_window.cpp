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
#include <QSplitter>
#include <QListWidget>
#include <QDirIterator>
#include <QTableWidget>
#include <QPushButton>
#include <QProcess>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QDirIterator>
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

	// Quad-view layout (Perspective, Top, Front, Side)
	m_viewPerspective = new VkViewportWidget(rendererHint, VkViewportWidget::ViewType::Perspective, this);
	m_viewTop         = new VkViewportWidget(rendererHint, VkViewportWidget::ViewType::Top, this);
	m_viewFront       = new VkViewportWidget(rendererHint, VkViewportWidget::ViewType::Front, this);
	m_viewSide        = new VkViewportWidget(rendererHint, VkViewportWidget::ViewType::Side, this);

	auto onSelect = [this](const QVector3D& pos){
		m_lastSelection = pos;
		if ( m_status ) {
			m_status->setText(QStringLiteral("Selection: (%1, %2, %3)")
				.arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1).arg(pos.z(), 0, 'f', 1));
		}
		if ( m_posX && m_posY && m_posZ ) {
			m_posX->setValue(pos.x());
			m_posY->setValue(pos.y());
			m_posZ->setValue(pos.z());
		}
	};
	connect(m_viewPerspective, &VkViewportWidget::selectionChanged, this, onSelect);
	connect(m_viewTop, &VkViewportWidget::selectionChanged, this, onSelect);
	connect(m_viewFront, &VkViewportWidget::selectionChanged, this, onSelect);
	connect(m_viewSide, &VkViewportWidget::selectionChanged, this, onSelect);

	auto *splitTop = new QSplitter(Qt::Horizontal, this);
	splitTop->addWidget(m_viewPerspective);
	splitTop->addWidget(m_viewTop);

	auto *splitBottom = new QSplitter(Qt::Horizontal, this);
	splitBottom->addWidget(m_viewFront);
	splitBottom->addWidget(m_viewSide);

	auto *splitVertical = new QSplitter(Qt::Vertical, this);
	splitVertical->addWidget(splitTop);
	splitVertical->addWidget(splitBottom);

	auto *center = new QWidget(this);
	auto *centerLayout = new QVBoxLayout(center);
	centerLayout->setContentsMargins(0,0,0,0);
	centerLayout->addWidget(splitVertical);
	setCentralWidget(center);

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

	// Texture browser dock (stubbed, file list from base/textures)
	auto *texDock = new QDockWidget(QStringLiteral("Textures"), this);
	texDock->setObjectName(QStringLiteral("TexturesDock"));
	m_textureList = new QListWidget(texDock);
	texDock->setWidget(m_textureList);
	addDockWidget(Qt::LeftDockWidgetArea, texDock);
	populateTextureBrowser(env);
	connect(m_textureList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item){
		if ( !item ) return;
		m_currentMaterial = item->text();
		if ( m_console ) {
			m_console->appendPlainText(QStringLiteral("Apply material: %1 to selection at (%2,%3,%4)")
				.arg(m_currentMaterial)
				.arg(m_lastSelection.x(), 0, 'f', 1)
				.arg(m_lastSelection.y(), 0, 'f', 1)
				.arg(m_lastSelection.z(), 0, 'f', 1));
		}
	});

	// Compile front-end (stub)
	auto *buildDock = new QDockWidget(QStringLiteral("Build/Compile"), this);
	buildDock->setObjectName(QStringLiteral("BuildDock"));
	auto *buildBody = new QWidget(buildDock);
	auto *buildLayout = new QVBoxLayout(buildBody);
	buildLayout->setContentsMargins(8, 8, 8, 8);
	auto *runBtn = new QPushButton(QStringLiteral("Run q3map2 (stub)"), buildBody);
	auto *presetCombo = new QComboBox(buildBody);
	presetCombo->addItems({QStringLiteral("BSP"), QStringLiteral("BSP + VIS"), QStringLiteral("BSP + VIS + LIGHT")});
	auto *buildLog = new QPlainTextEdit(buildBody);
	buildLog->setReadOnly(true);
	buildLayout->addWidget(presetCombo);
	buildLayout->addWidget(runBtn);
	buildLayout->addWidget(buildLog);
	buildLayout->addStretch(1);
	buildBody->setLayout(buildLayout);
	buildDock->setWidget(buildBody);
	addDockWidget(Qt::RightDockWidgetArea, buildDock);
	connect(runBtn, &QPushButton::clicked, this, [this, presetCombo, buildLog](){
		const QString msg = QStringLiteral("[Compile] %1 (stubbed; wire q3map2 next)").arg(presetCombo->currentText());
		buildLog->appendPlainText(msg);
		if ( m_console ) {
			m_console->appendPlainText(msg);
		}
	});

	// Menu
	auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
	auto *openMapAct = new QAction(QStringLiteral("Open Map..."), this);
	auto *saveMapAct = new QAction(QStringLiteral("Save Map..."), this);
	auto *quitAct = new QAction(QStringLiteral("Quit"), this);
	connect(openMapAct, &QAction::triggered, this, [this](){
		QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Map or BSP"), QString(), QStringLiteral("Maps (*.map *.bsp);;All Files (*.*)"));
		if ( path.isEmpty() ) return;
		VkViewportWidget* vp = m_viewPerspective ? m_viewPerspective : nullptr;
		if ( vp && vp->loadMap(path) ) {
			m_console->appendPlainText(QStringLiteral("Loaded map: %1").arg(path));
			setOutlinerLoadedMap(path);
		} else {
			m_console->appendPlainText(QStringLiteral("Failed to load map: %1").arg(path));
		}
	});

	connect(saveMapAct, &QAction::triggered, this, [this](){
		saveMap();
	});

	connect(quitAct, &QAction::triggered, this, &QWidget::close);
	fileMenu->addAction(openMapAct);
	fileMenu->addAction(saveMapAct);
	fileMenu->addSeparator();
	fileMenu->addAction(quitAct);

	// Toolbar with geometry tools
	auto *tb = addToolBar(QStringLiteral("Main"));
	tb->setMovable(true);
	tb->addAction(openMapAct);
	tb->addAction(saveMapAct);
	tb->addSeparator();

	// Selection and transform tools
	tb->addAction(QStringLiteral("Select"));
	tb->addAction(QStringLiteral("Move"));
	tb->addAction(QStringLiteral("Rotate"));
	tb->addAction(QStringLiteral("Scale"));
	tb->addSeparator();

	// Geometry creation tools
	m_createBrushAction = new QAction(QStringLiteral("Create Brush"), this);
	m_createBrushAction->setShortcut(QKeySequence("Ctrl+B"));
	connect(m_createBrushAction, &QAction::triggered, this, [this](){
		createBrushAtSelection();
	});
	tb->addAction(m_createBrushAction);

	m_createEntityAction = new QAction(QStringLiteral("Create Entity"), this);
	m_createEntityAction->setShortcut(QKeySequence("Ctrl+E"));
	connect(m_createEntityAction, &QAction::triggered, this, [this](){
		createEntityAtSelection();
	});
	tb->addAction(m_createEntityAction);

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

	m_nameEdit = new QLineEdit(QStringLiteral("selected_object"), m_inspector);
	layout->addRow(QStringLiteral("Name"), m_nameEdit);

	m_typeCombo = new QComboBox(m_inspector);
	m_typeCombo->addItems({QStringLiteral("Entity"), QStringLiteral("Brush"), QStringLiteral("Light"), QStringLiteral("Volume")});
	layout->addRow(QStringLiteral("Type"), m_typeCombo);

	m_posX = new QDoubleSpinBox(m_inspector); m_posX->setRange(-99999, 99999); m_posX->setValue(0.0);
	m_posY = new QDoubleSpinBox(m_inspector); m_posY->setRange(-99999, 99999); m_posY->setValue(0.0);
	m_posZ = new QDoubleSpinBox(m_inspector); m_posZ->setRange(-99999, 99999); m_posZ->setValue(0.0);
	layout->addRow(QStringLiteral("Position X"), m_posX);
	layout->addRow(QStringLiteral("Position Y"), m_posY);
	layout->addRow(QStringLiteral("Position Z"), m_posZ);

	auto *scale = new QDoubleSpinBox(m_inspector); scale->setRange(0.001, 1000.0); scale->setValue(1.0);
	layout->addRow(QStringLiteral("Uniform Scale"), scale);

	auto *matCombo = new QComboBox(m_inspector);
	matCombo->addItems({QStringLiteral("Default"), QStringLiteral("Metal"), QStringLiteral("Rough"), QStringLiteral("Emissive")});
	layout->addRow(QStringLiteral("Material"), matCombo);
	connect(matCombo, &QComboBox::currentTextChanged, this, [this](const QString& mat){
		m_currentMaterial = mat;
		if ( m_console ) {
			m_console->appendPlainText(QStringLiteral("Material set to %1").arg(mat));
		}
	});

	auto *targetEdit = new QLineEdit(m_inspector);
	layout->addRow(QStringLiteral("Target"), targetEdit);

	// Spawnflags (simple checkboxes)
	auto *spawnFlags = new QWidget(m_inspector);
	auto *spawnLayout = new QHBoxLayout(spawnFlags);
	spawnLayout->setContentsMargins(0,0,0,0);
	for (int i = 0; i < 4; ++i) {
		auto *cb = new QCheckBox(QStringLiteral("Flag %1").arg(i), spawnFlags);
		spawnLayout->addWidget(cb);
	}
	layout->addRow(QStringLiteral("Spawnflags"), spawnFlags);

	// Key/Value table
	auto *kvTable = new QTableWidget(0, 2, m_inspector);
	kvTable->setHorizontalHeaderLabels({QStringLiteral("Key"), QStringLiteral("Value")});
	kvTable->horizontalHeader()->setStretchLastSection(true);
	layout->addRow(QStringLiteral("Key/Values"), kvTable);

	auto *kvButtons = new QWidget(m_inspector);
	auto *kvBtnLayout = new QHBoxLayout(kvButtons);
	kvBtnLayout->setContentsMargins(0,0,0,0);
	auto *addKv = new QPushButton(QStringLiteral("Add KV"), kvButtons);
	auto *delKv = new QPushButton(QStringLiteral("Remove KV"), kvButtons);
	kvBtnLayout->addWidget(addKv);
	kvBtnLayout->addWidget(delKv);
	layout->addRow(QString(), kvButtons);

	connect(addKv, &QPushButton::clicked, this, [kvTable](){
		int row = kvTable->rowCount();
		kvTable->insertRow(row);
		kvTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("key")));
		kvTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("value")));
	});
	connect(delKv, &QPushButton::clicked, this, [kvTable](){
		int row = kvTable->currentRow();
		if ( row >= 0 ) kvTable->removeRow(row);
	});
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

void RadiantMainWindow::populateTextureBrowser(const QtRadiantEnv& env){
	if ( !m_textureList ) return;
	m_textureList->clear();

	// Look for textures under games/*/base*/textures
	QString gamesRoot = env.gamesPath;
	if (!gamesRoot.endsWith('/')) gamesRoot += '/';

	QStringList searchDirs;
	QDir gamesDir(gamesRoot);
	for (const QString& sub : gamesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		const QString baseDir = gamesRoot + sub + QStringLiteral("/base");
		if (QDir(baseDir).exists()) {
			searchDirs << baseDir + QStringLiteral("/textures");
		}
	}

	int added = 0;
	const QStringList exts = {QStringLiteral("*.tga"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png")};
	for (const QString& dirPath : searchDirs) {
		QDirIterator it(dirPath, exts, QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			const QString filePath = it.next();
			const QString rel = QDir(dirPath).relativeFilePath(filePath);
			m_textureList->addItem(rel);
			++added;
			if (added > 2048) break; // safety cap
		}
		if (added > 2048) break;
	}

	if (added == 0) {
		m_textureList->addItem(QStringLiteral("(no textures found)"));
	}
}

void RadiantMainWindow::createBrushAtSelection(){
	if (!hasSelection()) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("No selection - click in a viewport to set position"));
		}
		return;
	}

	// Create a simple cube brush at the selection position
	static int brushCount = 0;
	brushCount++;

	QString brushName = QStringLiteral("brush_%1").arg(brushCount);
	QTreeWidgetItem* brushItem = new QTreeWidgetItem(m_outliner, {brushName, QStringLiteral("brush")});

	// Store brush data (in a real implementation, this would be proper map geometry)
	brushItem->setData(0, Qt::UserRole, QVariant::fromValue(m_lastSelection));

	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Created brush '%1' at (%2,%3,%4)")
			.arg(brushName)
			.arg(m_lastSelection.x(), 0, 'f', 1)
			.arg(m_lastSelection.y(), 0, 'f', 1)
			.arg(m_lastSelection.z(), 0, 'f', 1));
	}

	// Update inspector with brush properties
	populateInspectorForItem(brushItem);
}

void RadiantMainWindow::createEntityAtSelection(){
	if (!hasSelection()) {
		if (m_console) {
			m_console->appendPlainText(QStringLiteral("No selection - click in a viewport to set position"));
		}
		return;
	}

	// Create a basic entity at the selection position
	static int entityCount = 0;
	entityCount++;

	QString entityName = QStringLiteral("entity_%1").arg(entityCount);
	QTreeWidgetItem* entityItem = new QTreeWidgetItem(m_outliner, {entityName, QStringLiteral("entity")});

	// Store entity data
	entityItem->setData(0, Qt::UserRole, QVariant::fromValue(m_lastSelection));

	if (m_console) {
		m_console->appendPlainText(QStringLiteral("Created entity '%1' at (%2,%3,%4)")
			.arg(entityName)
			.arg(m_lastSelection.x(), 0, 'f', 1)
			.arg(m_lastSelection.y(), 0, 'f', 1)
			.arg(m_lastSelection.z(), 0, 'f', 1));
	}

	// Update inspector with entity properties
	populateInspectorForItem(entityItem);
}

bool RadiantMainWindow::hasSelection() const {
	// Check if any viewport has a selection
	return (m_viewPerspective && m_viewPerspective->hasSelection()) ||
		   (m_viewTop && m_viewTop->hasSelection()) ||
		   (m_viewFront && m_viewFront->hasSelection()) ||
		   (m_viewSide && m_viewSide->hasSelection());
}

void RadiantMainWindow::populateInspectorForItem(QTreeWidgetItem* item){
	if (!item || !m_inspector) return;

	// Clear existing inspector content
	QWidget* oldWidget = m_inspector;
	if (oldWidget->layout()) {
		QLayout* layout = oldWidget->layout();
		QLayoutItem* child;
		while ((child = layout->takeAt(0)) != nullptr) {
			delete child->widget();
			delete child;
		}
		delete layout;
	}

	auto* layout = new QFormLayout(m_inspector);
	m_inspector->setLayout(layout);

	QString type = item->text(1);
	QString name = item->text(0);
	QVector3D pos = item->data(0, Qt::UserRole).value<QVector3D>();

	layout->addRow(new QLabel(QStringLiteral("Name:")), new QLabel(name));
	layout->addRow(new QLabel(QStringLiteral("Type:")), new QLabel(type));

	// Position controls
	m_posX = new QDoubleSpinBox(m_inspector);
	m_posY = new QDoubleSpinBox(m_inspector);
	m_posZ = new QDoubleSpinBox(m_inspector);
	m_posX->setRange(-99999, 99999);
	m_posY->setRange(-99999, 99999);
	m_posZ->setRange(-99999, 99999);
	m_posX->setValue(pos.x());
	m_posY->setValue(pos.y());
	m_posZ->setValue(pos.z());

	layout->addRow(QStringLiteral("X:"), m_posX);
	layout->addRow(QStringLiteral("Y:"), m_posY);
	layout->addRow(QStringLiteral("Z:"), m_posZ);

	if (type == QStringLiteral("brush")) {
		// Brush-specific properties
		auto* materialCombo = new QComboBox(m_inspector);
		materialCombo->addItems({QStringLiteral("common/caulk"), QStringLiteral("common/trigger"),
			QStringLiteral("textures/base_wall/metal"), QStringLiteral("textures/base_wall/stone")});
		layout->addRow(QStringLiteral("Material:"), materialCombo);
	} else if (type == QStringLiteral("entity")) {
		// Entity-specific properties
		m_nameEdit = new QLineEdit(name, m_inspector);
		m_typeCombo = new QComboBox(m_inspector);
		m_typeCombo->addItems({QStringLiteral("func_static"), QStringLiteral("info_player_start"),
			QStringLiteral("light"), QStringLiteral("trigger_multiple")});

		layout->addRow(QStringLiteral("Classname:"), m_typeCombo);
		layout->addRow(QStringLiteral("Targetname:"), m_nameEdit);
	}
}

void RadiantMainWindow::saveMap(){
	QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Map"), QString(), QStringLiteral("Maps (*.map);;All Files (*.*)"));
	if ( path.isEmpty() ) return;

	if ( saveMapToFile(path) ) {
		m_console->appendPlainText(QStringLiteral("Saved map: %1").arg(path));
	} else {
		m_console->appendPlainText(QStringLiteral("Failed to save map: %1").arg(path));
	}
}

bool RadiantMainWindow::saveMapToFile(const QString& path){
	QFile file(path);
	if ( !file.open(QIODevice::WriteOnly | QIODevice::Text) ) {
		return false;
	}

	QTextStream out(&file);

	// Write map header
	out << "// Radiant Qt exported map\n";
	out << "// Generated by Qt Radiant World Editor\n\n";

	// Write entities and brushes
	for ( int i = 0; i < m_outliner->topLevelItemCount(); ++i ) {
		QTreeWidgetItem* item = m_outliner->topLevelItem(i);
		if ( !item ) continue;

		QString type = item->text(1);
		QString name = item->text(0);

		if ( type == QStringLiteral("entity") ) {
			writeEntityToMap(out, item);
		} else if ( type == QStringLiteral("brush") ) {
			// For now, wrap brushes in worldspawn entity
			if ( i == 0 ) {
				out << "{\n";
				out << "\"classname\" \"worldspawn\"\n";
			}
			writeBrushToMap(out, item);
		}
	}

	// Close worldspawn entity if we have brushes
	bool hasBrushes = false;
	for ( int i = 0; i < m_outliner->topLevelItemCount(); ++i ) {
		if ( m_outliner->topLevelItem(i)->text(1) == QStringLiteral("brush") ) {
			hasBrushes = true;
			break;
		}
	}
	if ( hasBrushes ) {
		out << "}\n";
	}

	file.close();
	return true;
}

void RadiantMainWindow::writeEntityToMap(QTextStream& out, QTreeWidgetItem* item){
	QVector3D pos = item->data(0, Qt::UserRole).value<QVector3D>();
	QString name = item->text(0);

	out << "{\n";
	out << "\"classname\" \"func_static\"\n";
	out << "\"origin\" \"" << pos.x() << " " << pos.y() << " " << pos.z() << "\"\n";
	out << "\"targetname\" \"" << name << "\"\n";
	out << "}\n\n";
}

void RadiantMainWindow::writeBrushToMap(QTextStream& out, QTreeWidgetItem* item){
	QVector3D pos = item->data(0, Qt::UserRole).value<QVector3D>();

	// Create a simple cube brush (6 faces)
	// For a real implementation, this would use proper brush geometry
	const float size = 64.0f; // Half-extent

	// Bottom face (z = pos.z - size)
	out << "  {\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) common/caulk 0 0 0 1 1\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) common/caulk 0 0 0 1 1\n";
	out << "  }\n";

	// Top face (z = pos.z + size)
	out << "  {\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "  }\n";

	// Front face (y = pos.y - size)
	out << "  {\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "  }\n";

	// Back face (y = pos.y + size)
	out << "  {\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "  }\n";

	// Left face (x = pos.x - size)
	out << "  {\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "    ( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() - size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "  }\n";

	// Right face (x = pos.x + size)
	out << "  {\n";
	out << "    ( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "    ( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() - size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() - size) << " " << (pos.z() + size) << " ) ";
	out << "( " << (pos.x() + size) << " " << (pos.y() + size) << " " << (pos.z() + size) << " ) common/caulk 0 0 0 1 1\n";
	out << "  }\n\n";
}
