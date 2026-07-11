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

namespace
{

QDockWidget* g_luaWorkbenchDock{};
QLabel* g_luaWorkbenchStatus{};

struct LuaWorkbenchEntry
{
	const char* label;
	const CopiedString* path;
	const char* title;
};

const LuaWorkbenchEntry c_luaWorkbenchEntries[] = {
	{ "props.lua", &g_luaScriptProps, "Lua Props" },
	{ "entities.lua", &g_luaScriptEntities, "Lua Entities" },
	{ "items.lua", &g_luaScriptItems, "Lua Items" },
	{ "main.lua", &g_luaScriptMain, "Lua Main" },
	{ "objectives.lua", &g_luaScriptObjectives, "Lua Objectives" },
};

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
		"Open the key gameplay Lua files directly in the built-in editor or your external editor.",
		root );
	intro->setWordWrap( true );
	layout->addWidget( intro );

	auto* grid = new QGridLayout();
	grid->addWidget( new QLabel( "Script", root ), 0, 0 );
	grid->addWidget( new QLabel( "Path", root ), 0, 1 );
	grid->addWidget( new QLabel( "Actions", root ), 0, 2 );

	for ( int i = 0; i < int( sizeof( c_luaWorkbenchEntries ) / sizeof( c_luaWorkbenchEntries[0] ) ); ++i )
	{
		const LuaWorkbenchEntry& entry = c_luaWorkbenchEntries[i];
		auto* nameLabel = new QLabel( entry.label, root );
		auto* pathLabel = new QLabel( LuaWorkbench_displayPath( *entry.path ), root );
		pathLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
		auto* internalButton = new QPushButton( "Open", root );
		auto* externalButton = new QPushButton( "External", root );
		auto* actionRow = new QWidget( root );
		auto* actionLayout = new QHBoxLayout( actionRow );
		actionLayout->setContentsMargins( 0, 0, 0, 0 );
		actionLayout->addWidget( internalButton );
		actionLayout->addWidget( externalButton );
		actionLayout->addStretch();

		grid->addWidget( nameLabel, i + 1, 0 );
		grid->addWidget( pathLabel, i + 1, 1 );
		grid->addWidget( actionRow, i + 1, 2 );

		QObject::connect( internalButton, &QPushButton::clicked, [entry](){ LuaWorkbench_openScript( entry, false ); } );
		QObject::connect( externalButton, &QPushButton::clicked, [entry](){ LuaWorkbench_openScript( entry, true ); } );
	}

	layout->addLayout( grid );

	auto* utilityRow = new QHBoxLayout();
	auto* hubButton = new QPushButton( "Open Objectives", root );
	auto* defsButton = new QPushButton( "Reload Entity Definitions", root );
	utilityRow->addWidget( hubButton );
	utilityRow->addWidget( defsButton );
	utilityRow->addStretch();
	layout->addLayout( utilityRow );
	QObject::connect( hubButton, &QPushButton::clicked, Lua_editObjectives );
	QObject::connect( defsButton, &QPushButton::clicked, [](){ GlobalCommands_find( "EntityReloadDefinitions" ).m_callback(); } );

	g_luaWorkbenchStatus = new QLabel( "Ready", root );
	layout->addWidget( g_luaWorkbenchStatus );

	g_luaWorkbenchDock->setWidget( root );
	window->addDockWidget( Qt::BottomDockWidgetArea, g_luaWorkbenchDock );
	g_luaWorkbenchDock->hide();
}

void LuaWorkbench_stopAndRelease(){
	g_luaWorkbenchDock = nullptr;
	g_luaWorkbenchStatus = nullptr;
}
