#include "lua_workbench.h"

#include "commands.h"
#include "mainframe.h"
#include "preferences.h"
#include "qtdlgs.h"
#include "stream/stringstream.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileInfo>
#include <QLineEdit>
#include <QTextBrowser>
#include <QVector>
#include <QFrame>

namespace
{

QDockWidget* g_luaWorkbenchDock{};
QLabel* g_luaWorkbenchStatus{};
QLineEdit* g_luaWorkbenchSearch{};
QTextBrowser* g_luaWorkbenchDetails{};

struct LuaWorkbenchEntry
{
	const char* label;
	const CopiedString* path;
	const char* title;
	const char* summary;
};

const LuaWorkbenchEntry c_luaWorkbenchEntries[] = {
	{ "props.lua", &g_luaScriptProps, "Lua Props", "Author prop metadata, interactions, breakables, and prop-side scripting hooks." },
	{ "entities.lua", &g_luaScriptEntities, "Lua Entities", "Define gameplay entity behavior, volumes, triggers, spawners, and editor-exposed entity glue." },
	{ "items.lua", &g_luaScriptItems, "Lua Items", "Configure pickups, inventory, consumables, and equipment-driven gameplay systems." },
	{ "main.lua", &g_luaScriptMain, "Lua Main", "Own the main ruleset bootstrap, match flow, runtime setup, and shared utility entrypoints." },
	{ "objectives.lua", &g_luaScriptObjectives, "Lua Objectives", "Drive objective states, capture logic, encounter progression, and mission scripting." },
};

struct LuaWorkbenchRow
{
	QWidget* container{};
	QLabel* pathLabel{};
	const LuaWorkbenchEntry* entry{};
};

QVector<LuaWorkbenchRow> g_luaWorkbenchRows;

void LuaWorkbench_setStatus( const QString& text ){
	if ( g_luaWorkbenchStatus != nullptr ) {
		g_luaWorkbenchStatus->setText( text );
	}
}

void LuaWorkbench_openScript( const LuaWorkbenchEntry& entry, bool external ){
	if ( entry.path == nullptr || entry.path->empty() ) {
		LuaWorkbench_setStatus( StringStream( "Missing path for ", entry.label ).c_str() );
		return;
	}
	DoShaderView( entry.path->c_str(), entry.title, external );
	LuaWorkbench_setStatus(
		StringStream( external ? "Opened external: " : "Opened: ", entry.label ).c_str() );
}

QString LuaWorkbench_displayPath( const CopiedString& path ){
	if ( path.empty() ) {
		return "(not configured)";
	}
	const QFileInfo info( QString::fromUtf8( path.c_str() ) );
	return info.isAbsolute() ? info.absoluteFilePath() : info.filePath();
}

QString LuaWorkbench_entrySearchText( const LuaWorkbenchEntry& entry ){
	return QString( "%1 %2 %3 %4" )
		.arg( entry.label )
		.arg( entry.title )
		.arg( entry.summary )
		.arg( LuaWorkbench_displayPath( *entry.path ) );
}

void LuaWorkbench_refreshDetails( const LuaWorkbenchEntry* selectedEntry = nullptr ){
	if ( g_luaWorkbenchDetails == nullptr ) {
		return;
	}

	if ( selectedEntry == nullptr ) {
		g_luaWorkbenchDetails->setHtml(
			"<h3>Gameplay Scripting</h3>"
			"<p>Search scripts, open the core Lua files, and use the quick-create buttons to place gameplay entities directly in the scene.</p>"
			"<p>Recommended flow: place entities first, then open <b>entities.lua</b>, <b>objectives.lua</b>, or <b>main.lua</b> to wire up behavior.</p>"
		);
		return;
	}

	const QString path = LuaWorkbench_displayPath( *selectedEntry->path ).toHtmlEscaped();
	const bool configured = selectedEntry->path != nullptr && !selectedEntry->path->empty();
	g_luaWorkbenchDetails->setHtml(
		QString(
			"<h3>%1</h3>"
			"<p>%2</p>"
			"<p><b>Path:</b> %3<br/><b>Status:</b> %4</p>"
			"<p>Open it in the built-in editor for quick iteration or in your external editor for full script work.</p>"
		)
		.arg(
			QString::fromLatin1( selectedEntry->title ).toHtmlEscaped(),
			QString::fromLatin1( selectedEntry->summary ).toHtmlEscaped(),
			path,
			configured ? "Configured" : "Not configured"
		)
	);
}

void LuaWorkbench_refreshFilter(){
	const QString filter = g_luaWorkbenchSearch != nullptr ? g_luaWorkbenchSearch->text().trimmed() : QString();
	const LuaWorkbenchEntry* firstVisibleEntry = nullptr;

	for ( const LuaWorkbenchRow& row : g_luaWorkbenchRows )
	{
		const bool visible = filter.isEmpty()
			|| LuaWorkbench_entrySearchText( *row.entry ).contains( filter, Qt::CaseInsensitive );
		if ( row.container != nullptr ) {
			row.container->setVisible( visible );
		}
		if ( visible && firstVisibleEntry == nullptr ) {
			firstVisibleEntry = row.entry;
		}
	}

	LuaWorkbench_refreshDetails( firstVisibleEntry );
}

void LuaWorkbench_openByLabel( const char* label, bool external = false ){
	for ( const LuaWorkbenchEntry& entry : c_luaWorkbenchEntries )
	{
		if ( string_equal( entry.label, label ) ) {
			LuaWorkbench_openScript( entry, external );
			LuaWorkbench_refreshDetails( &entry );
			return;
		}
	}
}

static QWidget* LuaWorkbench_makeQuickActionButtonRow( QWidget* parent ){
	auto* row = new QWidget( parent );
	auto* layout = new QGridLayout( row );
	layout->setContentsMargins( 0, 0, 0, 0 );
	layout->setHorizontalSpacing( 6 );
	layout->setVerticalSpacing( 6 );

	auto* capturePointButton = new QPushButton( "Capture Point Volume", row );
	auto* pveSpawnButton = new QPushButton( "PvE Spawn Point", row );
	auto* playerStartButton = new QPushButton( "Player Start", row );
	auto* splineButton = new QPushButton( "Spline Path", row );
	auto* objectivesButton = new QPushButton( "Open objectives.lua", row );
	auto* entitiesButton = new QPushButton( "Open entities.lua", row );

	layout->addWidget( capturePointButton, 0, 0 );
	layout->addWidget( pveSpawnButton, 0, 1 );
	layout->addWidget( playerStartButton, 1, 0 );
	layout->addWidget( splineButton, 1, 1 );
	layout->addWidget( objectivesButton, 2, 0 );
	layout->addWidget( entitiesButton, 2, 1 );

	QObject::connect( capturePointButton, &QPushButton::clicked, [](){
		Add_createEntity( "trigger_capture_point" );
		LuaWorkbench_setStatus( "Placed trigger_capture_point at the camera." );
	} );
	QObject::connect( pveSpawnButton, &QPushButton::clicked, [](){
		Add_createEntity( "info_pve_spawn" );
		LuaWorkbench_setStatus( "Placed info_pve_spawn at the camera." );
	} );
	QObject::connect( playerStartButton, &QPushButton::clicked, [](){
		Add_createEntity( "info_player_start" );
		LuaWorkbench_setStatus( "Placed info_player_start at the camera." );
	} );
	QObject::connect( splineButton, &QPushButton::clicked, [](){
		Add_createEntity( "misc_spline" );
		LuaWorkbench_setStatus( "Placed misc_spline at the camera." );
	} );
	QObject::connect( objectivesButton, &QPushButton::clicked, [](){ LuaWorkbench_openByLabel( "objectives.lua" ); } );
	QObject::connect( entitiesButton, &QPushButton::clicked, [](){ LuaWorkbench_openByLabel( "entities.lua" ); } );

	return row;
}

}

void LuaWorkbench_open(){
	if ( g_luaWorkbenchDock == nullptr ) {
		return;
	}
	g_luaWorkbenchDock->show();
	g_luaWorkbenchDock->raise();
}

void LuaWorkbench_createDock( QMainWindow* window ){
	if ( window == nullptr || g_luaWorkbenchDock != nullptr ) {
		return;
	}

	g_luaWorkbenchDock = new QDockWidget( "Lua Script Hub", window );
	g_luaWorkbenchDock->setObjectName( "dock_lua_workbench" );

	auto* root = new QWidget( g_luaWorkbenchDock );
	auto* layout = new QVBoxLayout( root );
	layout->setContentsMargins( 6, 6, 6, 6 );

	auto* intro = new QLabel(
		"Author gameplay scripts, place runtime entities, and jump into the core Lua files without leaving the editor.",
		root );
	intro->setWordWrap( true );
	layout->addWidget( intro );

	g_luaWorkbenchSearch = new QLineEdit( root );
	g_luaWorkbenchSearch->setPlaceholderText( "Search scripts, entities.lua, objectives, spawn, capture..." );
	layout->addWidget( g_luaWorkbenchSearch );

	auto* quickActionsLabel = new QLabel( "Quick Gameplay Authoring", root );
	layout->addWidget( quickActionsLabel );
	layout->addWidget( LuaWorkbench_makeQuickActionButtonRow( root ) );

	auto* scriptsLabel = new QLabel( "Scripts", root );
	layout->addWidget( scriptsLabel );

	g_luaWorkbenchRows.clear();
	for ( int i = 0; i < int( sizeof( c_luaWorkbenchEntries ) / sizeof( c_luaWorkbenchEntries[0] ) ); ++i )
	{
		const LuaWorkbenchEntry& entry = c_luaWorkbenchEntries[i];
		auto* scriptCard = new QFrame( root );
		scriptCard->setFrameShape( QFrame::StyledPanel );
		auto* cardLayout = new QGridLayout( scriptCard );
		auto* nameLabel = new QLabel( StringStream( "<b>", entry.label, "</b><br/>", entry.summary ).c_str(), scriptCard );
		nameLabel->setTextFormat( Qt::RichText );
		nameLabel->setWordWrap( true );
		auto* pathLabel = new QLabel( LuaWorkbench_displayPath( *entry.path ), scriptCard );
		pathLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
		pathLabel->setWordWrap( true );
		auto* internalButton = new QPushButton( "Open", scriptCard );
		auto* externalButton = new QPushButton( "External", scriptCard );
		auto* actionRow = new QWidget( scriptCard );
		auto* actionLayout = new QHBoxLayout( actionRow );
		actionLayout->setContentsMargins( 0, 0, 0, 0 );
		actionLayout->addWidget( internalButton );
		actionLayout->addWidget( externalButton );
		actionLayout->addStretch();

		cardLayout->addWidget( nameLabel, 0, 0 );
		cardLayout->addWidget( pathLabel, 1, 0 );
		cardLayout->addWidget( actionRow, 2, 0 );
		layout->addWidget( scriptCard );
		g_luaWorkbenchRows.push_back( { scriptCard, pathLabel, &entry } );

		QObject::connect( internalButton, &QPushButton::clicked, [entry](){
			LuaWorkbench_openScript( entry, false );
			LuaWorkbench_refreshDetails( &entry );
		} );
		QObject::connect( externalButton, &QPushButton::clicked, [entry](){
			LuaWorkbench_openScript( entry, true );
			LuaWorkbench_refreshDetails( &entry );
		} );
	}

	auto* detailsLabel = new QLabel( "Selection Details", root );
	layout->addWidget( detailsLabel );
	g_luaWorkbenchDetails = new QTextBrowser( root );
	g_luaWorkbenchDetails->setMinimumHeight( 150 );
	layout->addWidget( g_luaWorkbenchDetails );

	auto* utilityRow = new QHBoxLayout();
	auto* hubButton = new QPushButton( "Open Objectives", root );
	auto* mainButton = new QPushButton( "Open Main", root );
	auto* defsButton = new QPushButton( "Reload Entity Definitions", root );
	utilityRow->addWidget( hubButton );
	utilityRow->addWidget( mainButton );
	utilityRow->addWidget( defsButton );
	utilityRow->addStretch();
	layout->addLayout( utilityRow );
	QObject::connect( hubButton, &QPushButton::clicked, Lua_editObjectives );
	QObject::connect( mainButton, &QPushButton::clicked, [](){ LuaWorkbench_openByLabel( "main.lua" ); } );
	QObject::connect( defsButton, &QPushButton::clicked, [](){ GlobalCommands_find( "EntityReloadDefinitions" ).m_callback(); } );
	QObject::connect( g_luaWorkbenchSearch, &QLineEdit::textChanged, []( const QString& ){ LuaWorkbench_refreshFilter(); } );

	g_luaWorkbenchStatus = new QLabel( "Ready", root );
	layout->addWidget( g_luaWorkbenchStatus );
	LuaWorkbench_refreshDetails();

	g_luaWorkbenchDock->setWidget( root );
	window->addDockWidget( Qt::BottomDockWidgetArea, g_luaWorkbenchDock );
	g_luaWorkbenchDock->hide();
}

void LuaWorkbench_stopAndRelease(){
	g_luaWorkbenchDock = nullptr;
	g_luaWorkbenchStatus = nullptr;
	g_luaWorkbenchSearch = nullptr;
	g_luaWorkbenchDetails = nullptr;
	g_luaWorkbenchRows.clear();
}
