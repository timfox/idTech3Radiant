/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

//
// Main Window for Q3Radiant
//
// Leonardo Zide (leo@lokigames.com)
//

#include "mainframe.h"
#include "mainframe_commands.h"
#include "generic/callback.h"

#include "debugging/debugging.h"
#include "version.h"

#include "ifilesystem.h"
#include "ibrush.h"
#include "ientity.h"
#include "ipatch.h"
#include "iselection.h"
#include "ishaders.h"
#include "ieclass.h"
#include "irender.h"
#include "igl.h"
#include "moduleobserver.h"

#include <cstdio>
#include <ctime>

#include <QWidget>
#include <QSplashScreen>
#include <QCoreApplication>
#include <QTimer>
#include <QMainWindow>
#include <QLabel>
#include <QSplitter>
#include <QMenuBar>
#include <QWidgetAction>
#include <QApplication>
#include <QCheckBox>
#include <QToolBar>
#include <QStatusBar>
#include <QProgressBar>
#include <QDateTime>
#include <QBoxLayout>
#include <QDialog>
#include <QCloseEvent>
#include <QSettings>
#include <QDockWidget>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTabWidget>
#include <QTextBrowser>
#include <QSlider>
#include <QTreeWidget>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QTableWidget>
#include <QHeaderView>
#include <QProcess>
#include <QUrl>
#include <QClipboard>
#include <QCryptographicHash>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QHostAddress>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QList>
#include <QSpinBox>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVector>
#include <QComboBox>
#include <QWindow>

#if QT_CONFIG(vulkan)
#include <QVulkanInstance>
#include <QVulkanFunctions>
#include <QVulkanWindow>
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>

#include "commandlib.h"
#include "scenelib.h"
#include "stream/stringstream.h"
#include "signal/isignal.h"
#include "os/path.h"
#include "os/file.h"
#include <glib.h>
#include "moduleobservers.h"

#include "gtkutil/glfont.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/image.h"
#include "gtkutil/spinbox.h"
#include "math/aabb.h"
#include "math/matrix.h"
#include "scenelib.h"
#include "stringio.h"
#include "gtkutil/menu.h"
#include "gtkutil/guisettings.h"

#include "autosave.h"
#include "build.h"
#include "brushmanip.h"
#include "brushnode.h"
#include "camwindow.h"
#include "csg.h"
#include "commands.h"
#include "console.h"
#include "entity.h"
#include "entityinspector.h"
#include "eclasslib.h"
#include "gtkutil/image.h"
#include "entityinspector.h"
#include "entitylist.h"
#include "filters.h"
#include "findtexturedialog.h"
#include "grid.h"
#include "groupdialog.h"
#include "qtdlgs.h"
#include "qtmisc.h"
#include "help.h"
#include "layers.h"
#include "map.h"
#include "mru.h"
#include "patch.h"
#include "patchmanip.h"
#include "plugin.h"
#include "pluginmanager.h"
#include "pluginmenu.h"
#include "plugintoolbar.h"
#include "preferences.h"
#include "qe3.h"
#include "qgl.h"
#include "select.h"
#include "selection.h"
#include "server.h"
#include "surfacedialog.h"
#include "trayicon.h"
#include "textures.h"
#include "texwindow.h"
#include "modelwindow.h"
#include "layerswindow.h"
#include "url.h"
#include "xywindow.h"
#include "windowobservers.h"
#include "renderstate.h"
#include "feedback.h"
#include "referencecache.h"
#include "iundo.h"
#include "audio_workbench.h"
#include "video_workbench.h"
#include "spreadsheet_workbench.h"
#include "python_script_workbench.h"
#include "scenegraphinspector.h"
#include "ai_assistant.h"

#include "colors.h"
#include "tools.h"
#include "filterbar.h"

void GamePacksPath_importString( const char* value );
void GamePacksPath_exportString( const StringImportCallback& importer );


// VFS
class VFSModuleObserver : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	VFSModuleObserver() : m_unrealised( 1 ){
	}
	void realise() override {
		if ( --m_unrealised == 0 ) {
			QE_InitVFS();
			GlobalFileSystem().initialise();
		}
	}
	void unrealise() override {
		if ( ++m_unrealised == 1 ) {
			GlobalFileSystem().shutdown();
		}
	}
};

VFSModuleObserver g_VFSModuleObserver;

void VFS_Construct(){
	Radiant_attachHomePathsObserver( g_VFSModuleObserver );
}
void VFS_Destroy(){
	Radiant_detachHomePathsObserver( g_VFSModuleObserver );
}

// Home Paths

#ifdef WIN32
#include <shlobj.h>
#include <objbase.h>
const GUID qFOLDERID_SavedGames = {0x4C5C32FF, 0xBB9D, 0x43b0, {0xB5, 0xB4, 0x2D, 0x72, 0xE5, 0x4E, 0xAA, 0xA4}};
#define qREFKNOWNFOLDERID GUID
#define qKF_FLAG_CREATE 0x8000
#define qKF_FLAG_NO_ALIAS 0x1000
typedef HRESULT ( WINAPI qSHGetKnownFolderPath_t )( qREFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR *ppszPath );
static qSHGetKnownFolderPath_t *qSHGetKnownFolderPath;
#endif
void HomePaths_Realise(){
	do
	{
		const char* prefix = g_pGameDescription->getKeyValue( "prefix" );
		if ( !string_empty( prefix ) ) {
			StringOutputStream path( 256 );

#if defined( __APPLE__ )
			path( DirectoryCleaned( g_get_home_dir() ), "Library/Application Support", ( prefix + 1 ), '/' );
			if ( file_is_directory( path ) ) {
				g_qeglobals.m_userEnginePath = path;
				break;
			}
			path( DirectoryCleaned( g_get_home_dir() ), prefix, '/' );
#endif

#if defined( WIN32 )
			TCHAR mydocsdir[MAX_PATH + 1];
			wchar_t *mydocsdirw;
			HMODULE shfolder = LoadLibrary( "shfolder.dll" );
			if ( shfolder ) {
				qSHGetKnownFolderPath = (qSHGetKnownFolderPath_t *) GetProcAddress( shfolder, "SHGetKnownFolderPath" );
			}
			else{
				qSHGetKnownFolderPath = nullptr;
			}
			CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED );
			if ( qSHGetKnownFolderPath && qSHGetKnownFolderPath( qFOLDERID_SavedGames, qKF_FLAG_CREATE | qKF_FLAG_NO_ALIAS, nullptr, &mydocsdirw ) == S_OK ) {
				memset( mydocsdir, 0, sizeof( mydocsdir ) );
				wcstombs( mydocsdir, mydocsdirw, sizeof( mydocsdir ) - 1 );
				CoTaskMemFree( mydocsdirw );
				path( DirectoryCleaned( mydocsdir ), ( prefix + 1 ), '/' );
				if ( file_is_directory( path ) ) {
					g_qeglobals.m_userEnginePath = path;
					CoUninitialize();
					FreeLibrary( shfolder );
					break;
				}
			}
			CoUninitialize();
			if ( shfolder ) {
				FreeLibrary( shfolder );
			}
			if ( SUCCEEDED( SHGetFolderPath( nullptr, CSIDL_PERSONAL, nullptr, 0, mydocsdir ) ) ) {
				path( DirectoryCleaned( mydocsdir ), "My Games/", ( prefix + 1 ), '/' );
				// win32: only add it if it already exists
				if ( file_is_directory( path ) ) {
					g_qeglobals.m_userEnginePath = path;
					break;
				}
			}
#endif

#if defined( POSIX )
			path( DirectoryCleaned( g_get_home_dir() ), prefix, '/' );
			g_qeglobals.m_userEnginePath = path;
			break;
#endif
		}

		g_qeglobals.m_userEnginePath = EnginePath_get();
	}
	while ( false );

	Q_mkdir( g_qeglobals.m_userEnginePath.c_str() );

	g_qeglobals.m_userGamePath = StringStream( g_qeglobals.m_userEnginePath, gamename_get(), '/' );
	ASSERT_MESSAGE( !g_qeglobals.m_userGamePath.empty(), "HomePaths_Realise: user-game-path is empty" );
	Q_mkdir( g_qeglobals.m_userGamePath.c_str() );
}

ModuleObservers g_homePathObservers;

void Radiant_attachHomePathsObserver( ModuleObserver& observer ){
	g_homePathObservers.attach( observer );
}

void Radiant_detachHomePathsObserver( ModuleObserver& observer ){
	g_homePathObservers.detach( observer );
}

class HomePathsModuleObserver : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	HomePathsModuleObserver() : m_unrealised( 1 ){
	}
	void realise() override {
		if ( --m_unrealised == 0 ) {
			HomePaths_Realise();
			g_homePathObservers.realise();
		}
	}
	void unrealise() override {
		if ( ++m_unrealised == 1 ) {
			g_homePathObservers.unrealise();
		}
	}
};

HomePathsModuleObserver g_HomePathsModuleObserver;

void HomePaths_Construct(){
	Radiant_attachEnginePathObserver( g_HomePathsModuleObserver );
}
void HomePaths_Destroy(){
	Radiant_detachEnginePathObserver( g_HomePathsModuleObserver );
}


// Engine Path

CopiedString g_strEnginePath;
ModuleObservers g_enginePathObservers;
std::size_t g_enginepath_unrealised = 1;

void Radiant_attachEnginePathObserver( ModuleObserver& observer ){
	g_enginePathObservers.attach( observer );
}

void Radiant_detachEnginePathObserver( ModuleObserver& observer ){
	g_enginePathObservers.detach( observer );
}


void EnginePath_Realise(){
	if ( --g_enginepath_unrealised == 0 ) {
		g_enginePathObservers.realise();
	}
}


const char* EnginePath_get(){
	ASSERT_MESSAGE( g_enginepath_unrealised == 0, "EnginePath_get: engine path not realised" );
	return g_strEnginePath.c_str();
}

void EnginePath_Unrealise(){
	if ( ++g_enginepath_unrealised == 1 ) {
		g_enginePathObservers.unrealise();
	}
}

static CopiedString g_installedDevFilesPath; // track last engine path, where dev files installation occured, to prompt again when changed

static void installDevFiles(){
	if( !path_equal( g_strEnginePath.c_str(), g_installedDevFilesPath.c_str() ) ){
		ASSERT_MESSAGE( g_enginepath_unrealised != 0, "installDevFiles: engine path realised" );
		DoInstallDevFilesDlg( g_strEnginePath.c_str() );
		g_installedDevFilesPath = g_strEnginePath;
	}
}

void setEnginePath( CopiedString& self, const char* value ){
	const auto buffer = StringStream( DirectoryCleaned( value ) );
	if ( !path_equal( buffer, self.c_str() ) ) {
#if 0
		while ( !ConfirmModified( "Paths Changed" ) )
		{
			if ( Map_Unnamed( g_map ) ) {
				Map_SaveAs();
			}
			else
			{
				Map_Save();
			}
		}
		Map_RegionOff();
#endif

		ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Changing Engine Path" );

		EnginePath_Unrealise();

		self = buffer;

		installDevFiles();

		EnginePath_Realise();
	}
}
typedef ReferenceCaller<CopiedString, void(const char*), setEnginePath> EnginePathImportCaller;


// Extra Resource Path

std::array<CopiedString, 5> g_strExtraResourcePaths;

const std::array<CopiedString, 5>& ExtraResourcePaths_get(){
	return g_strExtraResourcePaths;
}


// App Path

CopiedString g_strAppPath;                 ///< holds the full path of the executable

const char* AppPath_get(){
	return g_strAppPath.c_str();
}

/// the path to the local rc-dir
const char* LocalRcPath_get(){
	static CopiedString rc_path;
	if ( rc_path.empty() ) {
		rc_path = StringStream( GlobalRadiant().getSettingsPath(), g_pGameDescription->mGameFile, '/' );
	}
	return rc_path.c_str();
}

/// directory for temp files
/// NOTE: on *nix this is were we check for .pid
CopiedString g_strSettingsPath;
const char* SettingsPath_get(){
	return g_strSettingsPath.c_str();
}

CopiedString g_strGamePacksPath;
const char* GamePacksPath_get(){
	return g_strGamePacksPath.c_str();
}

void GamePacksPath_setDefault(){
	g_strGamePacksPath = StringStream( g_strAppPath, "gamepacks/" );
}

void GamePacksPath_set( const char* path ){
	if ( string_empty( path ) ) {
		GamePacksPath_setDefault();
		return;
	}
	StringOutputStream stream( 256 );
	stream << DirectoryCleaned( path );
	g_strGamePacksPath = stream.c_str();
}


/*!
   points to the game tools directory, for instance
   C:/Program Files/Quake III Arena/GtkRadiant
   (or other games)
   this is one of the main variables that are configured by the game selection on startup
   [GameToolsPath]/plugins
   [GameToolsPath]/modules
   and also q3map, bspc
 */
CopiedString g_strGameToolsPath;           ///< this is set by g_GamesDialog

bool g_bMayaNavigation = true;
bool g_trayIconEnabled = true;
bool g_minimizeToTray = false;

const char* GameToolsPath_get(){
	return g_strGameToolsPath.c_str();
}


void Paths_constructPreferences( PreferencesPage& page ){
	page.appendPathEntry( "Engine Path", true,
	                      StringImportCallback( EnginePathImportCaller( g_strEnginePath ) ),
	                      StringExportCallback( StringExportCaller( g_strEnginePath ) )
	                    );
	page.appendPathEntry( "Gamepacks Path", true,
	                      StringImportCallback( FreeCaller<void(const char*), GamePacksPath_importString>() ),
	                      StringExportCallback( FreeCaller<void(const StringImportCallback&), GamePacksPath_exportString>() )
	                    );
}
void Paths_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Paths", "Path Settings" ) );
	Paths_constructPreferences( page );
	for( auto& extraPath : g_strExtraResourcePaths )
		page.appendPathEntry( "Extra Resource Path", true,
		                      StringImportCallback( EnginePathImportCaller( extraPath ) ),
		                      StringExportCallback( StringExportCaller( extraPath ) )
		                    );
}
void Paths_registerPreferencesPage(){
	PreferencesDialog_addGamePage( makeCallbackF( Paths_constructPage ) );
}


class PathsDialog : public Dialog
{
public:
	void BuildDialog() override {
		GetWidget()->setWindowTitle( "Engine Path Configuration" );

		auto *vbox = new QVBoxLayout( GetWidget() );
		{
			auto *frame = new QGroupBox( "Path settings" );
			vbox->addWidget( frame );

			auto *grid = new QGridLayout( frame );
			grid->setAlignment( Qt::AlignmentFlag::AlignTop );
			grid->setColumnStretch( 0, 111 );
			grid->setColumnStretch( 1, 333 );
			{
				const char* engine;
#if defined( WIN32 )
				engine = g_pGameDescription->getRequiredKeyValue( "engine_win32" );
#elif defined( __linux__ ) || defined ( __FreeBSD__ )
				engine = g_pGameDescription->getRequiredKeyValue( "engine_linux" );
#elif defined( __APPLE__ )
				engine = g_pGameDescription->getRequiredKeyValue( "engine_macos" );
#else
#error "unsupported platform"
#endif
				const auto text = StringStream( "Select directory, where game executable sits (typically ", Quoted( engine ), ")\n" );
				grid->addWidget( new QLabel( text.c_str() ), 0, 0, 1, 2 );
			}
			{
				PreferencesPage preferencesPage( *this, grid );
				Paths_constructPreferences( preferencesPage );
			}
		}
		{
			auto *buttons = new QDialogButtonBox( QDialogButtonBox::StandardButton::Ok );
			vbox->addWidget( buttons );
			QObject::connect( buttons, &QDialogButtonBox::accepted, GetWidget(), &QDialog::accept );
		}
	}
};

PathsDialog g_PathsDialog;

static bool g_strEnginePath_was_empty_1st_start = false;

void EnginePath_verify(){
	if ( !file_exists( g_strEnginePath.c_str() ) || g_strEnginePath_was_empty_1st_start ) {
		g_installedDevFilesPath = ""; // trigger install for non existing engine path case
		g_PathsDialog.Create( nullptr );
		g_PathsDialog.DoModal();
		g_PathsDialog.Destroy();
	}
	installDevFiles(); // try this anytime, as engine path may be set via command line or -gamedetect
}

namespace
{
CopiedString g_gamename;
CopiedString g_gamemode;
ModuleObservers g_gameNameObservers;
ModuleObservers g_gameModeObservers;
}

void Radiant_attachGameNameObserver( ModuleObserver& observer ){
	g_gameNameObservers.attach( observer );
}

void Radiant_detachGameNameObserver( ModuleObserver& observer ){
	g_gameNameObservers.detach( observer );
}

const char* basegame_get(){
	return g_pGameDescription->getRequiredKeyValue( "basegame" );
}

const char* gamename_get(){
	if ( g_gamename.empty() ) {
		return basegame_get();
	}
	return g_gamename.c_str();
}

void gamename_set( const char* gamename ){
	if ( !string_equal( gamename, g_gamename.c_str() ) ) {
		g_gameNameObservers.unrealise();
		g_gamename = gamename;
		g_gameNameObservers.realise();
	}
}

void Radiant_attachGameModeObserver( ModuleObserver& observer ){
	g_gameModeObservers.attach( observer );
}

void Radiant_detachGameModeObserver( ModuleObserver& observer ){
	g_gameModeObservers.detach( observer );
}

const char* gamemode_get(){
	return g_gamemode.c_str();
}

void gamemode_set( const char* gamemode ){
	if ( !string_equal( gamemode, g_gamemode.c_str() ) ) {
		g_gameModeObservers.unrealise();
		g_gamemode = gamemode;
		g_gameModeObservers.realise();
	}
}

#include "os/dir.h"

const char* const c_library_extension =
#if defined( WIN32 )
    "dll"
#elif defined ( __APPLE__ )
    "dylib"
#elif defined( __linux__ ) || defined ( __FreeBSD__ )
    "so"
#endif
    ;

static bool plugin_name_safe( const char* name ){
	for ( const char* p = name; *p; ++p ) {
		if ( *p == '.' && p[1] == '.' ) {
			return false; // reject path traversal
		}
		if ( *p == '/' || *p == '\\' ) {
			return false; // reject path separators
		}
	}
	return true;
}

void Radiant_loadModules( const char* path ){
	Directory_forEach( path, matchFileExtension( c_library_extension, [&]( const char *name ){
		if ( !plugin_name_safe( name ) ) {
			globalErrorStream() << "Skipping plugin with invalid name: " << name << '\n';
			return;
		}
		char fullname[1024];
		const int len = snprintf( fullname, sizeof( fullname ), "%s%s", path, name );
		ASSERT_MESSAGE( len >= 0 && static_cast<std::size_t>( len ) < sizeof( fullname ), "" );
		globalOutputStream() << "Found " << SingleQuoted( fullname ) << '\n';
		GlobalModuleServer_loadModule( fullname );
	}));
}

void Radiant_loadModulesFromRoot( const char* directory ){
	Radiant_loadModules( StringStream( directory, g_pluginsDir ) );

	if ( !string_equal( g_pluginsDir, g_modulesDir ) ) {
		Radiant_loadModules( StringStream( directory, g_modulesDir ) );
	}
}


class WorldspawnColourEntityClassObserver : public ModuleObserver
{
	std::size_t m_unrealised;
public:
	WorldspawnColourEntityClassObserver() : m_unrealised( 1 ){
	}
	void realise() override {
		if ( --m_unrealised == 0 ) {
			SetWorldspawnColour( g_xywindow_globals.color_brushes );
		}
	}
	void unrealise() override {
		if ( ++m_unrealised == 1 ) {
		}
	}
};

WorldspawnColourEntityClassObserver g_WorldspawnColourEntityClassObserver;


ModuleObservers g_gameToolsPathObservers;

void Radiant_attachGameToolsPathObserver( ModuleObserver& observer ){
	g_gameToolsPathObservers.attach( observer );
}

void Radiant_detachGameToolsPathObserver( ModuleObserver& observer ){
	g_gameToolsPathObservers.detach( observer );
}

void Radiant_Initialise(){
	GlobalModuleServer_Initialise();

	Radiant_loadModulesFromRoot( AppPath_get() );

	Preferences_Load();

	bool success = Radiant_Construct( GlobalModuleServer_get() );
	ASSERT_MESSAGE( success, "module system failed to initialise - see radiant.log for error messages" );

	g_gameToolsPathObservers.realise();
	g_gameModeObservers.realise();
	g_gameNameObservers.realise();
}

void Radiant_Shutdown(){
	g_gameNameObservers.unrealise();
	g_gameModeObservers.unrealise();
	g_gameToolsPathObservers.unrealise();

	if ( !g_preferences_globals.disable_ini ) {
		globalOutputStream() << "Start writing prefs\n";
		Preferences_Save();
		globalOutputStream() << "Done prefs\n";
	}

	Radiant_Destroy();

	GlobalModuleServer_Shutdown();
}

void Exit(){
	if ( ConfirmModified( "Exit Radiant" ) && Spreadsheet_requestClose() ) {
		QCoreApplication::quit();
	}
}

#include "environment.h"

#ifdef WIN32
#include <process.h>
#else
#include <spawn.h>
/* According to the Single Unix Specification, environ is not
 * in any system header, although unistd.h often declares it.
 */
extern char **environ;
#endif
void Radiant_Restart(){
	if( ConfirmModified( "Restart Radiant" ) && Spreadsheet_requestClose() ){
		const auto mapname = StringStream( Quoted( Map_Name( g_map ) ) );

		char *argv[] = { string_clone( environment_get_app_filepath() ),
	                     Map_Unnamed( g_map )? nullptr : string_clone( mapname ),
	                     nullptr };
#ifdef WIN32
		const int status = !_spawnv( P_NOWAIT, argv[0], argv );
#else
		const int status = posix_spawn( nullptr, argv[0], nullptr, nullptr, argv, environ );
#endif

		// quit if radiant successfully started
		if ( status == 0 ) {
			QCoreApplication::quit();
		}
	}
}


void Restart(){
	PluginsMenu_clear();
	PluginToolbar_clear();

	Radiant_Shutdown();
	Radiant_Initialise();

	PluginsMenu_populate();

	PluginToolbar_populate();
}


QVector<int> UpdateCheck_parseVersionParts( const QString& value ){
	QVector<int> parts;
	QRegularExpression digits( "(\\d+)" );
	auto match = digits.globalMatch( value );
	while ( match.hasNext() )
	{
		const auto token = match.next().captured( 1 );
		bool ok = false;
		const int number = token.toInt( &ok );
		if ( ok ) {
			parts.push_back( number );
		}
	}
	while ( !parts.isEmpty() && parts.back() == 0 )
	{
		parts.pop_back();
	}
	return parts;
}

int UpdateCheck_compareVersions( const QString& lhs, const QString& rhs, bool& comparable ){
	const auto left = UpdateCheck_parseVersionParts( lhs );
	const auto right = UpdateCheck_parseVersionParts( rhs );
	comparable = !left.isEmpty() && !right.isEmpty();
	if ( !comparable ) {
		return 0;
	}
	const int count = std::max( left.size(), right.size() );
	for ( int i = 0; i < count; ++i )
	{
		const int a = i < left.size() ? left[i] : 0;
		const int b = i < right.size() ? right[i] : 0;
		if ( a < b ) {
			return -1;
		}
		if ( a > b ) {
			return 1;
		}
	}
	return 0;
}

void OpenUpdateURL(){
	OpenURL( "https://github.com/timfox/idtech3radiant/releases" );
}

constexpr const char* c_idTech3WebsiteUrl = "https://idtech3.com";
constexpr const char* c_idTech3DocumentationUrl = "https://idtech3.com/documentation";
constexpr const char* c_idTech3LinksUrl = "https://idtech3.com/links";

static void CheckForUpdate_showResult( QByteArray payload, QString fetchError, int tryNext );

static void CheckForUpdate_tryFetch( int index ){
	const char* releaseApiUrl = "https://api.github.com/repos/timfox/idtech3radiant/releases?per_page=1";
	const QString acceptHeader = "Accept: application/vnd.github+json";
	const QString userAgentHeader = StringStream( "User-Agent: idtech3radiant/", RADIANT_VERSION ).c_str();
	struct FetchCommand { const char* executable; QStringList arguments; };
	const FetchCommand commands[] = {
		{ "wget", { "-qO-", "--header", acceptHeader, "--header", userAgentHeader, releaseApiUrl } },
		{ "curl", { "-fsSL", "-H", acceptHeader, "-H", userAgentHeader, releaseApiUrl } },
	};
	if ( index >= static_cast<int>( std::size( commands ) ) ) {
		CheckForUpdate_showResult( {}, "Neither wget nor curl could fetch release information.", index );
		return;
	}
	auto* process = new QProcess( MainFrame_getWindow() );
	const auto& command = commands[index];
	QObject::connect( process, QOverload<int, QProcess::ExitStatus>::of( &QProcess::finished ), [process, index]( int exitCode, QProcess::ExitStatus status ){
		QByteArray payload;
		QString fetchError;
		if ( status == QProcess::NormalExit && exitCode == 0 ) {
			payload = process->readAllStandardOutput();
		}
		else{
			const auto stderrText = process->readAllStandardError();
			fetchError = stderrText.isEmpty()
				? StringStream( process->program().toUtf8().constData(), " failed with exit code ", exitCode, "." ).c_str()
				: StringStream( process->program().toUtf8().constData(), " failed: ", stderrText.constData() ).c_str();
		}
		process->deleteLater();
		CheckForUpdate_showResult( payload, fetchError, index + 1 );
	} );
	process->start( command.executable, command.arguments );
	if ( !process->waitForStarted( 3000 ) ) {
		process->deleteLater();
		CheckForUpdate_tryFetch( index + 1 );
	}
}

static void CheckForUpdate_showResult( QByteArray payload, QString fetchError, int tryNext ){
	struct FetchCommand { const char* executable; QStringList arguments; };
	const FetchCommand commands[] = {
		{ "wget", { "-qO-", "dummy" } },
		{ "curl", { "-fsSL", "dummy" } },
	};
	(void)commands;

	if ( !payload.isEmpty() ) {
		const auto json = QJsonDocument::fromJson( payload );
		QJsonObject object;
		if ( json.isObject() ) {
			object = json.object();
		}
		else if ( json.isArray() ) {
			const auto releases = json.array();
			if ( !releases.isEmpty() && releases.first().isObject() ) {
				object = releases.first().toObject();
			}
			else{
				if ( QMessageBox::information( MainFrame_getWindow(), "Check for Update",
				                               "No published GitHub releases were found for this project yet.\n\nOpen the releases page?",
				                               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes ) == QMessageBox::Yes ) {
					OpenUpdateURL();
				}
				return;
			}
		}
		else{
			if ( QMessageBox::question( MainFrame_getWindow(), "Check for Update",
			                            "Could not parse update response.\nOpen the releases page instead?",
			                            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes ) == QMessageBox::Yes ) {
				OpenUpdateURL();
			}
			return;
		}
		const QString latestTag = object.value( "tag_name" ).toString();
		const QString latestName = object.value( "name" ).toString( latestTag );
		const QString latestUrl = object.value( "html_url" ).toString( "https://github.com/timfox/idtech3radiant/releases" );
		const QString publishedAt = object.value( "published_at" ).toString();
		const QString currentVersion = RADIANT_VERSION;
		bool comparable = false;
		const int comparison = UpdateCheck_compareVersions( latestTag, currentVersion, comparable );
		if ( comparable && comparison > 0 ) {
			const auto message = StringStream(
			    "Update available.\n\nCurrent: ", currentVersion.toUtf8().constData(),
			    "\nLatest: ", latestTag.toUtf8().constData(),
			    latestName.isEmpty() ? "" : StringStream( " (", latestName.toUtf8().constData(), ")" ).c_str(),
			    publishedAt.isEmpty() ? "" : StringStream( "\nPublished: ", publishedAt.toUtf8().constData() ).c_str(),
			    "\n\nOpen release page?"
			);
			if ( QMessageBox::question( MainFrame_getWindow(), "Check for Update", message.c_str(),
			                            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes ) == QMessageBox::Yes ) {
				OpenURL( latestUrl.toUtf8().constData() );
			}
		}
		else{
			const auto summary = comparable
				? StringStream( "You are up to date.\n\nCurrent: ", currentVersion.toUtf8().constData(),
				                "\nLatest: ", latestTag.toUtf8().constData() ).c_str()
				: StringStream( "Latest release: ", latestTag.toUtf8().constData(),
				                "\nCurrent build version: ", currentVersion.toUtf8().constData() ).c_str();
			QMessageBox::information( MainFrame_getWindow(), "Check for Update", summary );
		}
		return;
	}

	if ( tryNext < static_cast<int>( std::size( commands ) ) ) {
		CheckForUpdate_tryFetch( tryNext );
		return;
	}

	if ( fetchError.isEmpty() ) {
		fetchError = "Neither wget nor curl could fetch release information.";
	}
	const auto message = StringStream( "Update check failed:\n", fetchError.toUtf8().constData(), "\n\nOpen the releases page instead?" );
	if ( QMessageBox::question( MainFrame_getWindow(), "Check for Update", message.c_str(),
	                            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes ) == QMessageBox::Yes ) {
		OpenUpdateURL();
	}
}

void CheckForUpdate(){
	CheckForUpdate_tryFetch( 0 );
}

// open the Q3Rad manual
void OpenHelpURL(){
	OpenURL( c_idTech3DocumentationUrl );
}

void OpenBugReportURL(){
	// OpenURL( "http://www.icculus.org/netradiant/?cmd=bugs" );
	OpenURL( "https://github.com/timfox/idtech3radiant/issues" );
}


QWidget* g_page_console{};
QDockWidget* g_consoleDock{};

void Console_ToggleShow(){
	if ( g_consoleDock != nullptr ) {
		const bool shouldShow = !g_consoleDock->isVisible();
		g_consoleDock->setVisible( shouldShow );
		if ( shouldShow ) {
			g_consoleDock->raise();
		}
		return;
	}

	if ( g_page_console != nullptr ) {
		GroupDialog_showPage( g_page_console );
	}
}

QWidget* g_page_entity;

void EntityInspector_ToggleShow(){
	GroupDialog_showPage( g_page_entity );
}

QWidget* g_page_models;

void ModelBrowser_ToggleShow(){
	GroupDialog_showPage( g_page_models );
}

QWidget* g_page_layers;

void LayersBrowser_ToggleShow(){
	GroupDialog_showPage( g_page_layers);
}


static class EverySecondTimer
{
	QTimer m_timer;
public:
	EverySecondTimer(){
		m_timer.setInterval( 1000 );
		m_timer.callOnTimeout( [](){
			if ( QGuiApplication::mouseButtons().testFlag( Qt::MouseButton::NoButton ) ) {
				QE_CheckAutoSave();
			}
		} );
	}
	void enable(){
		m_timer.start();
	}
	void disable(){
		m_timer.stop();
	}
}
s_qe_every_second_timer;


class WaitDialog
{
public:
	QWidget* m_window;
	QLabel* m_label;
};

WaitDialog create_wait_dialog( const char* title, const char* text ){
	/* Qt::Tool window type doesn't steal focus, which saves e.g. from losing freelook camera mode on autosave
	   or entity menu from hiding, while clicked with ctrl, by tex/model loading popup.
	   Qt::WidgetAttribute::WA_ShowWithoutActivating is implied, but lets have it set too. */
	auto *window = new QWidget( MainFrame_getWindow(), Qt::Tool | Qt::WindowTitleHint );
	window->setWindowTitle( title );
	window->setWindowModality( Qt::WindowModality::ApplicationModal );
	window->setAttribute( Qt::WidgetAttribute::WA_ShowWithoutActivating );

	auto *label = new QLabel( text );
	{
		auto *box = new QHBoxLayout( window );
		box->setSizeConstraint( QLayout::SizeConstraint::SetFixedSize );
		box->setContentsMargins( 20, 5, 20, 3 );
		box->addWidget( label );
		label->setMinimumWidth( 200 );
	}
	return WaitDialog{ .m_window = window, .m_label = label };
}

namespace
{
clock_t g_lastRedrawTime = 0;
const clock_t c_redrawInterval = clock_t( CLOCKS_PER_SEC / 10 );

bool redrawRequired(){
	clock_t currentTime = std::clock();
	if ( currentTime - g_lastRedrawTime >= c_redrawInterval ) {
		g_lastRedrawTime = currentTime;
		return true;
	}
	return false;
}
}

typedef std::list<CopiedString> StringStack;
StringStack g_wait_stack;
WaitDialog g_wait;

bool ScreenUpdates_Enabled(){
	return g_wait_stack.empty();
}

void ScreenUpdates_process(){
	if ( redrawRequired() ) {
		process_gui();
	}
}


void ScreenUpdates_Disable( const char* message, const char* title ){
	if ( g_pParentWnd == nullptr ) {
		g_wait_stack.push_back( message );
		return;
	}
	if ( g_wait_stack.empty() ) {
		s_qe_every_second_timer.disable();

		process_gui();

		g_wait = create_wait_dialog( title, message );

		g_wait.m_window->show();
		ScreenUpdates_process();
	}
	else {
		g_wait.m_window->setWindowTitle( title );
		g_wait.m_label->setText( message );
		ScreenUpdates_process();
	}
	g_wait_stack.push_back( message );
}

void ScreenUpdates_Enable(){
	ASSERT_MESSAGE( !ScreenUpdates_Enabled(), "screen updates already enabled" );
	g_wait_stack.pop_back();
	if ( g_pParentWnd == nullptr ) return;
	if ( g_wait_stack.empty() ) {
		s_qe_every_second_timer.enable();

		delete std::exchange( g_wait.m_window, nullptr );
	}
	else {
		g_wait.m_label->setText( g_wait_stack.back().c_str() );
		ScreenUpdates_process();
	}
}



void GlobalCamera_UpdateWindow(){
	if ( g_pParentWnd != 0 ) {
		CamWnd_Update( *g_pParentWnd->GetCamWnd() );
	}
}

void XY_UpdateAllWindows(){
	if ( g_pParentWnd != 0 ) {
		g_pParentWnd->forEachXYWnd( []( XYWnd* xywnd ){
			XYWnd_Update( *xywnd );
		} );
	}
}

void UpdateAllWindows(){
	GlobalCamera_UpdateWindow();
	XY_UpdateAllWindows();
}


LatchedInt g_Layout_viewStyle( 0, "Window Layout" );
LatchedBool g_Layout_enableDetachableMenus( true, "Detachable Menus" );
LatchedBool g_Layout_builtInGroupDialog( false, "Built-In Group Dialog" );
LatchedBool g_Layout_experimentalFeatures( false, "Experimental Features" );
bool Layout_experimentalFeaturesEnabled(){
	return g_Layout_experimentalFeatures.m_value;
}

Vector3 Add_entitySpawnOrigin();
extern Vector3 g_cameraBookmarks_origin[5];
extern Vector3 g_cameraBookmarks_angles[5];
extern bool g_cameraBookmarks_valid[5];

namespace
{
QDockWidget* g_exp_propertiesDock{};
QDockWidget* g_exp_previewDock{};
QDockWidget* g_exp_assetsDock{};
QDockWidget* g_exp_historyDock{};
QDockWidget* g_exp_usdDock{};
QDockWidget* g_exp_ecsDock{};
QDockWidget* g_exp_syncDock{};
QComboBox* g_exp_ecsCategoryCombo{};
QListWidget* g_exp_ecsEntityList{};
QLabel* g_exp_selectedCountLabel{};
QLabel* g_exp_selectedComponentsLabel{};
QLabel* g_exp_selectionTypeLabel{};
QLabel* g_exp_selectionBoundsLabel{};
QLabel* g_exp_selectionShaderLabel{};
QGroupBox* g_exp_entityGroup{};
QLabel* g_exp_entityClassLabel{};
QLineEdit* g_exp_entityNameEdit{};
QLineEdit* g_exp_entityTargetnameEdit{};
QLineEdit* g_exp_entityTargetEdit{};
QLineEdit* g_exp_entitySoundEdit{};
QLineEdit* g_exp_entityModelEdit{};
QGroupBox* g_exp_worldGroup{};
QLineEdit* g_exp_worldMessageEdit{};
QLineEdit* g_exp_worldMusicEdit{};
QLineEdit* g_exp_worldGravityEdit{};
QLineEdit* g_exp_worldColorEdit{};
QDoubleSpinBox* g_exp_worldMinlight{};
QGroupBox* g_exp_gravityGroup{};
QLineEdit* g_exp_gravityDirectionEdit{};
QLineEdit* g_exp_gravityMagnitudeEdit{};
QLineEdit* g_exp_shaderEdit{};
QLineEdit* g_exp_skyboxHDREdit{};
QLineEdit* g_exp_pbrAlbedo{};
QLineEdit* g_exp_pbrNormal{};
QDoubleSpinBox* g_exp_pbrRoughness{};
QDoubleSpinBox* g_exp_pbrMetallic{};
QLineEdit* g_exp_pbrAO{};
QDoubleSpinBox* g_exp_locX{};
QDoubleSpinBox* g_exp_locY{};
QDoubleSpinBox* g_exp_locZ{};
QDoubleSpinBox* g_exp_rotX{};
QDoubleSpinBox* g_exp_rotY{};
QDoubleSpinBox* g_exp_rotZ{};
QDoubleSpinBox* g_exp_scaleX{};
QDoubleSpinBox* g_exp_scaleY{};
QDoubleSpinBox* g_exp_scaleZ{};
QCheckBox* g_exp_uniformScale{};
QListWidget* g_exp_assetsList{};
QListWidget* g_exp_historyList{};
QTreeWidget* g_exp_usdTree{};
QLabel* g_exp_syncStateLabel{};
QLabel* g_exp_syncClientsLabel{};
QLabel* g_exp_syncRuntimeLabel{};
QSpinBox* g_exp_syncPort{};
QCheckBox* g_exp_syncAutoStart{};
QCheckBox* g_exp_syncAutoSync{};
QListWidget* g_exp_syncLog{};
std::size_t g_exp_historyCounter{};
bool g_exp_undoTrackerAttached{};
QString g_exp_activePreviewBackend = "OpenGL";
QString g_exp_lastRuntimeEvent = "No runtime messages yet";

class ExperimentalPreviewHostWidget;
class ExperimentalLiveSyncService;
ExperimentalPreviewHostWidget* g_exp_previewHost{};
ExperimentalLiveSyncService* g_exp_liveSyncService{};

static void Experimental_refreshLiveSyncUi();
static void Experimental_appendLiveSyncLog( const QString& message );
static void Experimental_liveSyncPreviewBackendChanged();
static QJsonArray Experimental_buildCameraBookmarksArray();
static void Experimental_restoreCameraBookmarks( const QJsonValue& value );

class ExperimentalPreviewWidget final : public QOpenGLWidget, protected QOpenGLFunctions
{
	void initializeGL() override {
		initializeOpenGLFunctions();
	}
	void paintGL() override {
		glClearColor( 0.14f, 0.14f, 0.16f, 1.0f );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}
};

static QString Experimental_normalisePreviewBackend( QString backend ){
	backend = backend.trimmed();
	return backend.compare( "Vulkan", Qt::CaseInsensitive ) == 0 ? "Vulkan" : "OpenGL";
}

static QString Experimental_requestedPreviewBackend(){
	return Experimental_normalisePreviewBackend( QSettings().value( "Properties/Experimental/PreviewBackend", "OpenGL" ).toString() );
}

static void Experimental_storeRequestedPreviewBackend( const QString& backend ){
	QSettings().setValue( "Properties/Experimental/PreviewBackend", Experimental_normalisePreviewBackend( backend ) );
}

#if QT_CONFIG(vulkan)
class ExperimentalVulkanClearRenderer final : public QVulkanWindowRenderer
{
	QPointer<QVulkanWindow> m_window;
	QVulkanDeviceFunctions* m_deviceFunctions = nullptr;
public:
	explicit ExperimentalVulkanClearRenderer( QVulkanWindow* window ) : m_window( window ){
	}

	void initResources() override {
		if ( m_window == nullptr || m_window->vulkanInstance() == nullptr ) {
			return;
		}
		m_deviceFunctions = m_window->vulkanInstance()->deviceFunctions( m_window->device() );
	}

	void startNextFrame() override {
		if ( m_window == nullptr || m_deviceFunctions == nullptr ) {
			return;
		}

		QVector<VkClearValue> clearValues;
		clearValues.resize( m_window->depthStencilFormat() == VK_FORMAT_UNDEFINED ? 1 : 2 );
		clearValues[0].color.float32[0] = 0.05f;
		clearValues[0].color.float32[1] = 0.07f;
		clearValues[0].color.float32[2] = 0.09f;
		clearValues[0].color.float32[3] = 1.0f;
		if ( clearValues.size() > 1 ) {
			clearValues[1].depthStencil.depth = 1.0f;
			clearValues[1].depthStencil.stencil = 0;
		}

		VkRenderPassBeginInfo rpBeginInfo = {};
		rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpBeginInfo.renderPass = m_window->defaultRenderPass();
		rpBeginInfo.framebuffer = m_window->currentFramebuffer();
		rpBeginInfo.renderArea.extent.width = static_cast<uint32_t>( m_window->swapChainImageSize().width() );
		rpBeginInfo.renderArea.extent.height = static_cast<uint32_t>( m_window->swapChainImageSize().height() );
		rpBeginInfo.clearValueCount = static_cast<uint32_t>( clearValues.size() );
		rpBeginInfo.pClearValues = clearValues.constData();

		const VkCommandBuffer commandBuffer = m_window->currentCommandBuffer();
		m_deviceFunctions->vkCmdBeginRenderPass( commandBuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE );
		m_deviceFunctions->vkCmdEndRenderPass( commandBuffer );
		m_window->frameReady();
	}
};

class ExperimentalVulkanWindow final : public QVulkanWindow
{
public:
	QVulkanWindowRenderer* createRenderer() override {
		return new ExperimentalVulkanClearRenderer( this );
	}
};
#endif

class ExperimentalPreviewHostWidget final : public QWidget
{
	QComboBox* m_backendCombo = nullptr;
	QLabel* m_statusLabel = nullptr;
	QVBoxLayout* m_surfaceLayout = nullptr;
	QWidget* m_surfaceWidget = nullptr;
#if QT_CONFIG(vulkan)
	QVulkanInstance* m_vulkanInstance = nullptr;
	QVulkanWindow* m_vulkanWindow = nullptr;
#endif
public:
	explicit ExperimentalPreviewHostWidget( QWidget* parent = nullptr ) : QWidget( parent ){
		auto* vbox = new QVBoxLayout( this );
		auto* form = new QFormLayout;
		m_backendCombo = new QComboBox( this );
		m_backendCombo->addItems( QStringList() << "OpenGL" << "Vulkan" );
		m_statusLabel = new QLabel( this );
		m_statusLabel->setWordWrap( true );
		form->addRow( "Backend", m_backendCombo );
		form->addRow( "Status", m_statusLabel );
		vbox->addLayout( form );

		auto* surfaceHost = new QWidget( this );
		m_surfaceLayout = new QVBoxLayout( surfaceHost );
		m_surfaceLayout->setContentsMargins( 0, 0, 0, 0 );
		vbox->addWidget( surfaceHost, 1 );

		m_backendCombo->setCurrentText( Experimental_requestedPreviewBackend() );
		QObject::connect( m_backendCombo, &QComboBox::currentTextChanged, [this]( const QString& backend ){
			rebuildPreview( backend );
		} );

		rebuildPreview( m_backendCombo->currentText() );
	}

	void setRequestedBackend( const QString& backend ){
		const QString normalised = Experimental_normalisePreviewBackend( backend );
		if ( m_backendCombo->currentText() == normalised ) {
			rebuildPreview( normalised );
			return;
		}
		m_backendCombo->setCurrentText( normalised );
	}

private:
	void destroySurface(){
		if ( m_surfaceWidget != nullptr ) {
			m_surfaceLayout->removeWidget( m_surfaceWidget );
			delete m_surfaceWidget;
			m_surfaceWidget = nullptr;
		}
	#if QT_CONFIG(vulkan)
		m_vulkanWindow = nullptr;
		delete m_vulkanInstance;
		m_vulkanInstance = nullptr;
	#endif
	}

	void rebuildPreview( const QString& backend ){
		const QString requestedBackend = Experimental_normalisePreviewBackend( backend );
		Experimental_storeRequestedPreviewBackend( requestedBackend );
		destroySurface();

		QString status;
		if ( requestedBackend == "Vulkan" ) {
	#if QT_CONFIG(vulkan)
			auto* instance = new QVulkanInstance;
			if ( instance->create() ) {
				auto* window = new ExperimentalVulkanWindow;
				window->setVulkanInstance( instance );
				auto* container = QWidget::createWindowContainer( window, this );
				if ( container != nullptr ) {
					container->setFocusPolicy( Qt::StrongFocus );
					m_vulkanInstance = instance;
					m_vulkanWindow = window;
					m_surfaceWidget = container;
					g_exp_activePreviewBackend = "Vulkan";
					status = "Vulkan preview active.";
					}
					else{
						delete window;
						delete instance;
						status = "Vulkan preview container could not be created. Falling back to OpenGL.";
					}
				}
				else{
					delete instance;
					status = "Vulkan runtime is unavailable on this machine. Falling back to OpenGL.";
				}
	#else
			status = "This Qt build does not expose Vulkan support. Falling back to OpenGL.";
#endif
		}

		if ( m_surfaceWidget == nullptr ) {
			m_surfaceWidget = new ExperimentalPreviewWidget;
			g_exp_activePreviewBackend = "OpenGL";
			if ( status.isEmpty() ) {
				status = "OpenGL preview active.";
			}
		}

		m_surfaceLayout->addWidget( m_surfaceWidget, 1 );
		m_statusLabel->setText( status );
		Experimental_liveSyncPreviewBackendChanged();
		Experimental_refreshLiveSyncUi();
	}
};

static QJsonArray Experimental_vector3ToJson( const Vector3& value ){
	QJsonArray array;
	array.append( value.x() );
	array.append( value.y() );
	array.append( value.z() );
	return array;
}

static bool Experimental_tryParseVector3( const QJsonValue& value, Vector3& out ){
	if ( !value.isArray() ) {
		return false;
	}
	const QJsonArray array = value.toArray();
	if ( array.size() < 3 ) {
		return false;
	}
	out = Vector3( array[0].toDouble(), array[1].toDouble(), array[2].toDouble() );
	return true;
}

static const char* Experimental_selectionModeName( SelectionSystem::EMode mode ){
	switch ( mode )
	{
	case SelectionSystem::eEntity:
		return "entity";
	case SelectionSystem::ePrimitive:
		return "primitive";
	case SelectionSystem::eComponent:
		return "component";
	}
	return "unknown";
}

static const char* Experimental_componentModeName( SelectionSystem::EComponentMode mode ){
	switch ( mode )
	{
	case SelectionSystem::eDefault:
		return "default";
	case SelectionSystem::eVertex:
		return "vertex";
	case SelectionSystem::eEdge:
		return "edge";
	case SelectionSystem::eFace:
		return "face";
	}
	return "unknown";
}

static const char* Experimental_manipulatorModeName( SelectionSystem::EManipulatorMode mode ){
	switch ( mode )
	{
	case SelectionSystem::eTranslate:
		return "translate";
	case SelectionSystem::eRotate:
		return "rotate";
	case SelectionSystem::eScale:
		return "scale";
	case SelectionSystem::eSkew:
		return "skew";
	case SelectionSystem::eDrag:
		return "drag";
	case SelectionSystem::eClip:
		return "clip";
	case SelectionSystem::eBuild:
		return "build";
	case SelectionSystem::eUV:
		return "uv";
	}
	return "unknown";
}

static QString Experimental_firstBrushShader( scene::Node& node );

static QString Experimental_currentMapPath(){
	if ( !Map_Valid( g_map ) || Map_Unnamed( g_map ) ) {
		return {};
	}
	return QString::fromLatin1( Map_Name( g_map ) );
}

static QJsonObject Experimental_buildCameraState(){
	QJsonObject camera;
	if ( CamWnd* camwnd = GlobalCamera_getCamWnd() ) {
		camera.insert( "origin", Experimental_vector3ToJson( Camera_getOrigin( *camwnd ) ) );
		camera.insert( "angles", Experimental_vector3ToJson( Camera_getAngles( *camwnd ) ) );
		camera.insert( "viewVector", Experimental_vector3ToJson( Camera_getViewVector( *camwnd ) ) );
	}
	return camera;
}

static QJsonObject Experimental_buildCameraBookmarksState( const QString& reason = QString() ){
	QJsonObject state;
	if ( !reason.isEmpty() ) {
		state.insert( "reason", reason );
	}
	state.insert( "cameraBookmarks", Experimental_buildCameraBookmarksArray() );
	return state;
}

static QJsonObject Experimental_buildSelectionState(){
	QJsonObject selection;
	selection.insert( "count", static_cast<int>( GlobalSelectionSystem().countSelected() ) );
	selection.insert( "componentCount", static_cast<int>( GlobalSelectionSystem().countSelectedComponents() ) );
	selection.insert( "mode", Experimental_selectionModeName( GlobalSelectionSystem().Mode() ) );
	selection.insert( "componentMode", Experimental_componentModeName( GlobalSelectionSystem().ComponentMode() ) );
	selection.insert( "manipulator", Experimental_manipulatorModeName( GlobalSelectionSystem().ManipulatorMode() ) );

	if ( GlobalSelectionSystem().countSelected() > 0 ) {
		const AABB bounds = GlobalSelectionSystem().getBoundsSelected();
		if ( aabb_valid( bounds ) ) {
			QJsonObject boundsJson;
			boundsJson.insert( "origin", Experimental_vector3ToJson( bounds.origin ) );
			boundsJson.insert( "extents", Experimental_vector3ToJson( bounds.extents ) );
			selection.insert( "bounds", boundsJson );
		}
	}

	QJsonArray items;
	class SelectedVisitor final : public SelectionSystem::Visitor
	{
		QJsonArray& m_items;
	public:
		explicit SelectedVisitor( QJsonArray& items ) : m_items( items ){
		}

		void visit( scene::Instance& instance ) const override {
			if ( m_items.size() >= 24 ) {
				return;
			}

			scene::Node& node = instance.path().top();
			QJsonObject item;
			if ( Entity* entity = Node_getEntity( node ) ) {
				item.insert( "type", "entity" );
				item.insert( "classname", entity->getClassName() );
				const char* name = entity->getKeyValue( "name" );
				const char* targetname = entity->getKeyValue( "targetname" );
				if ( name != nullptr && !string_empty( name ) ) {
					item.insert( "name", QString::fromLatin1( name ) );
				}
				else if ( targetname != nullptr && !string_empty( targetname ) ) {
					item.insert( "name", QString::fromLatin1( targetname ) );
				}
				else{
					item.insert( "name", QString::fromLatin1( entity->getClassName() ) );
				}
			}
			else if ( Node_getBrush( node ) != nullptr ) {
				item.insert( "type", "brush" );
				item.insert( "shader", Experimental_firstBrushShader( node ) );
			}
			else if ( Patch* patch = Node_getPatch( node ) ) {
				item.insert( "type", "patch" );
				item.insert( "shader", QString::fromLatin1( patch->GetShader() ) );
			}
			else{
				item.insert( "type", "node" );
			}

			const AABB bounds = instance.worldAABB();
			if ( aabb_valid( bounds ) ) {
				item.insert( "origin", Experimental_vector3ToJson( bounds.origin ) );
				item.insert( "extents", Experimental_vector3ToJson( bounds.extents ) );
			}

			m_items.append( item );
		}
	} visitor( items );

	GlobalSelectionSystem().foreachSelected( visitor );
	selection.insert( "items", items );
	return selection;
}

static QJsonObject Experimental_buildSceneSummary(){
	QJsonObject sceneSummary;
	sceneSummary.insert( "projectRoot", QString::fromLatin1( GameToolsPath_get() ) );
	sceneSummary.insert( "mapPath", Experimental_currentMapPath() );
	sceneSummary.insert( "modified", Map_Modified( g_map ) );
	sceneSummary.insert( "brushes", static_cast<int>( g_brushCount.get() ) );
	sceneSummary.insert( "patches", static_cast<int>( g_patchCount.get() ) );
	sceneSummary.insert( "entities", static_cast<int>( g_entityCount.get() ) );
	sceneSummary.insert( "previewBackendRequested", Experimental_requestedPreviewBackend() );
	sceneSummary.insert( "previewBackendActive", g_exp_activePreviewBackend );
	if ( Layer* currentLayer = GlobalSceneGraph().currentLayer() ) {
		sceneSummary.insert( "currentLayer", QString::fromLatin1( currentLayer->m_name.c_str() ) );
	}

	struct RuntimeSyncCounts
	{
		int streamingVolumes = 0;
		int spawnAnchors = 0;
		int props = 0;
		int fxEntities = 0;
	};

	class RuntimeSyncWalker final : public scene::Graph::Walker
	{
		RuntimeSyncCounts& m_counts;
	public:
		explicit RuntimeSyncWalker( RuntimeSyncCounts& counts ) : m_counts( counts ){
		}

			bool pre( const scene::Path& path, scene::Instance& ) const override {
				if ( Entity* entity = Node_getEntity( path.top() ) ) {
				const char* classname = entity->getClassName();
				if ( string_equal_prefix_nocase( classname, "trigger_level" ) || string_equal_prefix_nocase( classname, "trigger_stream" ) ) {
					++m_counts.streamingVolumes;
				}
				if ( string_equal_prefix_nocase( classname, "env_spawn" ) || string_equal_prefix_nocase( classname, "info_player" ) ) {
					++m_counts.spawnAnchors;
				}
				if ( string_equal_prefix_nocase( classname, "prop_" ) || string_equal_nocase( classname, "func_static" ) ) {
					++m_counts.props;
				}
				if ( string_equal_prefix_nocase( classname, "env_" ) || string_equal_prefix_nocase( classname, "fx_" ) ) {
					++m_counts.fxEntities;
				}
				}
				return true;
			}
		};
		RuntimeSyncCounts counts;
	GlobalSceneGraph().traverse( RuntimeSyncWalker( counts ) );
	sceneSummary.insert( "streamingVolumes", counts.streamingVolumes );
	sceneSummary.insert( "spawnAnchors", counts.spawnAnchors );
	sceneSummary.insert( "props", counts.props );
	sceneSummary.insert( "fxEntities", counts.fxEntities );
	return sceneSummary;
}

static QJsonObject Experimental_buildLiveSyncSnapshot( const QString& reason ){
	QJsonObject payload;
	payload.insert( "reason", reason );
	payload.insert( "scene", Experimental_buildSceneSummary() );
	payload.insert( "selection", Experimental_buildSelectionState() );
	payload.insert( "camera", Experimental_buildCameraState() );
	payload.insert( "bookmarks", Experimental_buildCameraBookmarksState() );
	return payload;
}

static QJsonObject Experimental_buildLiveSyncEnvelope( const QString& type, const QJsonObject& payload = QJsonObject() ){
	QJsonObject envelope;
	envelope.insert( "type", type );
	envelope.insert( "sentAt", QDateTime::currentDateTimeUtc().toString( Qt::ISODate ) );
	envelope.insert( "editorVersion", RADIANT_VERSION );
	envelope.insert( "payload", payload );
	return envelope;
}

class ExperimentalLiveSyncService final : public QObject
{
	struct PeerState
	{
		QByteArray buffer;
		bool handshakeComplete = false;
		QString runtimeName;
		QString runtimeRole;
		QString runtimeBuild;
		QDateTime lastMessageAt;
	};

	QTcpServer m_server;
	QHash<QTcpSocket*, PeerState> m_peers;
	QTimer m_sceneTimer;
	QTimer m_selectionTimer;
	QTimer m_cameraTimer;
	bool m_autoSync = true;

public:
	explicit ExperimentalLiveSyncService( QObject* parent = nullptr ) : QObject( parent ){
		m_sceneTimer.setSingleShot( true );
		m_selectionTimer.setSingleShot( true );
		m_cameraTimer.setSingleShot( true );

		QObject::connect( &m_server, &QTcpServer::newConnection, [this](){ acceptConnections(); } );
		QObject::connect( &m_sceneTimer, &QTimer::timeout, [this](){ sendSnapshot( "sceneChanged" ); } );
		QObject::connect( &m_selectionTimer, &QTimer::timeout, [this](){ broadcastSelectionState( "selectionChanged" ); } );
		QObject::connect( &m_cameraTimer, &QTimer::timeout, [this](){ broadcastCameraState( "cameraChanged" ); } );
	}

	~ExperimentalLiveSyncService() override {
		stop();
	}

	bool isRunning() const {
		return m_server.isListening();
	}

	quint16 port() const {
		return m_server.serverPort();
	}

	int clientCount() const {
		int count = 0;
		for ( auto it = m_peers.constBegin(); it != m_peers.constEnd(); ++it )
		{
			if ( it.value().handshakeComplete && it.key() != nullptr && it.key()->state() == QAbstractSocket::ConnectedState ) {
				++count;
			}
		}
		return count;
	}

	QString clientSummary() const {
		QStringList labels;
		for ( auto it = m_peers.constBegin(); it != m_peers.constEnd(); ++it )
		{
			if ( !it.value().handshakeComplete || it.key() == nullptr || it.key()->state() != QAbstractSocket::ConnectedState ) {
				continue;
			}

			QString label = it.value().runtimeName.trimmed();
			if ( label.isEmpty() ) {
				label = it.key()->peerAddress().toString();
			}
			if ( !it.value().runtimeRole.trimmed().isEmpty() ) {
				label += QString( " (%1)" ).arg( it.value().runtimeRole.trimmed() );
			}
			labels.push_back( label );
		}
		return labels.join( ", " );
	}

	bool autoSync() const {
		return m_autoSync;
	}

	void setAutoSync( bool enabled ){
		m_autoSync = enabled;
	}

	bool start( quint16 requestedPort ){
		if ( isRunning() && port() == requestedPort ) {
			Experimental_refreshLiveSyncUi();
			return true;
		}

		stop();
		if ( !m_server.listen( QHostAddress::LocalHost, requestedPort ) ) {
			Experimental_appendLiveSyncLog( QString( "Live Sync failed to start on ws://127.0.0.1:%1: %2" )
				.arg( requestedPort )
				.arg( m_server.errorString() ) );
			Experimental_refreshLiveSyncUi();
			return false;
		}

		Experimental_appendLiveSyncLog( QString( "Live Sync listening on ws://127.0.0.1:%1" ).arg( m_server.serverPort() ) );
		Experimental_refreshLiveSyncUi();
		return true;
	}

	void stop(){
		for ( auto it = m_peers.begin(); it != m_peers.end(); ++it )
		{
			if ( it.key() != nullptr ) {
				sendControlFrame( it.key(), 0x8, QByteArray() );
				it.key()->disconnectFromHost();
				it.key()->deleteLater();
			}
		}
		m_peers.clear();
		if ( m_server.isListening() ) {
			m_server.close();
			Experimental_appendLiveSyncLog( "Live Sync stopped." );
		}
		Experimental_refreshLiveSyncUi();
	}

	void scheduleSceneSync(){
		if ( m_autoSync && clientCount() > 0 ) {
			m_sceneTimer.start( 100 );
		}
	}

	void scheduleSelectionSync(){
		if ( m_autoSync && clientCount() > 0 ) {
			m_selectionTimer.start( 60 );
		}
	}

	void scheduleCameraSync(){
		if ( m_autoSync && clientCount() > 0 ) {
			m_cameraTimer.start( 45 );
		}
	}

	void sendSnapshot( const QString& reason ){
		if ( clientCount() == 0 ) {
			return;
		}
		broadcastJson( Experimental_buildLiveSyncEnvelope( "snapshot", Experimental_buildLiveSyncSnapshot( reason ) ) );
	}

	void broadcastSelectionState( const QString& reason ){
		if ( clientCount() == 0 ) {
			return;
		}
		QJsonObject payload;
		payload.insert( "reason", reason );
		payload.insert( "selection", Experimental_buildSelectionState() );
		broadcastJson( Experimental_buildLiveSyncEnvelope( "selection", payload ) );
	}

	void broadcastCameraState( const QString& reason ){
		if ( clientCount() == 0 ) {
			return;
		}
		QJsonObject payload;
		payload.insert( "reason", reason );
		payload.insert( "camera", Experimental_buildCameraState() );
		broadcastJson( Experimental_buildLiveSyncEnvelope( "camera", payload ) );
	}

	void broadcastRendererState(){
		if ( clientCount() == 0 ) {
			return;
		}
		QJsonObject payload;
		payload.insert( "requested", Experimental_requestedPreviewBackend() );
		payload.insert( "active", g_exp_activePreviewBackend );
		broadcastJson( Experimental_buildLiveSyncEnvelope( "renderer", payload ) );
	}

	void broadcastBookmarksState( const QString& reason ){
		if ( clientCount() == 0 ) {
			return;
		}
		broadcastJson( Experimental_buildLiveSyncEnvelope( "bookmarks", Experimental_buildCameraBookmarksState( reason ) ) );
	}

private:
	static QByteArray websocketAcceptKey( const QByteArray& key ){
		return QCryptographicHash::hash(
			key.trimmed() + QByteArrayLiteral( "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" ),
			QCryptographicHash::Sha1 ).toBase64();
	}

	void acceptConnections(){
		while ( m_server.hasPendingConnections() )
		{
			QTcpSocket* socket = m_server.nextPendingConnection();
			if ( socket == nullptr ) {
				continue;
			}

			m_peers.insert( socket, PeerState() );
			QObject::connect( socket, &QTcpSocket::readyRead, [this, socket](){ handleReadyRead( socket ); } );
			QObject::connect( socket, &QTcpSocket::disconnected, [this, socket](){ handleDisconnected( socket ); } );

			Experimental_appendLiveSyncLog( QString( "Incoming runtime connection from %1" ).arg( socket->peerAddress().toString() ) );
		}
	}

	void handleReadyRead( QTcpSocket* socket ){
		auto it = m_peers.find( socket );
		if ( it == m_peers.end() || socket == nullptr ) {
			return;
		}

		it->buffer += socket->readAll();
		if ( !it->handshakeComplete ) {
			if ( !tryHandleHandshake( socket, it.value() ) ) {
				return;
			}
		}

		processFrames( socket, it.value() );
	}

	void handleDisconnected( QTcpSocket* socket ){
		if ( socket == nullptr ) {
			return;
		}
		if ( m_peers.remove( socket ) > 0 ) {
			Experimental_appendLiveSyncLog( QString( "Runtime disconnected: %1" ).arg( socket->peerAddress().toString() ) );
			Experimental_refreshLiveSyncUi();
		}
		socket->deleteLater();
	}

	bool tryHandleHandshake( QTcpSocket* socket, PeerState& peer ){
		const int headerEnd = peer.buffer.indexOf( "\r\n\r\n" );
		if ( headerEnd < 0 ) {
			return false;
		}

		const QByteArray request = peer.buffer.left( headerEnd + 4 );
		peer.buffer.remove( 0, headerEnd + 4 );

		if ( !request.startsWith( "GET " ) ) {
			Experimental_appendLiveSyncLog( "Rejected non-WebSocket runtime connection." );
			socket->disconnectFromHost();
			return false;
		}

		QByteArray websocketKey;
		const QList<QByteArray> lines = request.split( '\n' );
		for ( QByteArray line : lines )
		{
			line = line.trimmed();
			if ( line.startsWith( "Sec-WebSocket-Key:" ) ) {
				websocketKey = line.mid( QByteArray( "Sec-WebSocket-Key:" ).size() ).trimmed();
			}
		}

		if ( websocketKey.isEmpty() ) {
			Experimental_appendLiveSyncLog( "Rejected WebSocket connection with missing Sec-WebSocket-Key." );
			socket->disconnectFromHost();
			return false;
		}

		QByteArray response;
		response += "HTTP/1.1 101 Switching Protocols\r\n";
		response += "Upgrade: websocket\r\n";
		response += "Connection: Upgrade\r\n";
		response += "Sec-WebSocket-Accept: " + websocketAcceptKey( websocketKey ) + "\r\n\r\n";
		socket->write( response );
		peer.handshakeComplete = true;

		Experimental_appendLiveSyncLog( QString( "Runtime handshake complete: ws://127.0.0.1:%1" ).arg( m_server.serverPort() ) );
		Experimental_refreshLiveSyncUi();
		sendJson( socket, Experimental_buildLiveSyncEnvelope( "hello", Experimental_buildLiveSyncSnapshot( "hello" ) ) );
		broadcastRendererState();
		return true;
	}

	void processFrames( QTcpSocket* socket, PeerState& peer ){
		for (;; )
		{
			if ( peer.buffer.size() < 2 ) {
				return;
			}

			const auto* data = reinterpret_cast<const unsigned char*>( peer.buffer.constData() );
			const bool fin = ( data[0] & 0x80 ) != 0;
			const quint8 opcode = data[0] & 0x0F;
			const bool masked = ( data[1] & 0x80 ) != 0;
			quint64 payloadLength = data[1] & 0x7F;
			int offset = 2;

			if ( !fin ) {
				Experimental_appendLiveSyncLog( "Fragmented WebSocket frames are not supported; closing runtime connection." );
				socket->disconnectFromHost();
				return;
			}

			if ( payloadLength == 126 ) {
				if ( peer.buffer.size() < 4 ) {
					return;
				}
				payloadLength = ( static_cast<quint64>( data[2] ) << 8 ) | static_cast<quint64>( data[3] );
				offset = 4;
			}
			else if ( payloadLength == 127 ) {
				if ( peer.buffer.size() < 10 ) {
					return;
				}
				payloadLength = 0;
				for ( int i = 0; i < 8; ++i )
				{
					payloadLength = ( payloadLength << 8 ) | static_cast<quint64>( data[2 + i] );
				}
				offset = 10;
			}

			if ( payloadLength > static_cast<quint64>( std::numeric_limits<int>::max() ) ) {
				Experimental_appendLiveSyncLog( "Received an oversized WebSocket payload; closing runtime connection." );
				socket->disconnectFromHost();
				return;
			}

			if ( masked ) {
				if ( peer.buffer.size() < offset + 4 ) {
					return;
				}
			}

			const int frameSize = offset + ( masked ? 4 : 0 ) + static_cast<int>( payloadLength );
			if ( peer.buffer.size() < frameSize ) {
				return;
			}

			QByteArray payload = peer.buffer.mid( offset + ( masked ? 4 : 0 ), static_cast<int>( payloadLength ) );
			if ( masked ) {
				const QByteArray mask = peer.buffer.mid( offset, 4 );
				for ( int i = 0; i < payload.size(); ++i )
				{
					payload[i] = payload[i] ^ mask[i % 4];
				}
			}
			peer.buffer.remove( 0, frameSize );

			if ( opcode == 0x8 ) {
				sendControlFrame( socket, 0x8, QByteArray() );
				socket->disconnectFromHost();
				return;
			}
			if ( opcode == 0x9 ) {
				sendControlFrame( socket, 0xA, payload );
				continue;
			}
			if ( opcode == 0xA ) {
				continue;
			}
			if ( opcode == 0x1 ) {
				handleTextMessage( socket, payload );
			}
		}
	}

	void handleTextMessage( QTcpSocket* socket, const QByteArray& payload ){
		auto peerIt = m_peers.find( socket );
		if ( peerIt != m_peers.end() ) {
			peerIt->lastMessageAt = QDateTime::currentDateTimeUtc();
		}

		QJsonParseError parseError;
		const QJsonDocument doc = QJsonDocument::fromJson( payload, &parseError );
		if ( parseError.error != QJsonParseError::NoError || !doc.isObject() ) {
			Experimental_appendLiveSyncLog( QString( "Ignored malformed runtime JSON: %1" ).arg( parseError.errorString() ) );
			return;
		}

		const QJsonObject message = doc.object();
		const QString type = message.value( "type" ).toString();
		const QJsonObject body = message.value( "payload" ).toObject();

		if ( type == "hello" ) {
			if ( peerIt != m_peers.end() ) {
				peerIt->runtimeName = body.value( "name" ).toString();
				peerIt->runtimeRole = body.value( "role" ).toString();
				peerIt->runtimeBuild = body.value( "build" ).toString();
			}
			const QString label = !body.value( "name" ).toString().trimmed().isEmpty()
				? body.value( "name" ).toString().trimmed()
				: socket->peerAddress().toString();
			g_exp_lastRuntimeEvent = QString( "Connected runtime: %1" ).arg( label );
			Experimental_appendLiveSyncLog( QString( "Runtime hello from %1" ).arg( label ) );
			Experimental_refreshLiveSyncUi();
			sendJson( socket, Experimental_buildLiveSyncEnvelope( "hello", Experimental_buildLiveSyncSnapshot( "editorHello" ) ) );
			return;
		}

		if ( type == "ping" ) {
			sendJson( socket, Experimental_buildLiveSyncEnvelope( "pong", QJsonObject() ) );
			return;
		}

		if ( type == "requestSnapshot" || type == "syncNow" ) {
			sendJson( socket, Experimental_buildLiveSyncEnvelope( "snapshot", Experimental_buildLiveSyncSnapshot( "runtimeRequest" ) ) );
			return;
		}

		if ( type == "requestSelection" ) {
			QJsonObject payload;
			payload.insert( "reason", "runtimeRequest" );
			payload.insert( "selection", Experimental_buildSelectionState() );
			sendJson( socket, Experimental_buildLiveSyncEnvelope( "selection", payload ) );
			return;
		}

		if ( type == "requestRenderer" || type == "requestRendererState" ) {
			QJsonObject payload;
			payload.insert( "requested", Experimental_requestedPreviewBackend() );
			payload.insert( "active", g_exp_activePreviewBackend );
			sendJson( socket, Experimental_buildLiveSyncEnvelope( "renderer", payload ) );
			return;
		}

		if ( type == "requestBookmarks" || type == "requestCameraBookmarks" ) {
			sendJson( socket, Experimental_buildLiveSyncEnvelope( "bookmarks", Experimental_buildCameraBookmarksState( "runtimeRequest" ) ) );
			return;
		}

		if ( type == "focusSelection" ) {
			FocusAllViews();
			sendJson( socket, Experimental_buildLiveSyncEnvelope( "ack", QJsonObject{ { "command", type } } ) );
			return;
		}

		if ( type == "setPreviewBackend" ) {
			const QString backend = Experimental_normalisePreviewBackend( body.value( "backend" ).toString() );
			if ( g_exp_previewHost != nullptr ) {
				g_exp_previewHost->setRequestedBackend( backend );
			}
			sendJson( socket, Experimental_buildLiveSyncEnvelope( "ack", QJsonObject{ { "command", type }, { "backend", backend } } ) );
			return;
		}

		if ( type == "runtimeCamera" || type == "setCamera" ) {
			if ( CamWnd* camwnd = GlobalCamera_getCamWnd() ) {
				Vector3 origin;
				Vector3 angles;
				bool changed = false;
				if ( Experimental_tryParseVector3( body.value( "origin" ), origin ) ) {
					Camera_setOrigin( *camwnd, origin );
					changed = true;
				}
				if ( Experimental_tryParseVector3( body.value( "angles" ), angles ) ) {
					Camera_setAngles( *camwnd, angles );
					changed = true;
				}
				if ( changed ) {
					CamWnd_Update( *camwnd );
					UpdateAllWindows();
					Experimental_appendLiveSyncLog( "Applied runtime camera transform." );
					sendJson( socket, Experimental_buildLiveSyncEnvelope( "camera", QJsonObject{ { "camera", Experimental_buildCameraState() } } ) );
				}
			}
			return;
		}

		if ( type == "storeBookmark" ) {
			const int index = body.value( "index" ).toInt( -1 );
			if ( index >= 0 && index < 5 ) {
				CameraBookmark_store( static_cast<std::size_t>( index ) );
				broadcastBookmarksState( "runtimeStore" );
				sendJson( socket, Experimental_buildLiveSyncEnvelope( "bookmarks", Experimental_buildCameraBookmarksState( "runtimeStore" ) ) );
			}
			return;
		}

		if ( type == "recallBookmark" ) {
			const int index = body.value( "index" ).toInt( -1 );
			if ( index >= 0 && index < 5 ) {
				CameraBookmark_recall( static_cast<std::size_t>( index ) );
				sendJson( socket, Experimental_buildLiveSyncEnvelope( "camera", QJsonObject{ { "camera", Experimental_buildCameraState() } } ) );
			}
			return;
		}

		const QString messageText = body.value( "message" ).toString();
		if ( type == "runtimeEvent" || type == "runtimeState" ) {
			g_exp_lastRuntimeEvent = messageText.isEmpty() ? type : messageText;
			Experimental_appendLiveSyncLog( QString( "Runtime: %1" ).arg( g_exp_lastRuntimeEvent ) );
			Experimental_refreshLiveSyncUi();
			return;
		}

		Experimental_appendLiveSyncLog( QString( "Ignored runtime message of type '%1'." ).arg( type ) );
	}

	void sendJson( QTcpSocket* socket, const QJsonObject& object ){
		if ( socket == nullptr || socket->state() != QAbstractSocket::ConnectedState ) {
			return;
		}
		sendDataFrame( socket, 0x1, QJsonDocument( object ).toJson( QJsonDocument::Compact ) );
	}

	void broadcastJson( const QJsonObject& object ){
		const QByteArray payload = QJsonDocument( object ).toJson( QJsonDocument::Compact );
		for ( auto it = m_peers.begin(); it != m_peers.end(); ++it )
		{
			if ( it.value().handshakeComplete ) {
				sendDataFrame( it.key(), 0x1, payload );
			}
		}
	}

	void sendControlFrame( QTcpSocket* socket, quint8 opcode, const QByteArray& payload ){
		sendDataFrame( socket, opcode, payload );
	}

	void sendDataFrame( QTcpSocket* socket, quint8 opcode, const QByteArray& payload ){
		if ( socket == nullptr || socket->state() != QAbstractSocket::ConnectedState ) {
			return;
		}

		QByteArray frame;
		frame.append( static_cast<char>( 0x80 | opcode ) );
		if ( payload.size() < 126 ) {
			frame.append( static_cast<char>( payload.size() ) );
		}
		else if ( payload.size() <= 0xFFFF ) {
			frame.append( static_cast<char>( 126 ) );
			frame.append( static_cast<char>( ( payload.size() >> 8 ) & 0xFF ) );
			frame.append( static_cast<char>( payload.size() & 0xFF ) );
		}
		else{
			frame.append( static_cast<char>( 127 ) );
			const quint64 size = static_cast<quint64>( payload.size() );
			for ( int shift = 56; shift >= 0; shift -= 8 )
			{
				frame.append( static_cast<char>( ( size >> shift ) & 0xFF ) );
			}
		}
		frame.append( payload );
		socket->write( frame );
	}
};

static void Experimental_appendLiveSyncLog( const QString& message ){
	const QString stamped = QString( "[%1] %2" )
		.arg( QDateTime::currentDateTime().toString( "HH:mm:ss" ) )
		.arg( message );
	if ( g_exp_syncLog != nullptr ) {
		g_exp_syncLog->addItem( stamped );
		while ( g_exp_syncLog->count() > 200 )
		{
			delete g_exp_syncLog->takeItem( 0 );
		}
		g_exp_syncLog->scrollToBottom();
	}
	globalOutputStream() << stamped.toLatin1().constData() << '\n';
}

static void Experimental_refreshLiveSyncUi(){
	if ( g_exp_syncStateLabel != nullptr ) {
		if ( g_exp_liveSyncService != nullptr && g_exp_liveSyncService->isRunning() ) {
			g_exp_syncStateLabel->setText( QString( "Listening on ws://127.0.0.1:%1" ).arg( g_exp_liveSyncService->port() ) );
		}
		else{
			g_exp_syncStateLabel->setText( "Stopped" );
		}
	}
	if ( g_exp_syncClientsLabel != nullptr ) {
		const int clients = g_exp_liveSyncService != nullptr ? g_exp_liveSyncService->clientCount() : 0;
		g_exp_syncClientsLabel->setText( QString::number( clients ) );
		g_exp_syncClientsLabel->setToolTip( g_exp_liveSyncService != nullptr ? g_exp_liveSyncService->clientSummary() : QString() );
	}
	if ( g_exp_syncRuntimeLabel != nullptr ) {
		g_exp_syncRuntimeLabel->setText( g_exp_lastRuntimeEvent );
		g_exp_syncRuntimeLabel->setToolTip( QString( "Preview backend: %1 active, %2 requested" )
			.arg( g_exp_activePreviewBackend )
			.arg( Experimental_requestedPreviewBackend() ) );
	}
}

static void Experimental_liveSyncSceneChanged(){
	if ( g_exp_liveSyncService != nullptr ) {
		g_exp_liveSyncService->scheduleSceneSync();
	}
}

static void Experimental_liveSyncSelectionChanged(){
	if ( g_exp_liveSyncService != nullptr ) {
		g_exp_liveSyncService->scheduleSelectionSync();
	}
}

static void Experimental_liveSyncCameraChanged(){
	if ( g_exp_liveSyncService != nullptr ) {
		g_exp_liveSyncService->scheduleCameraSync();
	}
}

static void Experimental_liveSyncPreviewBackendChanged(){
	if ( g_exp_liveSyncService != nullptr ) {
		g_exp_liveSyncService->broadcastRendererState();
	}
}

class ExperimentalUndoTracker final : public UndoTracker
{
	void addEvent( const char* event ) const {
		if ( g_exp_historyList == nullptr ) {
			return;
		}
		g_exp_historyList->addItem( StringStream( '#', ++g_exp_historyCounter, ' ', event ).c_str() );
		g_exp_historyList->scrollToBottom();
	}
public:
	void clear() override {
		g_exp_historyCounter = 0;
		if ( g_exp_historyList != nullptr ) {
			g_exp_historyList->clear();
		}
	}
	void begin() override {
		addEvent( "Begin change" );
	}
	void undo() override {
		addEvent( "Undo" );
	}
	void redo() override {
		addEvent( "Redo" );
	}
};
ExperimentalUndoTracker g_experimentalUndoTracker;

void Experimental_setUndoTrackerAttached( bool attached ){
	if ( attached && !g_exp_undoTrackerAttached ) {
		GlobalUndoSystem().trackerAttach( g_experimentalUndoTracker );
		g_exp_undoTrackerAttached = true;
	}
	else if ( !attached && g_exp_undoTrackerAttached ) {
		GlobalUndoSystem().trackerDetach( g_experimentalUndoTracker );
		g_exp_undoTrackerAttached = false;
	}
}

static Vector3 Experimental_getScaleFromSelection(){
	Vector3 scale( 1, 1, 1 );
	if ( GlobalSelectionSystem().countSelected() != 1 ) {
		return scale;
	}
	// Try entity keyvalues first (misc_model, etc.)
	const char* vec = SelectedEntity_getValueForKey( "modelscale_vec" );
	if ( !string_empty( vec ) ) {
		DoubleVector3 v;
		if ( string_parse_vector3( vec, v ) && v[0] != 0 && v[1] != 0 && v[2] != 0 ) {
			return Vector3( v );
		}
	}
	const char* uni = SelectedEntity_getValueForKey( "modelscale" );
	if ( !string_empty( uni ) ) {
		float f;
		if ( string_parse_float( uni, f ) && f != 0 ) {
			return Vector3( f, f, f );
		}
	}
	// Try transform matrix for nodes with TransformNode (walk path from selected to root)
	scene::Instance& inst = GlobalSelectionSystem().firstSelected();
	const scene::Path& path = inst.path();
	for ( std::size_t i = path.size(); i > 0; --i ) {
		TransformNode* tn = Node_getTransformNode( path[i - 1].get() );
		if ( tn != nullptr ) {
			DoubleVector3 s = matrix4_get_scale_vec3( tn->localToParent() );
			if ( s[0] > 0.0001 && s[1] > 0.0001 && s[2] > 0.0001 ) {
				return Vector3( s );
			}
		}
	}
	return scale;
}

static bool Experimental_selectionHasModelScale(){
	if ( GlobalSelectionSystem().countSelected() != 1 ) {
		return false;
	}
	const char* vec = SelectedEntity_getValueForKey( "modelscale_vec" );
	const char* uni = SelectedEntity_getValueForKey( "modelscale" );
	return !string_empty( vec ) || !string_empty( uni );
}

static QString Experimental_selectedNodeShader(){
	if ( GlobalSelectionSystem().countSelected() != 1 ) {
		return {};
	}

	scene::Instance& instance = GlobalSelectionSystem().firstSelected();
	scene::Node& node = instance.path().top();
	if ( Node_getBrush( node ) != nullptr ) {
		return Experimental_firstBrushShader( node );
	}
	if ( Patch* patch = Node_getPatch( node ) ) {
		return QString::fromLatin1( patch->GetShader() );
	}
	if ( Entity* entity = Node_getEntity( node ) ) {
		const char* model = entity->getKeyValue( "model" );
		if ( !string_empty( model ) ) {
			return QString::fromLatin1( model );
		}
	}
	return {};
}

static scene::Node* Experimental_worldspawnNode(){
	return Map_FindWorldspawn( g_map );
}

static Entity* Experimental_worldspawnEntity(){
	scene::Node* worldNode = Experimental_worldspawnNode();
	return worldNode != nullptr ? Node_getEntity( *worldNode ) : nullptr;
}

static Entity* Experimental_selectedEntityForInspector(){
	if ( GlobalSelectionSystem().countSelected() != 1 ) {
		return nullptr;
	}

	scene::Instance& instance = GlobalSelectionSystem().firstSelected();
	Entity* entity = Node_getEntity( instance.path().top() );
	if ( entity == nullptr && instance.path().size() > 1 ) {
		entity = Node_getEntity( instance.path().parent() );
	}
	return entity;
}

static bool Experimental_selectedEntityClassPrefix( const char* prefix ){
	Entity* entity = Experimental_selectedEntityForInspector();
	return entity != nullptr && string_equal_prefix_nocase( entity->getClassName(), prefix );
}

static void Experimental_setLineEditValue( QLineEdit* edit, const QString& value ){
	if ( edit == nullptr ) {
		return;
	}
	edit->blockSignals( true );
	edit->setText( value );
	edit->blockSignals( false );
}

static void Experimental_setSpinValue( QDoubleSpinBox* spin, double value ){
	if ( spin == nullptr ) {
		return;
	}
	spin->blockSignals( true );
	spin->setValue( value );
	spin->blockSignals( false );
}

static void Experimental_setSelectedEntityKeyValue( const char* key, const QString& value ){
	Entity* entity = Experimental_selectedEntityForInspector();
	if ( entity == nullptr ) {
		return;
	}

	const QByteArray utf8 = value.trimmed().toUtf8();
	const auto command = StringStream( "entitySetKeyValue -key ", Quoted( key ), " -value ", Quoted( utf8.constData() ) );
	UndoableCommand undo( command );
	Scene_EntitySetKeyValue_Selected( key, utf8.constData() );
	SceneChangeNotify();
}

static void Experimental_setWorldspawnKeyValue( const char* key, const QString& value ){
	Entity* worldspawn = Experimental_worldspawnEntity();
	if ( worldspawn == nullptr ) {
		return;
	}

	const QByteArray utf8 = value.trimmed().toUtf8();
	const auto command = StringStream( "entitySetWorldspawnKeyValue -key ", Quoted( key ), " -value ", Quoted( utf8.constData() ) );
	UndoableCommand undo( command );
	worldspawn->setKeyValue( key, utf8.constData() );
	SceneChangeNotify();
}

static QString Experimental_selectionTypeSummary(){
	const std::size_t selectedCount = GlobalSelectionSystem().countSelected();
	const std::size_t selectedComponents = GlobalSelectionSystem().countSelectedComponents();
	if ( selectedCount == 0 ) {
		return "Nothing selected";
	}
	if ( selectedComponents != 0 ) {
		return StringStream( "Component selection (", Unsigned( selectedComponents ), ")" ).c_str();
	}
	if ( selectedCount != 1 ) {
		return StringStream( "Multi-selection (", Unsigned( selectedCount ), ")" ).c_str();
	}

	scene::Instance& instance = GlobalSelectionSystem().firstSelected();
	scene::Node& node = instance.path().top();
	if ( Entity* entity = Node_getEntity( node ) ) {
		return StringStream( "Entity: ", entity->getClassName() ).c_str();
	}
	if ( Node_getBrush( node ) != nullptr ) {
		return "Brush";
	}
	if ( Node_getPatch( node ) != nullptr ) {
		return "Patch";
	}
	return "Selection";
}

static QString Experimental_selectionBoundsSummary(){
	const bool hasSelection = GlobalSelectionSystem().countSelected() != 0;
	const AABB bounds = GlobalSelectionSystem().getBoundsSelected();
	if ( !hasSelection || !aabb_valid( bounds ) ) {
		return "No bounds";
	}

	const Vector3 size = bounds.extents * 2.f;
	return QString( "%1 x %2 x %3" )
		.arg( size.x(), 0, 'f', 1 )
		.arg( size.y(), 0, 'f', 1 )
		.arg( size.z(), 0, 'f', 1 );
}

static void Experimental_refreshEntityPanels(){
	Entity* entity = Experimental_selectedEntityForInspector();
	const bool hasEntity = entity != nullptr;
	if ( g_exp_entityGroup != nullptr ) {
		g_exp_entityGroup->setVisible( hasEntity );
	}
	if ( hasEntity ) {
		if ( g_exp_entityClassLabel != nullptr ) {
			g_exp_entityClassLabel->setText( entity->getClassName() );
		}
		Experimental_setLineEditValue( g_exp_entityNameEdit, entity->getKeyValue( "name" ) );
		Experimental_setLineEditValue( g_exp_entityTargetnameEdit, entity->getKeyValue( "targetname" ) );
		Experimental_setLineEditValue( g_exp_entityTargetEdit, entity->getKeyValue( "target" ) );
		Experimental_setLineEditValue( g_exp_entitySoundEdit, entity->getKeyValue( "sound" ) );
		Experimental_setLineEditValue( g_exp_entityModelEdit, entity->getKeyValue( "model" ) );
	}

	Entity* worldspawn = Experimental_worldspawnEntity();
	if ( g_exp_worldGroup != nullptr ) {
		g_exp_worldGroup->setVisible( worldspawn != nullptr );
	}
	if ( worldspawn != nullptr ) {
		Experimental_setLineEditValue( g_exp_worldMessageEdit, worldspawn->getKeyValue( "message" ) );
		Experimental_setLineEditValue( g_exp_worldMusicEdit, worldspawn->getKeyValue( "music" ) );
		Experimental_setLineEditValue( g_exp_worldGravityEdit, worldspawn->getKeyValue( "gravity" ) );
		Experimental_setLineEditValue( g_exp_worldColorEdit, worldspawn->getKeyValue( "_color" ) );
		float minlight = 0;
		string_parse_float( worldspawn->getKeyValue( "_minlight" ), minlight );
		Experimental_setSpinValue( g_exp_worldMinlight, minlight );
	}

	const bool isGravityVolume = Experimental_selectedEntityClassPrefix( "trigger_gravity" );
	if ( g_exp_gravityGroup != nullptr ) {
		g_exp_gravityGroup->setVisible( isGravityVolume );
	}
	if ( isGravityVolume && entity != nullptr ) {
		Experimental_setLineEditValue( g_exp_gravityDirectionEdit, entity->getKeyValue( "gravity_dir" ) );
		Experimental_setLineEditValue( g_exp_gravityMagnitudeEdit, entity->getKeyValue( "gravity_magnitude" ) );
	}
}

static void Experimental_pullShaderFromSelection(){
	if ( g_exp_shaderEdit == nullptr ) {
		return;
	}

	const QString shader = Experimental_selectedNodeShader();
	if ( shader.isEmpty() ) {
		Sys_Status( "Selection has no shader to pull" );
		return;
	}

	g_exp_shaderEdit->setText( shader );
}

void Experimental_refreshTransform(){
	const bool hasSelection = GlobalSelectionSystem().countSelected() != 0;
	const AABB bounds = GlobalSelectionSystem().getBoundsSelected();
	const bool validBounds = hasSelection && aabb_valid( bounds );

	if ( g_exp_locX != nullptr ) {
		g_exp_locX->setEnabled( hasSelection );
		g_exp_locY->setEnabled( hasSelection );
		g_exp_locZ->setEnabled( hasSelection );
		g_exp_rotX->setEnabled( hasSelection );
		g_exp_rotY->setEnabled( hasSelection );
		g_exp_rotZ->setEnabled( hasSelection );
		g_exp_scaleX->setEnabled( hasSelection );
		g_exp_scaleY->setEnabled( hasSelection );
		g_exp_scaleZ->setEnabled( hasSelection );
		if ( g_exp_uniformScale != nullptr ) {
			g_exp_uniformScale->setEnabled( hasSelection );
		}
		if ( validBounds ) {
			g_exp_locX->blockSignals( true );
			g_exp_locY->blockSignals( true );
			g_exp_locZ->blockSignals( true );
			g_exp_locX->setValue( bounds.origin.x() );
			g_exp_locY->setValue( bounds.origin.y() );
			g_exp_locZ->setValue( bounds.origin.z() );
			g_exp_locX->blockSignals( false );
			g_exp_locY->blockSignals( false );
			g_exp_locZ->blockSignals( false );
		}
		else{
			g_exp_locX->blockSignals( true );
			g_exp_locY->blockSignals( true );
			g_exp_locZ->blockSignals( true );
			g_exp_locX->setValue( 0 );
			g_exp_locY->setValue( 0 );
			g_exp_locZ->setValue( 0 );
			g_exp_locX->blockSignals( false );
			g_exp_locY->blockSignals( false );
			g_exp_locZ->blockSignals( false );
		}
		if ( hasSelection ) {
			g_exp_rotX->blockSignals( true );
			g_exp_rotY->blockSignals( true );
			g_exp_rotZ->blockSignals( true );
			g_exp_scaleX->blockSignals( true );
			g_exp_scaleY->blockSignals( true );
			g_exp_scaleZ->blockSignals( true );
			g_exp_rotX->setValue( 0 );
			g_exp_rotY->setValue( 0 );
			g_exp_rotZ->setValue( 0 );
			const Vector3 scale = Experimental_getScaleFromSelection();
			g_exp_scaleX->setValue( scale.x() );
			g_exp_scaleY->setValue( scale.y() );
			g_exp_scaleZ->setValue( scale.z() );
			g_exp_rotX->blockSignals( false );
			g_exp_rotY->blockSignals( false );
			g_exp_rotZ->blockSignals( false );
			g_exp_scaleX->blockSignals( false );
			g_exp_scaleY->blockSignals( false );
			g_exp_scaleZ->blockSignals( false );
		}
		else{
			g_exp_rotX->blockSignals( true );
			g_exp_rotY->blockSignals( true );
			g_exp_rotZ->blockSignals( true );
			g_exp_scaleX->blockSignals( true );
			g_exp_scaleY->blockSignals( true );
			g_exp_scaleZ->blockSignals( true );
			g_exp_rotX->setValue( 0 );
			g_exp_rotY->setValue( 0 );
			g_exp_rotZ->setValue( 0 );
			g_exp_scaleX->setValue( 1 );
			g_exp_scaleY->setValue( 1 );
			g_exp_scaleZ->setValue( 1 );
			g_exp_rotX->blockSignals( false );
			g_exp_rotY->blockSignals( false );
			g_exp_rotZ->blockSignals( false );
			g_exp_scaleX->blockSignals( false );
			g_exp_scaleY->blockSignals( false );
			g_exp_scaleZ->blockSignals( false );
		}
	}
}

void Experimental_refreshSelection(){
	const bool hasSelection = GlobalSelectionSystem().countSelected() != 0;
	if ( g_exp_selectedCountLabel != nullptr ) {
		g_exp_selectedCountLabel->setText( StringStream( GlobalSelectionSystem().countSelected() ).c_str() );
	}
	if ( g_exp_selectedComponentsLabel != nullptr ) {
		g_exp_selectedComponentsLabel->setText( StringStream( GlobalSelectionSystem().countSelectedComponents() ).c_str() );
	}
	if ( g_exp_selectionTypeLabel != nullptr ) {
		g_exp_selectionTypeLabel->setText( Experimental_selectionTypeSummary() );
	}
	if ( g_exp_selectionBoundsLabel != nullptr ) {
		g_exp_selectionBoundsLabel->setText( Experimental_selectionBoundsSummary() );
	}
	if ( g_exp_selectionShaderLabel != nullptr ) {
		const QString shader = Experimental_selectedNodeShader();
		g_exp_selectionShaderLabel->setText( shader.isEmpty() ? "None" : shader );
	}
	if ( g_exp_shaderEdit != nullptr ) {
		g_exp_shaderEdit->setEnabled( hasSelection );
		g_exp_shaderEdit->setPlaceholderText( hasSelection ? "textures/common/caulk" : "Select something to edit shader" );
	}
	Experimental_refreshEntityPanels();
	Experimental_refreshTransform();
}

void Experimental_selectionChanged( const Selectable& ){
	// Defer refresh: selection callbacks run before onSelectedChanged updates m_selection,
	// so firstSelected() would assert if we ran synchronously when selecting the first item.
	QTimer::singleShot( 0, [](){ Experimental_refreshSelection(); } );
	Experimental_liveSyncSelectionChanged();
}

void Experimental_applySelectedShader(){
	if ( g_exp_shaderEdit == nullptr || g_exp_shaderEdit->text().isEmpty() ) {
		return;
	}

	const auto shader = g_exp_shaderEdit->text().trimmed().toLatin1();
	if ( shader.isEmpty() ) {
		return;
	}

	Select_SetShader_Undo( shader.constData() );
	UpdateAllWindows();
}

struct ExperimentalShaderNameVisitor
{
	void operator()( const char* name ) const {
		if ( g_exp_assetsList != nullptr ) {
			g_exp_assetsList->addItem( name );
		}
	}
};

void Experimental_refreshAssetLibrary(){
	if ( g_exp_assetsList == nullptr ) {
		return;
	}
	g_exp_assetsList->clear();
	GlobalShaderSystem().foreachShaderName( makeCallback( ExperimentalShaderNameVisitor() ) );
	g_exp_assetsList->sortItems();
}

void Experimental_toggleDock( QDockWidget* dock ){
	if ( dock != nullptr ) {
		dock->setVisible( !dock->isVisible() );
	}
}

static void Experimental_showDock( QDockWidget* dock ){
	if ( dock != nullptr ) {
		dock->show();
		dock->raise();
	}
}

static void Experimental_hideDock( QDockWidget* dock ){
	if ( dock != nullptr ) {
		dock->hide();
	}
}

static void OpenWysiwygWorkspace_impl(){
	Experimental_showDock( g_exp_propertiesDock );
	Experimental_showDock( g_exp_previewDock );
	Experimental_showDock( g_exp_assetsDock );
	Experimental_showDock( g_exp_usdDock );
	Experimental_hideDock( g_exp_historyDock );
	Experimental_hideDock( g_exp_syncDock );
	Experimental_hideDock( g_exp_ecsDock );
	Sys_Status( "Workspace panels ready" );
}

void Experimental_togglePropertiesDock_impl(){
	Experimental_toggleDock( g_exp_propertiesDock );
}
void Experimental_togglePreviewDock_impl(){
	Experimental_toggleDock( g_exp_previewDock );
}
void Experimental_toggleAssetsDock_impl(){
	Experimental_toggleDock( g_exp_assetsDock );
}
void Experimental_toggleHistoryDock_impl(){
	Experimental_toggleDock( g_exp_historyDock );
}
void Experimental_toggleSyncDock_impl(){
	Experimental_toggleDock( g_exp_syncDock );
}
void Experimental_toggleUSDDock_impl(){
	Experimental_toggleDock( g_exp_usdDock );
}

void Experimental_toggleECSDock_impl(){
	Experimental_toggleDock( g_exp_ecsDock );
}

static bool ECS_isAdvancedEntity( const char* name ){
	return string_equal_prefix_nocase( name, "env_" )
	    || string_equal_prefix_nocase( name, "prop_" )
	    || string_equal_prefix_nocase( name, "trigger_gravity" )
	    || string_equal_prefix_nocase( name, "env_spawn" )
	    || string_equal_prefix_nocase( name, "trigger_level_stream" )
	    || string_equal_prefix_nocase( name, "info_vehicle" )
	    || string_equal_prefix_nocase( name, "func_vehicle" )
	    || string_equal_nocase( name, "misc_spline" );
}

static const char* ECS_categoryForEntity( const char* name ){
	if ( string_equal_prefix_nocase( name, "env_fire" ) || string_equal_prefix_nocase( name, "env_water" ) || string_equal_prefix_nocase( name, "env_spill" ) )
		return "Fire & Environment";
	if ( string_equal_prefix_nocase( name, "env_fan" ) )
		return "Wind & Physics";
	if ( string_equal_prefix_nocase( name, "prop_" ) )
		return "Props & Physics";
	if ( string_equal_prefix_nocase( name, "trigger_gravity" ) )
		return "Gravity & Space";
	if ( string_equal_prefix_nocase( name, "env_spawn" ) )
		return "Spawn & Streaming";
	if ( string_equal_prefix_nocase( name, "trigger_level" ) )
		return "Level Streaming";
	if ( string_equal_prefix_nocase( name, "info_vehicle" ) || string_equal_prefix_nocase( name, "func_vehicle" ) )
		return "Vehicles";
	if ( string_equal_nocase( name, "misc_spline" ) )
		return "Splines";
	return "Other";
}

static void Experimental_refreshECSList(){
	if ( g_exp_ecsEntityList == nullptr || g_exp_ecsCategoryCombo == nullptr ) {
		return;
	}
	const QString cat = g_exp_ecsCategoryCombo->currentText();
	g_exp_ecsEntityList->clear();

	class ECSCollector final : public EntityClassVisitor
	{
		QListWidget* m_list;
		QString m_cat;
		static QIcon iconForEntityClass( const EntityClass* eclass ){
			if ( classname_equal( eclass->name(), "light" ) || classname_equal( eclass->name(), "lightJunior" ) ) {
				return new_local_icon( "ecs_light" );
			}
			if ( eclass->miscmodel_is ) {
				return new_local_icon( "ecs_model" );
			}
			if ( string_compare_nocase_n( eclass->name(), "trigger_", 8 ) == 0 ) {
				return new_local_icon( "ecs_trigger" );
			}
			if ( classname_equal( eclass->name(), "info_player_start" )
			  || classname_equal( eclass->name(), "info_player_deathmatch" )
			  || classname_equal( eclass->name(), "team_ctf_redplayer" )
			  || classname_equal( eclass->name(), "team_ctf_blueplayer" )
			  || classname_equal( eclass->name(), "team_ctf_redspawn" )
			  || classname_equal( eclass->name(), "team_ctf_bluespawn" ) ) {
				return new_local_icon( "ecs_spawn" );
			}
			if ( eclass->fixedsize ) {
				return new_local_icon( "ecs_pointentity" );
			}
			return new_local_icon( "ecs_brushentity" );
		}
	public:
		ECSCollector( QListWidget* list, const QString& cat ) : m_list( list ), m_cat( cat ){}
		void visit( EntityClass* eclass ) override {
			if ( !ECS_isAdvancedEntity( eclass->name() ) ) {
				return;
			}
			if ( !m_cat.isEmpty() && m_cat != "All" && QString( ECS_categoryForEntity( eclass->name() ) ) != m_cat ) {
				return;
			}
			m_list->addItem( new QListWidgetItem( iconForEntityClass( eclass ), eclass->name() ) );
		}
	} collector( g_exp_ecsEntityList, cat );
	GlobalEntityClassManager().forEach( collector );
	g_exp_ecsEntityList->sortItems();
}

static void Experimental_ecsAddEntity( const char* classname ){
	Entity_createFromSelection( classname, Add_entitySpawnOrigin() );
	SceneChangeNotify();
}

void Experimental_importUSDStructure_impl(){
	if ( g_exp_usdTree == nullptr ) {
		return;
	}

	const auto filename = QFileDialog::getOpenFileName( MainFrame_getWindow(), "Import USD Structure", "", "USD Files (*.usd *.usda *.usdc)" );
	if ( filename.isEmpty() ) {
		return;
	}

	QFile file( filename );
	if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		globalErrorStream() << "failed to open USD file: " << filename.toLatin1().constData() << '\n';
		return;
	}

	g_exp_usdTree->clear();

	QTextStream stream( &file );
	QList<QTreeWidgetItem*> stack;
	const QRegularExpression defRegex( "^def\\s+\\w+\\s+\"([^\"]+)\"" );

	while ( !stream.atEnd() )
	{
		const auto line = stream.readLine().trimmed();

		if ( line.startsWith( '}' ) ) {
			if ( !stack.isEmpty() ) {
				stack.removeLast();
			}
			continue;
		}

		const auto match = defRegex.match( line );
		if ( !match.hasMatch() ) {
			continue;
		}

		auto* item = new QTreeWidgetItem( QStringList( match.captured( 1 ) ) );
		if ( stack.isEmpty() ) {
			g_exp_usdTree->addTopLevelItem( item );
		}
		else{
			stack.back()->addChild( item );
		}

		if ( line.contains( '{' ) ) {
			stack.push_back( item );
		}
	}

	g_exp_usdTree->expandAll();
	if ( g_exp_usdDock != nullptr ) {
		g_exp_usdDock->show();
	}
}

static QString usdSanitizeName( const char* name ){
	QString s;
	for ( const char* p = name; *p; ++p ) {
		if ( ( *p >= 'a' && *p <= 'z' ) || ( *p >= 'A' && *p <= 'Z' ) || ( *p >= '0' && *p <= '9' ) || *p == '_' ) {
			s += *p;
		}
		else if ( *p == ' ' || *p == '-' ) {
			s += '_';
		}
	}
	return s.isEmpty() ? "unnamed" : s;
}

struct ExperimentalSceneHierarchyEntry
{
	QString name;
	QString parent;
	QString type;
};

struct MayaAsciiNode
{
	QString name;
	QString parent;
	QString radiantClassname;
	QString radiantNodeType;
	QString radiantShader;
	QString radiantEntityJson;
	QString radiantSceneJson;
	QString radiantPatchJson;
	Vector3 translation{ 0, 0, 0 };
	Vector3 rotation{ 0, 0, 0 };
	Vector3 scale{ 1, 1, 1 };
	Vector3 size{ 16, 16, 16 };
	int patchWidth = 3;
	int patchHeight = 3;
	bool hasGeometry = false;
};

struct MayaAsciiCreator
{
	QString type;
	double width = 1;
	double height = 1;
	double depth = 1;
	int subdivX = 1;
	int subdivY = 1;
};

struct MayaResolvedTransform
{
	Vector3 translation{ 0, 0, 0 };
	Vector3 rotation{ 0, 0, 0 };
	Vector3 scale{ 1, 1, 1 };
};

static QString Experimental_escapeMayaString( QString value ){
	value.replace( '\\', "\\\\" );
	value.replace( '"', "\\\"" );
	return value;
}

static QString Experimental_unescapeMayaString( QString value ){
	value.replace( "\\\"", "\"" );
	value.replace( "\\\\", "\\" );
	return value;
}

static QString Experimental_mayaSanitizeName( const QString& name ){
	QString sanitized;
	for ( const QChar ch : name ) {
		if ( ch.isLetterOrNumber() || ch == '_' ) {
			sanitized += ch;
		}
		else if ( ch == ' ' || ch == '-' || ch == '.' || ch == ':' || ch == '|' || ch == '/' ) {
			sanitized += '_';
		}
	}
	if ( sanitized.isEmpty() ) {
		sanitized = "node";
	}
	if ( sanitized.front().isDigit() ) {
		sanitized.prepend( "n_" );
	}
	return sanitized;
}

static QString Experimental_makeUniqueMayaName( const QString& name, QSet<QString>& usedNames ){
	const QString base = Experimental_mayaSanitizeName( name );
	QString candidate = base;
	int suffix = 1;
	while ( usedNames.contains( candidate ) ) {
		candidate = base + "_" + QString::number( suffix++ );
	}
	usedNames.insert( candidate );
	return candidate;
}

static void Experimental_populateSceneHierarchyTree( const QVector<ExperimentalSceneHierarchyEntry>& entries ){
	if ( g_exp_usdTree == nullptr ) {
		return;
	}

	g_exp_usdTree->clear();

	QHash<QString, QTreeWidgetItem*> items;
	for ( const auto& entry : entries ) {
		auto* item = new QTreeWidgetItem( QStringList() << entry.name << entry.type );
		item->setToolTip( 0, entry.parent.isEmpty() ? entry.type : QString( "%1 under %2" ).arg( entry.type, entry.parent ) );
		if ( !entry.parent.isEmpty() && items.contains( entry.parent ) ) {
			items.value( entry.parent )->addChild( item );
		}
		else{
			g_exp_usdTree->addTopLevelItem( item );
		}
		items.insert( entry.name, item );
	}

	g_exp_usdTree->expandAll();
	if ( g_exp_usdDock != nullptr ) {
		g_exp_usdDock->show();
	}
}

static Vector3 Experimental_componentMultiply( const Vector3& a, const Vector3& b ){
	return Vector3( a.x() * b.x(), a.y() * b.y(), a.z() * b.z() );
}

static Vector3 Experimental_componentAbs( const Vector3& value ){
	return Vector3( std::fabs( value.x() ), std::fabs( value.y() ), std::fabs( value.z() ) );
}

static bool Experimental_vectorNearlyEqual( const Vector3& value, const Vector3& target, double epsilon = 0.0001 ){
	return std::fabs( value.x() - target.x() ) < epsilon
	    && std::fabs( value.y() - target.y() ) < epsilon
	    && std::fabs( value.z() - target.z() ) < epsilon;
}

static QString Experimental_vectorToMayaString( const Vector3& value ){
	return QString( "%1 %2 %3" )
		.arg( value.x(), 0, 'f', 6 )
		.arg( value.y(), 0, 'f', 6 )
		.arg( value.z(), 0, 'f', 6 );
}

static QString Experimental_exportEntityJson( Entity& entity ){
	QJsonObject object;
	class Visitor final : public Entity::Visitor
	{
		QJsonObject& m_object;
	public:
		explicit Visitor( QJsonObject& object ) : m_object( object ){}
		void visit( const char* key, const char* value ) override {
			if ( value == nullptr || string_empty( value ) ) {
				return;
			}
			if ( string_equal( key, "classname" )
				|| string_equal( key, "origin" )
				|| string_equal( key, "angle" )
				|| string_equal( key, "angles" )
				|| string_equal( key, "modelscale" )
				|| string_equal( key, "modelscale_vec" ) ) {
				return;
			}
			m_object.insert( QString::fromLatin1( key ), QString::fromLatin1( value ) );
		}
	} visitor( object );
	entity.forEachKeyValue( visitor );
	if ( object.isEmpty() ) {
		return {};
	}
	return QString::fromUtf8( QJsonDocument( object ).toJson( QJsonDocument::Compact ) );
}

static void Experimental_restoreEntityJson( Entity& entity, const QString& json ){
	if ( json.isEmpty() ) {
		return;
	}

	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson( json.toUtf8(), &parseError );
	if ( parseError.error != QJsonParseError::NoError || !doc.isObject() ) {
		globalErrorStream() << "failed to parse Maya entity JSON metadata: " << parseError.errorString().toLatin1().constData() << '\n';
		return;
	}

	const QJsonObject object = doc.object();
	for ( auto it = object.begin(); it != object.end(); ++it ) {
		if ( it->isString() ) {
			const QByteArray key = it.key().toUtf8();
			const QByteArray value = it->toString().toUtf8();
			entity.setKeyValue( key.constData(), value.constData() );
		}
	}
}

static Vector3 Experimental_entityTranslation( Entity& entity ){
	Vector3 translation( 0, 0, 0 );
	string_parse_vector3( entity.getKeyValue( "origin" ), translation );
	return translation;
}

static Vector3 Experimental_entityRotation( Entity& entity ){
	Vector3 rotation( 0, 0, 0 );
	if ( string_parse_vector3( entity.getKeyValue( "angles" ), rotation ) ) {
		return rotation;
	}
	const QString angle = QString::fromLatin1( entity.getKeyValue( "angle" ) ).trimmed();
	if ( !angle.isEmpty() ) {
		bool ok = false;
		const double yaw = angle.toDouble( &ok );
		if ( ok ) {
			rotation.y() = yaw;
		}
	}
	return rotation;
}

static Vector3 Experimental_entityScale( Entity& entity ){
	Vector3 scale( 1, 1, 1 );
	if ( string_parse_vector3( entity.getKeyValue( "modelscale_vec" ), scale ) ) {
		return scale;
	}
	const QString scalar = QString::fromLatin1( entity.getKeyValue( "modelscale" ) ).trimmed();
	if ( !scalar.isEmpty() ) {
		bool ok = false;
		const double value = scalar.toDouble( &ok );
		if ( ok ) {
			scale = Vector3( value, value, value );
		}
	}
	return scale;
}

static QString Experimental_exportSceneMetadataJson(){
	QJsonObject object;
	object.insert( "schemaVersion", 2 );
	object.insert( "projectRoot", QString::fromLatin1( GameToolsPath_get() ) );
	object.insert( "mapPath", Experimental_currentMapPath() );
	object.insert( "cameraBookmarks", Experimental_buildCameraBookmarksArray() );
	return QString::fromUtf8( QJsonDocument( object ).toJson( QJsonDocument::Compact ) );
}

static void Experimental_restoreSceneMetadataJson( const QString& json ){
	if ( json.isEmpty() ) {
		return;
	}

	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson( json.toUtf8(), &parseError );
	if ( parseError.error != QJsonParseError::NoError || !doc.isObject() ) {
		globalErrorStream() << "failed to parse Maya scene metadata: " << parseError.errorString().toLatin1().constData() << '\n';
		return;
	}

	Experimental_restoreCameraBookmarks( doc.object().value( "cameraBookmarks" ) );
}

static QString Experimental_exportPatchJson( scene::Node& node, const Vector3& pivot ){
	const PatchControlMatrix controlPoints = GlobalPatchCreator().Patch_getControlPoints( node );
	QJsonObject object;
	object.insert( "schemaVersion", 1 );
	object.insert( "width", static_cast<int>( controlPoints.y() ) );
	object.insert( "height", static_cast<int>( controlPoints.x() ) );

	QJsonArray points;
	for ( std::size_t row = 0; row < controlPoints.x(); ++row ) {
		for ( std::size_t col = 0; col < controlPoints.y(); ++col ) {
			const PatchControl& control = controlPoints( row, col );
			QJsonObject point;
			point.insert( "vertex", Experimental_vector3ToJson( control.m_vertex - pivot ) );
			QJsonArray uv;
			uv.append( control.m_texcoord.x() );
			uv.append( control.m_texcoord.y() );
			point.insert( "uv", uv );
			points.append( point );
		}
	}

	object.insert( "points", points );
	return QString::fromUtf8( QJsonDocument( object ).toJson( QJsonDocument::Compact ) );
}

static bool Experimental_restorePatchJson( scene::Node& node, const QString& json, const MayaResolvedTransform& transform ){
	if ( json.isEmpty() ) {
		return false;
	}

	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson( json.toUtf8(), &parseError );
	if ( parseError.error != QJsonParseError::NoError || !doc.isObject() ) {
		globalErrorStream() << "failed to parse Maya patch metadata: " << parseError.errorString().toLatin1().constData() << '\n';
		return false;
	}

	const QJsonObject object = doc.object();
	const int width = object.value( "width" ).toInt();
	const int height = object.value( "height" ).toInt();
	const QJsonArray points = object.value( "points" ).toArray();
	if ( width < 3 || height < 3 || points.size() != width * height ) {
		return false;
	}

	struct PatchPointData
	{
		Vector3 vertex;
		Vector2 uv;
	};

	QVector<PatchPointData> parsedPoints;
	parsedPoints.reserve( points.size() );

	for ( const QJsonValue& value : points ) {
		if ( !value.isObject() ) {
			return false;
		}
		const QJsonObject point = value.toObject();
		Vector3 vertex;
		if ( !Experimental_tryParseVector3( point.value( "vertex" ), vertex ) ) {
			return false;
		}
		const QJsonArray uvArray = point.value( "uv" ).toArray();
		if ( uvArray.size() < 2 ) {
			return false;
		}

		parsedPoints.push_back( {
			vertex,
			Vector2( uvArray[0].toDouble(), uvArray[1].toDouble() )
		} );
	}

	Matrix4 localToWorld( g_matrix4_identity );
	matrix4_transform_by_euler_xyz_degrees( localToWorld, transform.translation, transform.rotation, transform.scale );

	GlobalPatchCreator().Patch_resize( node, static_cast<std::size_t>( width ), static_cast<std::size_t>( height ) );
	PatchControlMatrix controlPoints = GlobalPatchCreator().Patch_getControlPoints( node );
	if ( controlPoints.x() != static_cast<std::size_t>( height ) || controlPoints.y() != static_cast<std::size_t>( width ) ) {
		return false;
	}

	int index = 0;
	for ( int row = 0; row < height; ++row ) {
		for ( int col = 0; col < width; ++col, ++index ) {
			PatchControl& control = controlPoints( static_cast<std::size_t>( row ), static_cast<std::size_t>( col ) );
			control.m_vertex = parsedPoints[index].vertex;
			matrix4_transform_point( localToWorld, control.m_vertex );
			control.m_texcoord = parsedPoints[index].uv;
		}
	}

	GlobalPatchCreator().Patch_controlPointsChanged( node );
	return true;
}

static QString Experimental_entityDisplayName( Entity& entity, int index ){
	const char* name = entity.getKeyValue( "name" );
	if ( name != nullptr && !string_empty( name ) ) {
		return QString::fromLatin1( name );
	}
	const char* targetname = entity.getKeyValue( "targetname" );
	if ( targetname != nullptr && !string_empty( targetname ) ) {
		return QString::fromLatin1( targetname );
	}
	const char* classname = entity.getClassName();
	if ( string_equal( classname, "worldspawn" ) ) {
		return "World";
	}
	return QString( "%1_%2" ).arg( usdSanitizeName( classname ) ).arg( index );
}

static void Experimental_collectFirstBrushShader( QString& shader, const _QERFaceData& face ){
	if ( shader.isEmpty() && face.m_shader != nullptr && !string_empty( face.m_shader ) ) {
		shader = QString::fromLatin1( face.m_shader );
	}
}
typedef ReferenceCaller<QString, void(const _QERFaceData&), Experimental_collectFirstBrushShader> ExperimentalCollectFirstBrushShaderCaller;

static QString Experimental_firstBrushShader( scene::Node& node ){
	QString shader;
	GlobalBrushCreator().Brush_forEachFace( node, BrushFaceDataCallback( ExperimentalCollectFirstBrushShaderCaller( shader ) ) );
	return shader.isEmpty() ? QString::fromLatin1( texdef_name_default() ) : shader;
}

static int Experimental_normalisePatchSpan( int value ){
	value = std::max( 3, value );
	if ( ( value % 2 ) == 0 ) {
		++value;
	}
	return value;
}

static VIEWTYPE Experimental_smallestAxisToViewType( const Vector3& size ){
	if ( size.x() <= size.y() && size.x() <= size.z() ) {
		return YZ;
	}
	if ( size.y() <= size.x() && size.y() <= size.z() ) {
		return XZ;
	}
	return XY;
}

static void Experimental_writeMayaStringAttr( QTextStream& out, const QString& indent, const char* name, const QString& value ){
	out << indent << "addAttr -ln \"" << name << "\" -dt \"string\";\n";
	out << indent << "setAttr \"."
	    << name << "\" -type \"string\" \"" << Experimental_escapeMayaString( value ) << "\";\n";
}

static void Experimental_writeMayaDoubleAttr( QTextStream& out, const QString& indent, const char* name, double value ){
	out << indent << "addAttr -ln \"" << name << "\" -at \"double\";\n";
	out << indent << "setAttr \"."
	    << name << "\" " << QString::number( value, 'f', 6 ) << ";\n";
}

static void Experimental_writeMayaIntAttr( QTextStream& out, const QString& indent, const char* name, int value ){
	out << indent << "addAttr -ln \"" << name << "\" -at \"long\";\n";
	out << indent << "setAttr \"."
	    << name << "\" " << value << ";\n";
}

static void Experimental_writeMayaTransformAttrs( QTextStream& out, const QString& indent, const Vector3& translation, const Vector3& rotation, const Vector3& scale ){
	if ( !Experimental_vectorNearlyEqual( translation, Vector3( 0, 0, 0 ) ) ) {
		out << indent << "setAttr \".t\" -type \"double3\" " << Experimental_vectorToMayaString( translation ) << ";\n";
	}
	if ( !Experimental_vectorNearlyEqual( rotation, Vector3( 0, 0, 0 ) ) ) {
		out << indent << "setAttr \".r\" -type \"double3\" " << Experimental_vectorToMayaString( rotation ) << ";\n";
	}
	if ( !Experimental_vectorNearlyEqual( scale, Vector3( 1, 1, 1 ) ) ) {
		out << indent << "setAttr \".s\" -type \"double3\" " << Experimental_vectorToMayaString( scale ) << ";\n";
	}
}

static bool Experimental_parseMayaAsciiFile( QTextStream& stream, QVector<MayaAsciiNode>& nodes, QVector<ExperimentalSceneHierarchyEntry>& hierarchy, QString& error ){
	nodes.clear();
	hierarchy.clear();

	QHash<QString, int> transformIndices;
	QHash<QString, QString> meshParents;
	QHash<QString, MayaAsciiCreator> creators;

	const QRegularExpression createTypeRegex( "^createNode\\s+(\\w+)" );
	const QRegularExpression flagNameRegex( "-n\\s+\"([^\"]+)\"" );
	const QRegularExpression flagParentRegex( "-p\\s+\"([^\"]+)\"" );
	const QRegularExpression setAttrNameRegex( "^setAttr\\s+\"\\.([^\"]+)\"" );
	const QRegularExpression stringValueRegex( "-type\\s+\"string\"\\s+\"((?:\\\\.|[^\"])*)\"" );
	const QRegularExpression double3Regex( "-type\\s+\"double3\"\\s+([^\\s;]+)\\s+([^\\s;]+)\\s+([^\\s;]+)" );
	const QRegularExpression singleNumberRegex( "^setAttr\\s+\"\\.[^\"]+\"\\s+([^\\s;]+)" );
	const QRegularExpression connectRegex( "^connectAttr\\s+\"([^\"]+)\\.out\"\\s+\"([^\"]+)\\.i\"" );

	auto parseDouble = []( const QString& token, double& value ) -> bool {
		bool ok = false;
		value = token.toDouble( &ok );
		return ok;
	};

	auto assignCreatorToTransform = [&]( const QString& creatorName, const QString& meshName ){
		const auto creatorIt = creators.constFind( creatorName );
		const auto meshParentIt = meshParents.constFind( meshName );
		if ( creatorIt == creators.constEnd() || meshParentIt == meshParents.constEnd() ) {
			return;
		}
		const auto transformIt = transformIndices.constFind( meshParentIt.value() );
		if ( transformIt == transformIndices.constEnd() ) {
			return;
		}

		MayaAsciiNode& node = nodes[transformIt.value()];
		if ( creatorIt->type == "polyCube" ) {
			node.size = Vector3( creatorIt->width, creatorIt->height, creatorIt->depth );
			node.hasGeometry = true;
			if ( node.radiantNodeType.isEmpty() ) {
				node.radiantNodeType = "brush";
			}
		}
		else if ( creatorIt->type == "polyPlane" ) {
			node.size = Vector3( creatorIt->width, 1, creatorIt->height );
			node.patchWidth = Experimental_normalisePatchSpan( creatorIt->subdivX + 1 );
			node.patchHeight = Experimental_normalisePatchSpan( creatorIt->subdivY + 1 );
			node.hasGeometry = true;
			if ( node.radiantNodeType.isEmpty() ) {
				node.radiantNodeType = "patch";
			}
		}
	};

	QString currentNodeName;
	QString currentNodeType;

	while ( !stream.atEnd() )
	{
		const QString line = stream.readLine().trimmed();
		if ( line.isEmpty() || line.startsWith( "//" ) ) {
			continue;
		}

		const auto createTypeMatch = createTypeRegex.match( line );
		if ( createTypeMatch.hasMatch() ) {
			currentNodeType = createTypeMatch.captured( 1 );
			const auto nameMatch = flagNameRegex.match( line );
			currentNodeName = nameMatch.hasMatch() ? nameMatch.captured( 1 ) : QString();
			const auto parentMatch = flagParentRegex.match( line );
			const QString parentName = parentMatch.hasMatch() ? parentMatch.captured( 1 ) : QString();

			if ( currentNodeType == "transform" && !currentNodeName.isEmpty() ) {
				MayaAsciiNode node;
				node.name = currentNodeName;
				node.parent = parentName;
				nodes.push_back( node );
				transformIndices.insert( currentNodeName, nodes.size() - 1 );
			}
			else if ( currentNodeType == "mesh" && !currentNodeName.isEmpty() ) {
				meshParents.insert( currentNodeName, parentName );
			}
			else if ( ( currentNodeType == "polyCube" || currentNodeType == "polyPlane" ) && !currentNodeName.isEmpty() ) {
				MayaAsciiCreator creator;
				creator.type = currentNodeType;
				creators.insert( currentNodeName, creator );
			}
			continue;
		}

		const auto connectMatch = connectRegex.match( line );
		if ( connectMatch.hasMatch() ) {
			assignCreatorToTransform( connectMatch.captured( 1 ), connectMatch.captured( 2 ) );
			continue;
		}

		if ( currentNodeName.isEmpty() || !line.startsWith( "setAttr " ) ) {
			continue;
		}

		const auto nameMatch = setAttrNameRegex.match( line );
		if ( !nameMatch.hasMatch() ) {
			continue;
		}
		const QString attrName = nameMatch.captured( 1 );

		if ( currentNodeType == "transform" ) {
			const auto transformIt = transformIndices.find( currentNodeName );
			if ( transformIt == transformIndices.end() ) {
				continue;
			}
			MayaAsciiNode& node = nodes[transformIt.value()];

			const auto double3Match = double3Regex.match( line );
			if ( double3Match.hasMatch() ) {
				double x = 0;
				double y = 0;
				double z = 0;
				if ( parseDouble( double3Match.captured( 1 ), x )
					&& parseDouble( double3Match.captured( 2 ), y )
					&& parseDouble( double3Match.captured( 3 ), z ) ) {
					if ( attrName == "t" ) {
						node.translation = Vector3( x, y, z );
					}
					else if ( attrName == "r" ) {
						node.rotation = Vector3( x, y, z );
					}
					else if ( attrName == "s" ) {
						node.scale = Vector3( x, y, z );
					}
				}
				continue;
			}

			const auto stringMatch = stringValueRegex.match( line );
			if ( stringMatch.hasMatch() ) {
				const QString value = Experimental_unescapeMayaString( stringMatch.captured( 1 ) );
				if ( attrName == "radiantClassname" ) {
					node.radiantClassname = value;
				}
				else if ( attrName == "radiantNodeType" ) {
					node.radiantNodeType = value;
				}
				else if ( attrName == "radiantShader" ) {
					node.radiantShader = value;
				}
				else if ( attrName == "radiantEntityJson" ) {
					node.radiantEntityJson = value;
				}
				else if ( attrName == "radiantSceneJson" ) {
					node.radiantSceneJson = value;
				}
				else if ( attrName == "radiantPatchJson" ) {
					node.radiantPatchJson = value;
				}
				continue;
			}

			const auto singleNumberMatch = singleNumberRegex.match( line );
			if ( singleNumberMatch.hasMatch() ) {
				double value = 0;
				if ( parseDouble( singleNumberMatch.captured( 1 ), value ) ) {
					if ( attrName == "radiantWidth" ) {
						node.size.x() = value;
						node.hasGeometry = true;
					}
					else if ( attrName == "radiantHeight" ) {
						node.size.y() = value;
						node.hasGeometry = true;
					}
					else if ( attrName == "radiantDepth" ) {
						node.size.z() = value;
						node.hasGeometry = true;
					}
					else if ( attrName == "radiantSubdivX" ) {
						node.patchWidth = Experimental_normalisePatchSpan( static_cast<int>( value ) );
					}
					else if ( attrName == "radiantSubdivY" ) {
						node.patchHeight = Experimental_normalisePatchSpan( static_cast<int>( value ) );
					}
				}
			}
		}
		else if ( currentNodeType == "polyCube" || currentNodeType == "polyPlane" ) {
			auto creatorIt = creators.find( currentNodeName );
			if ( creatorIt == creators.end() ) {
				continue;
			}
			const auto singleNumberMatch = singleNumberRegex.match( line );
			if ( !singleNumberMatch.hasMatch() ) {
				continue;
			}
			double value = 0;
			if ( !parseDouble( singleNumberMatch.captured( 1 ), value ) ) {
				continue;
			}
			if ( attrName == "w" ) {
				creatorIt->width = value;
			}
			else if ( attrName == "h" ) {
				creatorIt->height = value;
			}
			else if ( attrName == "d" ) {
				creatorIt->depth = value;
			}
			else if ( attrName == "sx" ) {
				creatorIt->subdivX = std::max( 1, static_cast<int>( value ) );
			}
			else if ( attrName == "sy" ) {
				creatorIt->subdivY = std::max( 1, static_cast<int>( value ) );
			}
		}
	}

	for ( auto it = meshParents.constBegin(); it != meshParents.constEnd(); ++it ) {
		const QString meshName = it.key();
		for ( auto creatorIt = creators.constBegin(); creatorIt != creators.constEnd(); ++creatorIt ) {
			assignCreatorToTransform( creatorIt.key(), meshName );
		}
	}

	if ( nodes.isEmpty() ) {
		error = "No Maya transform nodes were detected.";
		return false;
	}

	for ( const auto& node : nodes ) {
		QString type = node.radiantNodeType;
		if ( type.isEmpty() ) {
			type = node.radiantClassname.isEmpty() ? "Transform" : node.radiantClassname;
		}
		hierarchy.push_back( { node.name, node.parent, type } );
	}

	return true;
}

static MayaResolvedTransform Experimental_resolveMayaTransform(
	const QVector<MayaAsciiNode>& nodes,
	const QHash<QString, int>& indices,
	int index,
	QHash<int, MayaResolvedTransform>& cache,
	QSet<int>& active ){
	if ( cache.contains( index ) ) {
		return cache.value( index );
	}
	if ( active.contains( index ) ) {
		return MayaResolvedTransform();
	}
	active.insert( index );

	const MayaAsciiNode& node = nodes[index];
	MayaResolvedTransform result;
	result.translation = node.translation;
	result.rotation = node.rotation;
	result.scale = node.scale;

	const auto parentIt = indices.constFind( node.parent );
	if ( parentIt != indices.constEnd() ) {
		const MayaResolvedTransform parent = Experimental_resolveMayaTransform( nodes, indices, parentIt.value(), cache, active );
		result.translation += parent.translation;
		result.rotation += parent.rotation;
		result.scale = Experimental_componentMultiply( parent.scale, result.scale );
	}

	active.remove( index );
	cache.insert( index, result );
	return result;
}

static scene::Node& Experimental_mayaParentForGeometry( const QString& parentName, const QHash<QString, scene::Node*>& groupNodes, scene::Node& worldspawn ){
	const auto it = groupNodes.find( parentName );
	return it != groupNodes.end() && it.value() != nullptr ? *it.value() : worldspawn;
}

void Experimental_importMayaASCII_impl(){
	if ( !Map_Valid( g_map ) ) {
		return;
	}

	const auto filename = QFileDialog::getOpenFileName( MainFrame_getWindow(), "Import Maya ASCII", "", "Maya ASCII Files (*.ma)" );
	if ( filename.isEmpty() ) {
		return;
	}

	QFile file( filename );
	if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		globalErrorStream() << "failed to open Maya ASCII file: " << filename.toLatin1().constData() << '\n';
		return;
	}

	QTextStream stream( &file );
	QVector<MayaAsciiNode> nodes;
	QVector<ExperimentalSceneHierarchyEntry> hierarchy;
	QString parseError;
	if ( !Experimental_parseMayaAsciiFile( stream, nodes, hierarchy, parseError ) ) {
		globalErrorStream() << "failed to parse Maya ASCII file: " << parseError.toLatin1().constData() << '\n';
		return;
	}

	Experimental_populateSceneHierarchyTree( hierarchy );

	QHash<QString, int> indices;
	QHash<QString, int> childCounts;
	for ( int i = 0; i < nodes.size(); ++i ) {
		indices.insert( nodes[i].name, i );
		if ( !nodes[i].parent.isEmpty() ) {
			childCounts[nodes[i].parent] += 1;
		}
	}

	UndoableCommand undo( "importMayaAscii" );
	scene::Node& worldspawn = Map_FindOrInsertWorldspawn( g_map );
	QHash<QString, scene::Node*> groupNodes;
	groupNodes.insert( "worldspawn", &worldspawn );

	QHash<int, MayaResolvedTransform> resolvedCache;
	QSet<int> resolving;
	int entityCount = 0;
	int brushCount = 0;
	int patchCount = 0;

	GlobalSelectionSystem().setSelectedAll( false );

	for ( int i = 0; i < nodes.size(); ++i ) {
		const MayaAsciiNode& node = nodes[i];
		if ( node.radiantNodeType == "scene" ) {
			Experimental_restoreSceneMetadataJson( node.radiantSceneJson );
			continue;
		}

		const MayaResolvedTransform resolved = Experimental_resolveMayaTransform( nodes, indices, i, resolvedCache, resolving );
		const bool isGenericGroup = node.radiantClassname.isEmpty() && !node.hasGeometry && childCounts.value( node.name ) > 0;
		QString classname = node.radiantClassname;
		if ( classname.isEmpty() && isGenericGroup ) {
			classname = "func_group";
		}

		if ( classname == "worldspawn" ) {
			groupNodes.insert( node.name, &worldspawn );
			continue;
		}

		if ( !classname.isEmpty() ) {
			const bool isGroupEntity = classname == "func_group";
			EntityClass* eclass = GlobalEntityClassManager().findOrInsert( classname.toLatin1().constData(), isGroupEntity );
			if ( eclass->unknown ) {
				if ( isGroupEntity ) {
					eclass = GlobalEntityClassManager().findOrInsert( "func_group", true );
				}
				else{
					globalErrorStream() << "unknown Maya entity class: " << classname.toLatin1().constData() << '\n';
					continue;
				}
			}

			NodeSmartReference entityNode( GlobalEntityCreator().createEntity( eclass ) );
			Node_getTraversable( GlobalSceneGraph().root() )->insert( entityNode );
			Entity* entity = Node_getEntity( entityNode );
			if ( entity == nullptr ) {
				continue;
			}

			if ( !node.name.isEmpty() ) {
				const QByteArray nameUtf8 = node.name.toUtf8();
				entity->setKeyValue( "name", nameUtf8.constData() );
			}
			if ( !node.parent.isEmpty() && node.parent != "RadiantScene" ) {
				const QByteArray parentUtf8 = node.parent.toUtf8();
				entity->setKeyValue( "mayaParent", parentUtf8.constData() );
			}
			Experimental_restoreEntityJson( *entity, node.radiantEntityJson );

			if ( eclass->fixedsize ) {
				scene::Path entityPath( makeReference( GlobalSceneGraph().root() ) );
				entityPath.push( makeReference( entityNode.get() ) );
				if ( scene::Instance* instance = GlobalSceneGraph().find( entityPath ) ) {
					if ( Transformable* transform = Instance_getTransformable( *instance ) ) {
						transform->setType( TRANSFORM_PRIMITIVE );
						transform->setTranslation( resolved.translation );
						transform->freezeTransform();
					}
				}

				if ( !Experimental_vectorNearlyEqual( resolved.rotation, Vector3( 0, 0, 0 ) ) ) {
					const QByteArray rotationUtf8 = Experimental_vectorToMayaString( resolved.rotation ).toUtf8();
					entity->setKeyValue( "angles", rotationUtf8.constData() );
				}
				if ( !Experimental_vectorNearlyEqual( resolved.scale, Vector3( 1, 1, 1 ) ) ) {
					if ( Experimental_vectorNearlyEqual( resolved.scale, Vector3( resolved.scale.x(), resolved.scale.x(), resolved.scale.x() ) ) ) {
						const QByteArray scaleUtf8 = QString::number( resolved.scale.x(), 'f', 6 ).toUtf8();
						entity->setKeyValue( "modelscale", scaleUtf8.constData() );
					}
					else{
						const QByteArray scaleUtf8 = Experimental_vectorToMayaString( resolved.scale ).toUtf8();
						entity->setKeyValue( "modelscale_vec", scaleUtf8.constData() );
					}
				}
			}

			groupNodes.insert( node.name, entityNode.get_pointer() );
			++entityCount;
			continue;
		}

		if ( !node.hasGeometry && node.radiantNodeType.isEmpty() ) {
			continue;
		}

		Vector3 scaledSize = Experimental_componentAbs( Experimental_componentMultiply( node.size, resolved.scale ) );
		scaledSize.x() = std::max<float>( 1.0f, scaledSize.x() );
		scaledSize.y() = std::max<float>( 1.0f, scaledSize.y() );
		scaledSize.z() = std::max<float>( 1.0f, scaledSize.z() );

		scene::Node& parentNode = Experimental_mayaParentForGeometry( node.parent, groupNodes, worldspawn );
		const QByteArray shaderUtf8 = ( node.radiantShader.isEmpty() ? QString::fromLatin1( texdef_name_default() ) : node.radiantShader ).toUtf8();
		const AABB bounds( resolved.translation, scaledSize * 0.5f );

		if ( node.radiantNodeType == "patch" ) {
			NodeSmartReference patchNode( GlobalPatchCreator().createPatch() );
			Node_getTraversable( parentNode )->insert( patchNode );
			Patch* patch = Node_getPatch( patchNode );
			if ( patch != nullptr ) {
				patch->SetShader( shaderUtf8.constData() );
				if ( !Experimental_restorePatchJson( patchNode.get(), node.radiantPatchJson, resolved ) ) {
					patch->ConstructPrefab(
						bounds,
						EPatchPrefab::Plane,
						Experimental_smallestAxisToViewType( scaledSize ),
						Experimental_normalisePatchSpan( node.patchWidth ),
						Experimental_normalisePatchSpan( node.patchHeight ) );
					patch->controlPointsChanged();
				}
				++patchCount;
			}
			continue;
		}

		scene::Node* brushNode = &GlobalBrushCreator().createBrush();
		Node_getTraversable( parentNode )->insert( NodeSmartReference( *brushNode ) );
		if ( Brush* brush = Node_getBrush( *brushNode ) ) {
			Brush_ConstructCuboid( *brush, bounds, shaderUtf8.constData(), TextureTransform_getDefault() );
			++brushCount;
		}
	}

	SceneChangeNotify();
	Sys_Status( QString( "Imported Maya ASCII: %1 entities, %2 brushes, %3 patches" )
		.arg( entityCount )
		.arg( brushCount )
		.arg( patchCount )
		.toUtf8().constData() );
}

void Experimental_exportToMayaASCII_impl(){
	if ( !Map_Valid( g_map ) ) {
		return;
	}

	const auto filename = QFileDialog::getSaveFileName( MainFrame_getWindow(), "Export to Maya ASCII", "", "Maya ASCII Files (*.ma)" );
	if ( filename.isEmpty() ) {
		return;
	}

	QFile file( filename );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		globalErrorStream() << "failed to open Maya ASCII file for write: " << filename.toLatin1().constData() << '\n';
		return;
	}

	QTextStream out( &file );
	out << "//Maya ASCII 2024 scene\n";
	out << "requires maya \"2024\";\n";
	out << "currentUnit -l centimeter -a degree -t film;\n";
	out << "fileInfo \"application\" \"idTech3Radiant\";\n";
	out << "fileInfo \"product\" \"Experimental Maya ASCII Export\";\n\n";
	out << "createNode transform -n \"RadiantScene\";\n";
	Experimental_writeMayaStringAttr( out, "    ", "radiantNodeType", "scene" );
	Experimental_writeMayaStringAttr( out, "    ", "radiantSceneJson", Experimental_exportSceneMetadataJson() );
	out << '\n';

	class MayaExportWalker final : public scene::Graph::Walker
	{
		QTextStream& m_out;
		mutable QList<QString> m_entityStack;
		mutable QHash<QString, QString> m_namedEntities;
		mutable QSet<QString> m_usedNames;
		mutable QVector<ExperimentalSceneHierarchyEntry> m_hierarchy;
		mutable int m_entityIndex = 0;
		mutable int m_brushIndex = 0;
		mutable int m_patchIndex = 0;
	public:
		explicit MayaExportWalker( QTextStream& out ) : m_out( out ){}

		const QVector<ExperimentalSceneHierarchyEntry>& hierarchy() const {
			return m_hierarchy;
		}

		bool pre( const scene::Path& path, scene::Instance& instance ) const override {
			scene::Node& node = path.top();
			if ( Entity* entity = Node_getEntity( node ) ) {
				const QString exportName = Experimental_makeUniqueMayaName( Experimental_entityDisplayName( *entity, m_entityIndex++ ), m_usedNames );
				QString parentName = "RadiantScene";
				const QString mayaParent = QString::fromLatin1( entity->getKeyValue( "mayaParent" ) ).trimmed();
				if ( !mayaParent.isEmpty() && m_namedEntities.contains( mayaParent ) ) {
					parentName = m_namedEntities.value( mayaParent );
				}

				m_out << "createNode transform -n \"" << exportName << "\" -p \"" << parentName << "\";\n";
				Experimental_writeMayaStringAttr( m_out, "    ", "radiantNodeType", "entity" );
				Experimental_writeMayaStringAttr( m_out, "    ", "radiantClassname", QString::fromLatin1( entity->getClassName() ) );
				const QString entityJson = Experimental_exportEntityJson( *entity );
				if ( !entityJson.isEmpty() ) {
					Experimental_writeMayaStringAttr( m_out, "    ", "radiantEntityJson", entityJson );
				}
				Experimental_writeMayaTransformAttrs(
					m_out,
					"    ",
					Experimental_entityTranslation( *entity ),
					Experimental_entityRotation( *entity ),
					Experimental_entityScale( *entity ) );
				m_out << '\n';

				const QString logicalName = QString::fromLatin1( entity->getKeyValue( "name" ) ).trimmed();
				if ( !logicalName.isEmpty() ) {
					m_namedEntities.insert( logicalName, exportName );
				}
				m_entityStack.push_back( exportName );
				m_hierarchy.push_back( { exportName, parentName, QString::fromLatin1( entity->getClassName() ) } );
				return true;
			}

			const QString parentName = m_entityStack.isEmpty() ? QString( "RadiantScene" ) : m_entityStack.back();
			if ( Node_getBrush( node ) != nullptr ) {
				const QString exportName = Experimental_makeUniqueMayaName( QString( "brush_%1" ).arg( m_brushIndex++ ), m_usedNames );
				const QString polyName = exportName + "PolyCube";
				const QString shapeName = exportName + "Shape";
				const AABB bounds = instance.worldAABB();
				const Vector3 size = bounds.extents * 2;
				const QString shader = Experimental_firstBrushShader( node );

				m_out << "createNode transform -n \"" << exportName << "\" -p \"" << parentName << "\";\n";
				Experimental_writeMayaStringAttr( m_out, "    ", "radiantNodeType", "brush" );
				Experimental_writeMayaStringAttr( m_out, "    ", "radiantShader", shader );
				Experimental_writeMayaDoubleAttr( m_out, "    ", "radiantWidth", size.x() );
				Experimental_writeMayaDoubleAttr( m_out, "    ", "radiantHeight", size.y() );
				Experimental_writeMayaDoubleAttr( m_out, "    ", "radiantDepth", size.z() );
				Experimental_writeMayaTransformAttrs( m_out, "    ", bounds.origin, Vector3( 0, 0, 0 ), Vector3( 1, 1, 1 ) );
				m_out << "createNode mesh -n \"" << shapeName << "\" -p \"" << exportName << "\";\n";
				m_out << "createNode polyCube -n \"" << polyName << "\";\n";
				m_out << "    setAttr \".w\" " << QString::number( size.x(), 'f', 6 ) << ";\n";
				m_out << "    setAttr \".h\" " << QString::number( size.y(), 'f', 6 ) << ";\n";
				m_out << "    setAttr \".d\" " << QString::number( size.z(), 'f', 6 ) << ";\n";
				m_out << "connectAttr \"" << polyName << ".out\" \"" << shapeName << ".i\";\n\n";

				m_hierarchy.push_back( { exportName, parentName, "Brush" } );
				return false;
			}

			if ( Patch* patch = Node_getPatch( node ) ) {
				const QString exportName = Experimental_makeUniqueMayaName( QString( "patch_%1" ).arg( m_patchIndex++ ), m_usedNames );
				const QString polyName = exportName + "PolyPlane";
				const QString shapeName = exportName + "Shape";
				const AABB bounds = instance.worldAABB();
				const Vector3 size = bounds.extents * 2;

				m_out << "createNode transform -n \"" << exportName << "\" -p \"" << parentName << "\";\n";
				Experimental_writeMayaStringAttr( m_out, "    ", "radiantNodeType", "patch" );
				Experimental_writeMayaStringAttr( m_out, "    ", "radiantShader", QString::fromLatin1( patch->GetShader() ) );
				Experimental_writeMayaDoubleAttr( m_out, "    ", "radiantWidth", size.x() );
				Experimental_writeMayaDoubleAttr( m_out, "    ", "radiantHeight", size.y() );
				Experimental_writeMayaDoubleAttr( m_out, "    ", "radiantDepth", size.z() );
				Experimental_writeMayaIntAttr( m_out, "    ", "radiantSubdivX", static_cast<int>( patch->getWidth() ) );
				Experimental_writeMayaIntAttr( m_out, "    ", "radiantSubdivY", static_cast<int>( patch->getHeight() ) );
				Experimental_writeMayaStringAttr( m_out, "    ", "radiantPatchJson", Experimental_exportPatchJson( node, bounds.origin ) );
				Experimental_writeMayaTransformAttrs( m_out, "    ", bounds.origin, Vector3( 0, 0, 0 ), Vector3( 1, 1, 1 ) );
				m_out << "createNode mesh -n \"" << shapeName << "\" -p \"" << exportName << "\";\n";
				m_out << "createNode polyPlane -n \"" << polyName << "\";\n";
				m_out << "    setAttr \".w\" " << QString::number( size.x(), 'f', 6 ) << ";\n";
				m_out << "    setAttr \".h\" " << QString::number( size.z(), 'f', 6 ) << ";\n";
				m_out << "    setAttr \".sx\" " << std::max( 1, static_cast<int>( patch->getWidth() ) - 1 ) << ";\n";
				m_out << "    setAttr \".sy\" " << std::max( 1, static_cast<int>( patch->getHeight() ) - 1 ) << ";\n";
				m_out << "connectAttr \"" << polyName << ".out\" \"" << shapeName << ".i\";\n\n";

				m_hierarchy.push_back( { exportName, parentName, "Patch" } );
				return false;
			}

			return true;
		}

		void post( const scene::Path& path, scene::Instance& instance ) const override {
			if ( Node_getEntity( path.top() ) != nullptr && !m_entityStack.isEmpty() ) {
				m_entityStack.removeLast();
			}
		}
	} walker( out );

	GlobalSceneGraph().traverse( walker );
	Experimental_populateSceneHierarchyTree( walker.hierarchy() );
	Sys_Status( "Exported map to Maya ASCII" );
}

void Experimental_exportToUSDA_impl(){
	if ( !Map_Valid( g_map ) ) {
		return;
	}

	const auto filename = QFileDialog::getSaveFileName( MainFrame_getWindow(), "Export to USDA", "", "USDA Files (*.usda)" );
	if ( filename.isEmpty() ) {
		return;
	}

	QFile file( filename );
	if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		globalErrorStream() << "failed to open USDA file for write: " << filename.toLatin1().constData() << '\n';
		return;
	}

	QTextStream out( &file );
	out << "#usda 1.0\n";
	out << "(\n";
	out << "    doc = \"Exported from Radiant map\"\n";
	out << ")\n\n";

	class USDAExportWalker final : public scene::Traversable::Walker
	{
		QTextStream& m_out;
		mutable QList<QString> m_stack;
		mutable int m_entityIndex{};
	public:
		explicit USDAExportWalker( QTextStream& out ) : m_out( out ){}
		bool pre( scene::Node& node ) const override {
			Entity* entity = Node_getEntity( node );
			if ( entity != nullptr ) {
				const char* classname = entity->getClassName();
				const bool isWorld = string_equal( classname, "worldspawn" );
				QString primName = isWorld ? "World" : ( usdSanitizeName( classname ) + "_" + QString::number( m_entityIndex++ ) );
				QString indent( m_stack.size() * 4, ' ' );
				m_out << indent << "def Xform \"" << primName << "\" (\n";
				m_out << indent << "    custom string classname = \"" << QString( classname ).replace( '"', "\\\"" ) << "\"\n";
				const char* origin = entity->getKeyValue( "origin" );
				if ( origin != nullptr && !string_empty( origin ) ) {
					m_out << indent << "    custom string origin = \"" << QString( origin ).replace( '"', "\\\"" ) << "\"\n";
				}
				class KeyValueVisitor final : public Entity::Visitor
				{
					QTextStream& m_out;
					QString m_indent;
				public:
					KeyValueVisitor( QTextStream& o, const QString& ind ) : m_out( o ), m_indent( ind ){}
					void visit( const char* key, const char* value ) override {
						if ( string_equal( key, "classname" ) || string_equal( key, "origin" ) ) return;
						if ( string_empty( value ) ) return;
						m_out << m_indent << "    custom string " << usdSanitizeName( key ).toLatin1().constData() << " = \"" << QString( value ).replace( '"', "\\\"" ) << "\"\n";
					}
				} kvVisitor( m_out, indent );
				entity->forEachKeyValue( kvVisitor );
				m_out << indent << ")\n";
				m_out << indent << "{\n";
				m_stack.push_back( indent );
				return true;
			}
			return true;
		}
		void post( scene::Node& node ) const override {
			Entity* entity = Node_getEntity( node );
			if ( entity != nullptr && !m_stack.isEmpty() ) {
				QString indent = m_stack.takeLast();
				m_out << indent << "}\n";
			}
		}
	} walker( out );

	Map_Traverse( GlobalSceneGraph().root(), walker );

	if ( g_exp_usdDock != nullptr ) {
		g_exp_usdDock->show();
	}
	Sys_Status( "Exported map to USDA" );
}

void Experimental_createDocks( QMainWindow* window ){
	if ( window == nullptr ) {
		return;
	}

	Experimental_setUndoTrackerAttached( true );
	if ( g_exp_liveSyncService == nullptr ) {
		g_exp_liveSyncService = new ExperimentalLiveSyncService( window );
		g_exp_liveSyncService->setAutoSync( QSettings().value( "Properties/Experimental/LiveSyncAutoSync", true ).toBool() );
	}

	g_exp_propertiesDock = new QDockWidget( "Inspector", window );
	g_exp_propertiesDock->setObjectName( "dock_experimental_properties" );
	{
		auto* root = new QWidget( g_exp_propertiesDock );
		auto* vbox = new QVBoxLayout( root );
		auto* form = new QFormLayout();
		g_exp_selectedCountLabel = new QLabel( "0", root );
		g_exp_selectedComponentsLabel = new QLabel( "0", root );
		g_exp_selectionTypeLabel = new QLabel( "Nothing selected", root );
		g_exp_selectionBoundsLabel = new QLabel( "No bounds", root );
		g_exp_selectionShaderLabel = new QLabel( "None", root );
		g_exp_shaderEdit = new QLineEdit( root );
		auto* applyButton = new QPushButton( "Apply Shader", root );
		auto* pullShaderButton = new QPushButton( "Use Selected", root );
		auto* frameButton = new QPushButton( "Frame", root );
		auto* shaderButtons = new QWidget( root );
		auto* shaderButtonsLayout = new QHBoxLayout( shaderButtons );
		shaderButtonsLayout->setContentsMargins( 0, 0, 0, 0 );
		shaderButtonsLayout->addWidget( applyButton );
		shaderButtonsLayout->addWidget( pullShaderButton );
		shaderButtonsLayout->addWidget( frameButton );
		form->addRow( "Selected", g_exp_selectedCountLabel );
		form->addRow( "Selected Components", g_exp_selectedComponentsLabel );
		form->addRow( "Type", g_exp_selectionTypeLabel );
		form->addRow( "Bounds", g_exp_selectionBoundsLabel );
		form->addRow( "Current Material", g_exp_selectionShaderLabel );
		form->addRow( "Shader", g_exp_shaderEdit );
		form->addRow( "", shaderButtons );
		QObject::connect( applyButton, &QPushButton::clicked, [](){ Experimental_applySelectedShader(); } );
		QObject::connect( pullShaderButton, &QPushButton::clicked, [](){ Experimental_pullShaderFromSelection(); } );
		QObject::connect( frameButton, &QPushButton::clicked, [](){ FocusAllViews(); } );
		QObject::connect( g_exp_shaderEdit, &QLineEdit::returnPressed, [](){ Experimental_applySelectedShader(); } );
		vbox->addLayout( form );

		g_exp_entityGroup = new QGroupBox( "Entity", root );
		{
			auto* entityForm = new QFormLayout( g_exp_entityGroup );
			g_exp_entityClassLabel = new QLabel( "None", g_exp_entityGroup );
			g_exp_entityNameEdit = new QLineEdit( g_exp_entityGroup );
			g_exp_entityTargetnameEdit = new QLineEdit( g_exp_entityGroup );
			g_exp_entityTargetEdit = new QLineEdit( g_exp_entityGroup );
			g_exp_entitySoundEdit = new QLineEdit( g_exp_entityGroup );
			g_exp_entityModelEdit = new QLineEdit( g_exp_entityGroup );
			g_exp_entityNameEdit->setClearButtonEnabled( true );
			g_exp_entityTargetnameEdit->setClearButtonEnabled( true );
			g_exp_entityTargetEdit->setClearButtonEnabled( true );
			g_exp_entitySoundEdit->setClearButtonEnabled( true );
			g_exp_entityModelEdit->setClearButtonEnabled( true );
			entityForm->addRow( "Classname", g_exp_entityClassLabel );
			entityForm->addRow( "Name", g_exp_entityNameEdit );
			entityForm->addRow( "Target Name", g_exp_entityTargetnameEdit );
			entityForm->addRow( "Target", g_exp_entityTargetEdit );
			entityForm->addRow( "Sound", g_exp_entitySoundEdit );
			entityForm->addRow( "Model", g_exp_entityModelEdit );
			QObject::connect( g_exp_entityNameEdit, &QLineEdit::editingFinished, [](){ Experimental_setSelectedEntityKeyValue( "name", g_exp_entityNameEdit->text() ); } );
			QObject::connect( g_exp_entityTargetnameEdit, &QLineEdit::editingFinished, [](){ Experimental_setSelectedEntityKeyValue( "targetname", g_exp_entityTargetnameEdit->text() ); } );
			QObject::connect( g_exp_entityTargetEdit, &QLineEdit::editingFinished, [](){ Experimental_setSelectedEntityKeyValue( "target", g_exp_entityTargetEdit->text() ); } );
			QObject::connect( g_exp_entitySoundEdit, &QLineEdit::editingFinished, [](){ Experimental_setSelectedEntityKeyValue( "sound", g_exp_entitySoundEdit->text() ); } );
			QObject::connect( g_exp_entityModelEdit, &QLineEdit::editingFinished, [](){ Experimental_setSelectedEntityKeyValue( "model", g_exp_entityModelEdit->text() ); } );
		}
		vbox->addWidget( g_exp_entityGroup );

		g_exp_worldGroup = new QGroupBox( "World Settings", root );
		{
			auto* worldForm = new QFormLayout( g_exp_worldGroup );
			g_exp_worldMessageEdit = new QLineEdit( g_exp_worldGroup );
			g_exp_worldMusicEdit = new QLineEdit( g_exp_worldGroup );
			g_exp_worldGravityEdit = new QLineEdit( g_exp_worldGroup );
			g_exp_worldColorEdit = new QLineEdit( g_exp_worldGroup );
			g_exp_worldMinlight = new QDoubleSpinBox( g_exp_worldGroup );
			g_exp_worldMessageEdit->setClearButtonEnabled( true );
			g_exp_worldMusicEdit->setClearButtonEnabled( true );
			g_exp_worldGravityEdit->setClearButtonEnabled( true );
			g_exp_worldColorEdit->setClearButtonEnabled( true );
			g_exp_worldMinlight->setRange( 0, 1 );
			g_exp_worldMinlight->setSingleStep( 0.05 );
			g_exp_worldMinlight->setDecimals( 2 );
			worldForm->addRow( "Message", g_exp_worldMessageEdit );
			worldForm->addRow( "Music", g_exp_worldMusicEdit );
			worldForm->addRow( "Gravity", g_exp_worldGravityEdit );
			worldForm->addRow( "Ambient Color", g_exp_worldColorEdit );
			worldForm->addRow( "Min Light", g_exp_worldMinlight );
			QObject::connect( g_exp_worldMessageEdit, &QLineEdit::editingFinished, [](){ Experimental_setWorldspawnKeyValue( "message", g_exp_worldMessageEdit->text() ); } );
			QObject::connect( g_exp_worldMusicEdit, &QLineEdit::editingFinished, [](){ Experimental_setWorldspawnKeyValue( "music", g_exp_worldMusicEdit->text() ); } );
			QObject::connect( g_exp_worldGravityEdit, &QLineEdit::editingFinished, [](){ Experimental_setWorldspawnKeyValue( "gravity", g_exp_worldGravityEdit->text() ); } );
			QObject::connect( g_exp_worldColorEdit, &QLineEdit::editingFinished, [](){ Experimental_setWorldspawnKeyValue( "_color", g_exp_worldColorEdit->text() ); } );
			QObject::connect( g_exp_worldMinlight, &QDoubleSpinBox::editingFinished, [](){ Experimental_setWorldspawnKeyValue( "_minlight", QString::number( g_exp_worldMinlight->value(), 'f', 2 ) ); } );
		}
		vbox->addWidget( g_exp_worldGroup );

		g_exp_gravityGroup = new QGroupBox( "Gravity Volume", root );
		{
			auto* gravityForm = new QFormLayout( g_exp_gravityGroup );
			g_exp_gravityDirectionEdit = new QLineEdit( g_exp_gravityGroup );
			g_exp_gravityMagnitudeEdit = new QLineEdit( g_exp_gravityGroup );
			g_exp_gravityDirectionEdit->setClearButtonEnabled( true );
			g_exp_gravityMagnitudeEdit->setClearButtonEnabled( true );
			gravityForm->addRow( "Direction", g_exp_gravityDirectionEdit );
			gravityForm->addRow( "Magnitude", g_exp_gravityMagnitudeEdit );
			QObject::connect( g_exp_gravityDirectionEdit, &QLineEdit::editingFinished, [](){ Experimental_setSelectedEntityKeyValue( "gravity_dir", g_exp_gravityDirectionEdit->text() ); } );
			QObject::connect( g_exp_gravityMagnitudeEdit, &QLineEdit::editingFinished, [](){ Experimental_setSelectedEntityKeyValue( "gravity_magnitude", g_exp_gravityMagnitudeEdit->text() ); } );
		}
		vbox->addWidget( g_exp_gravityGroup );

		auto* pbrGroup = new QGroupBox( "PBR Material", root );
		auto* pbrForm = new QFormLayout( pbrGroup );
		g_exp_pbrAlbedo = new QLineEdit( root );
		g_exp_pbrAlbedo->setPlaceholderText( "textures/mymat (albedo/diffuse)" );
		g_exp_pbrAlbedo->setClearButtonEnabled( true );
		g_exp_pbrNormal = new QLineEdit( root );
		g_exp_pbrNormal->setPlaceholderText( "textures/mymat_normals (optional)" );
		g_exp_pbrNormal->setClearButtonEnabled( true );
		g_exp_pbrRoughness = new QDoubleSpinBox( root );
		g_exp_pbrRoughness->setRange( 0, 1 );
		g_exp_pbrRoughness->setSingleStep( 0.05 );
		g_exp_pbrRoughness->setValue( 0.5 );
		g_exp_pbrRoughness->setDecimals( 2 );
		g_exp_pbrMetallic = new QDoubleSpinBox( root );
		g_exp_pbrMetallic->setRange( 0, 1 );
		g_exp_pbrMetallic->setSingleStep( 0.05 );
		g_exp_pbrMetallic->setValue( 0 );
		g_exp_pbrMetallic->setDecimals( 2 );
		g_exp_pbrAO = new QLineEdit( root );
		g_exp_pbrAO->setPlaceholderText( "textures/mymat_ao (optional)" );
		g_exp_pbrAO->setClearButtonEnabled( true );
		pbrForm->addRow( "Albedo", g_exp_pbrAlbedo );
		pbrForm->addRow( "Normal map", g_exp_pbrNormal );
		pbrForm->addRow( "Roughness", g_exp_pbrRoughness );
		pbrForm->addRow( "Metallic", g_exp_pbrMetallic );
		pbrForm->addRow( "AO", g_exp_pbrAO );
		auto* pbrApplyBtn = new QPushButton( "Apply PBR", root );
		auto* pbrCopyBtn = new QPushButton( "Copy PBR Shader", root );
		auto* pbrFromShaderBtn = new QPushButton( "Albedo from Shader", root );
		pbrForm->addRow( "", pbrApplyBtn );
		pbrForm->addRow( "", pbrCopyBtn );
		pbrForm->addRow( "", pbrFromShaderBtn );
		QObject::connect( pbrFromShaderBtn, &QPushButton::clicked, [](){
			if ( g_exp_shaderEdit != nullptr && g_exp_pbrAlbedo != nullptr && !g_exp_shaderEdit->text().trimmed().isEmpty() ) {
				g_exp_pbrAlbedo->setText( g_exp_shaderEdit->text().trimmed() );
			}
		} );
		QObject::connect( pbrApplyBtn, &QPushButton::clicked, [](){
			if ( g_exp_pbrAlbedo == nullptr || g_exp_pbrAlbedo->text().trimmed().isEmpty() ) return;
			QString shader = g_exp_pbrAlbedo->text().trimmed();
			if ( shader.contains( '.' ) ) {
				shader = shader.left( shader.lastIndexOf( '.' ) );
			}
			if ( g_exp_shaderEdit != nullptr ) {
				g_exp_shaderEdit->setText( shader );
			}
			Experimental_applySelectedShader();
		} );
		QObject::connect( pbrCopyBtn, &QPushButton::clicked, [](){
			if ( g_exp_pbrAlbedo == nullptr || g_exp_pbrAlbedo->text().trimmed().isEmpty() ) return;
			QString albedo = g_exp_pbrAlbedo->text().trimmed();
			if ( albedo.contains( '.' ) ) {
				albedo = albedo.left( albedo.lastIndexOf( '.' ) );
			}
			QString normal = g_exp_pbrNormal != nullptr ? g_exp_pbrNormal->text().trimmed() : QString();
			double roughness = g_exp_pbrRoughness != nullptr ? g_exp_pbrRoughness->value() : 0.5;
			double metallic = g_exp_pbrMetallic != nullptr ? g_exp_pbrMetallic->value() : 0;
			QString ao = g_exp_pbrAO != nullptr ? g_exp_pbrAO->text().trimmed() : QString();
			QString snippet;
			QTextStream out( &snippet );
			out << albedo << "\n{\n\tqer_editorImage " << albedo << "\n\tmap " << albedo << "\n";
			if ( !normal.isEmpty() ) {
				out << "\t// Normal map (Doom3: bumpmap " << normal << "; Q3: engine-dependent)\n";
			}
			out << "\t// PBR: roughness=" << roughness << " metallic=" << metallic << "\n";
			if ( !ao.isEmpty() ) {
				out << "\t// AO: " << ao << "\n";
			}
			out << "}\n";
			QGuiApplication::clipboard()->setText( snippet );
			Sys_Status( "PBR shader snippet copied to clipboard" );
		} );
		vbox->addWidget( pbrGroup );

		auto* envGroup = new QGroupBox( "Environment", root );
		auto* envForm = new QFormLayout( envGroup );
		g_exp_skyboxHDREdit = new QLineEdit( root );
		g_exp_skyboxHDREdit->setPlaceholderText( "hdr/skies/mysky.hdr (worldspawn _skyboxHDR)" );
		g_exp_skyboxHDREdit->setClearButtonEnabled( true );
		envForm->addRow( "HDR Sky (IBL)", g_exp_skyboxHDREdit );
		QObject::connect( g_exp_skyboxHDREdit, &QLineEdit::editingFinished, [](){
			scene::Node* worldNode = Map_FindWorldspawn( g_map );
			if ( worldNode != nullptr && g_exp_skyboxHDREdit != nullptr ) {
				Entity* worldspawn = Node_getEntity( *worldNode );
				if ( worldspawn != nullptr ) {
					UndoableCommand undo( "entitySetKeyValue" );
					worldspawn->setKeyValue( "_skyboxHDR", g_exp_skyboxHDREdit->text().trimmed().toLatin1().constData() );
					SceneChangeNotify();
				}
			}
		} );
		vbox->addWidget( envGroup );

		auto* transformGroup = new QGroupBox( "Transform", root );
		auto* transformGrid = new QGridLayout( transformGroup );
		// Layout: Label | X | Y | Z (Blender-style)
		transformGrid->addWidget( new QLabel( "" ), 0, 0 );
		transformGrid->addWidget( new QLabel( "X", transformGroup ), 0, 1 );
		transformGrid->addWidget( new QLabel( "Y", transformGroup ), 0, 2 );
		transformGrid->addWidget( new QLabel( "Z", transformGroup ), 0, 3 );
		transformGrid->addWidget( new QLabel( "Location", transformGroup ), 1, 0 );
		transformGrid->addWidget( g_exp_locX = new DoubleSpinBox( -32768, 32768, 0, 6, 8, false ), 1, 1 );
		transformGrid->addWidget( g_exp_locY = new DoubleSpinBox( -32768, 32768, 0, 6, 8, false ), 1, 2 );
		transformGrid->addWidget( g_exp_locZ = new DoubleSpinBox( -32768, 32768, 0, 6, 8, false ), 1, 3 );
		transformGrid->addWidget( new QLabel( "Rotation", transformGroup ), 2, 0 );
		transformGrid->addWidget( g_exp_rotX = new DoubleSpinBox( -360, 360, 0, 6, 1, true ), 2, 1 );
		transformGrid->addWidget( g_exp_rotY = new DoubleSpinBox( -360, 360, 0, 6, 1, true ), 2, 2 );
		transformGrid->addWidget( g_exp_rotZ = new DoubleSpinBox( -360, 360, 0, 6, 1, true ), 2, 3 );
		transformGrid->addWidget( new QLabel( "Scale", transformGroup ), 3, 0 );
		transformGrid->addWidget( g_exp_scaleX = new DoubleSpinBox( 0.001, 32768, 1, 6, 0.1, false ), 3, 1 );
		transformGrid->addWidget( g_exp_scaleY = new DoubleSpinBox( 0.001, 32768, 1, 6, 0.1, false ), 3, 2 );
		transformGrid->addWidget( g_exp_scaleZ = new DoubleSpinBox( 0.001, 32768, 1, 6, 0.1, false ), 3, 3 );
		g_exp_uniformScale = new QCheckBox( "Uniform scale", transformGroup );
		g_exp_uniformScale->setChecked( QSettings().value( "Properties/Experimental/UniformScale", true ).toBool() );
		transformGrid->addWidget( g_exp_uniformScale, 4, 0, 1, 4 );
		g_exp_locX->setEnabled( false );
		g_exp_locY->setEnabled( false );
		g_exp_locZ->setEnabled( false );
		g_exp_rotX->setEnabled( false );
		g_exp_rotY->setEnabled( false );
		g_exp_rotZ->setEnabled( false );
		g_exp_scaleX->setEnabled( false );
		g_exp_scaleY->setEnabled( false );
		g_exp_scaleZ->setEnabled( false );
		auto applyLoc = [](){
			if ( GlobalSelectionSystem().countSelected() == 0 ) return;
			const Vector3 target( g_exp_locX->value(), g_exp_locY->value(), g_exp_locZ->value() );
			UndoableCommand undo( "translateSelected" );
			Select_TranslateToPosition( target );
			SceneChangeNotify();
			Experimental_refreshTransform();
		};
		auto applyRot = [](){
			if ( GlobalSelectionSystem().countSelected() == 0 ) return;
			UndoableCommand undo( "rotateSelectedEulerXYZ" );
			Select_RotateByEulerXYZ( g_exp_rotX->value(), g_exp_rotY->value(), g_exp_rotZ->value() );
			SceneChangeNotify();
		};
		auto applyScale = [](){
			if ( GlobalSelectionSystem().countSelected() == 0 ) return;
			UndoableCommand undo( "scaleSelected" );
			const Vector3 target( g_exp_scaleX->value(), g_exp_scaleY->value(), g_exp_scaleZ->value() );
			if ( Experimental_selectionHasModelScale() ) {
				// misc_model: write directly to entity keyvalues so scale persists
				char buf[64];
				if ( target.x() == target.y() && target.y() == target.z() ) {
					sprintf( buf, "%g", target.x() );
					Scene_EntitySetKeyValue_Selected( "modelscale", buf );
					Scene_EntitySetKeyValue_Selected( "modelscale_vec", "" );
				} else {
					sprintf( buf, "%g %g %g", target.x(), target.y(), target.z() );
					Scene_EntitySetKeyValue_Selected( "modelscale", "" );
					Scene_EntitySetKeyValue_Selected( "modelscale_vec", buf );
				}
				SceneChangeNotify();
			} else {
				// Brushes etc: Select_Scale expects a factor (target/current)
				const Vector3 current = Experimental_getScaleFromSelection();
				const float fx = ( current.x() > 0.0001f ) ? ( target.x() / current.x() ) : 1.f;
				const float fy = ( current.y() > 0.0001f ) ? ( target.y() / current.y() ) : 1.f;
				const float fz = ( current.z() > 0.0001f ) ? ( target.z() / current.z() ) : 1.f;
				Select_Scale( fx, fy, fz );
				SceneChangeNotify();
			}
			Experimental_refreshTransform();
		};
		QObject::connect( g_exp_locX, &QDoubleSpinBox::editingFinished, applyLoc );
		QObject::connect( g_exp_locY, &QDoubleSpinBox::editingFinished, applyLoc );
		QObject::connect( g_exp_locZ, &QDoubleSpinBox::editingFinished, applyLoc );
		QObject::connect( g_exp_rotX, &QDoubleSpinBox::editingFinished, applyRot );
		QObject::connect( g_exp_rotY, &QDoubleSpinBox::editingFinished, applyRot );
		QObject::connect( g_exp_rotZ, &QDoubleSpinBox::editingFinished, applyRot );
		QObject::connect( g_exp_scaleX, &QDoubleSpinBox::editingFinished, applyScale );
		QObject::connect( g_exp_scaleY, &QDoubleSpinBox::editingFinished, applyScale );
		QObject::connect( g_exp_scaleZ, &QDoubleSpinBox::editingFinished, applyScale );
		// Uniform scale: sync X/Y/Z when any changes
		auto syncUniformScale = []( QDoubleSpinBox* source ){
			if ( g_exp_uniformScale != nullptr && g_exp_uniformScale->isChecked() && source != nullptr ) {
				const double v = source->value();
				g_exp_scaleX->blockSignals( true );
				g_exp_scaleY->blockSignals( true );
				g_exp_scaleZ->blockSignals( true );
				g_exp_scaleX->setValue( v );
				g_exp_scaleY->setValue( v );
				g_exp_scaleZ->setValue( v );
				g_exp_scaleX->blockSignals( false );
				g_exp_scaleY->blockSignals( false );
				g_exp_scaleZ->blockSignals( false );
			}
		};
		QObject::connect( g_exp_scaleX, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), [syncUniformScale](){ syncUniformScale( g_exp_scaleX ); } );
		QObject::connect( g_exp_scaleY, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), [syncUniformScale](){ syncUniformScale( g_exp_scaleY ); } );
		QObject::connect( g_exp_scaleZ, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), [syncUniformScale](){ syncUniformScale( g_exp_scaleZ ); } );
		QObject::connect( g_exp_uniformScale, &QCheckBox::toggled, [syncUniformScale]( bool checked ){
			QSettings().setValue( "Properties/Experimental/UniformScale", checked );
			if ( checked && g_exp_scaleX != nullptr ) {
				syncUniformScale( g_exp_scaleX );
			}
		} );
		vbox->addWidget( transformGroup );

		g_exp_propertiesDock->setWidget( root );
	}
	window->addDockWidget( Qt::RightDockWidgetArea, g_exp_propertiesDock );

	g_exp_previewDock = new QDockWidget( "Viewport Preview", window );
	g_exp_previewDock->setObjectName( "dock_experimental_preview" );
	g_exp_previewHost = new ExperimentalPreviewHostWidget( g_exp_previewDock );
	g_exp_previewDock->setWidget( g_exp_previewHost );
	window->addDockWidget( Qt::RightDockWidgetArea, g_exp_previewDock );

	g_exp_assetsDock = new QDockWidget( "Asset Browser", window );
	g_exp_assetsDock->setObjectName( "dock_experimental_asset_library" );
	{
		auto* root = new QWidget( g_exp_assetsDock );
		auto* vbox = new QVBoxLayout( root );
		g_exp_assetsList = new QListWidget( root );
		g_exp_assetsList->setViewMode( QListView::IconMode );
		g_exp_assetsList->setUniformItemSizes( true );
		g_exp_assetsList->setResizeMode( QListView::Adjust );
		g_exp_assetsList->setDragEnabled( true );
		auto* refreshButton = new QPushButton( "Refresh Assets", root );
		vbox->addWidget( g_exp_assetsList );
		vbox->addWidget( refreshButton );
		QObject::connect( refreshButton, &QPushButton::clicked, [](){ Experimental_refreshAssetLibrary(); } );
		QObject::connect( g_exp_assetsList, &QListWidget::itemDoubleClicked, []( QListWidgetItem* item ){
			if ( item != nullptr && g_exp_shaderEdit != nullptr ) {
				g_exp_shaderEdit->setText( item->text() );
				Experimental_applySelectedShader();
			}
		} );
		g_exp_assetsDock->setWidget( root );
	}
	window->addDockWidget( Qt::LeftDockWidgetArea, g_exp_assetsDock );

	g_exp_historyDock = new QDockWidget( "History", window );
	g_exp_historyDock->setObjectName( "dock_experimental_history" );
	{
		g_exp_historyList = new QListWidget( g_exp_historyDock );
		g_exp_historyDock->setWidget( g_exp_historyList );
	}
	window->addDockWidget( Qt::LeftDockWidgetArea, g_exp_historyDock );

	g_exp_syncDock = new QDockWidget( "Live Sync", window );
	g_exp_syncDock->setObjectName( "dock_experimental_live_sync" );
	{
		auto* root = new QWidget( g_exp_syncDock );
		auto* vbox = new QVBoxLayout( root );
		auto* form = new QFormLayout;
		g_exp_syncPort = new QSpinBox( root );
		g_exp_syncPort->setRange( 1024, 65535 );
		g_exp_syncPort->setValue( QSettings().value( "Properties/Experimental/LiveSyncPort", 28930 ).toInt() );
		g_exp_syncStateLabel = new QLabel( "Stopped", root );
		g_exp_syncStateLabel->setWordWrap( true );
		g_exp_syncClientsLabel = new QLabel( "0", root );
		g_exp_syncRuntimeLabel = new QLabel( g_exp_lastRuntimeEvent, root );
		g_exp_syncRuntimeLabel->setWordWrap( true );
		form->addRow( "Port", g_exp_syncPort );
		form->addRow( "Server", g_exp_syncStateLabel );
		form->addRow( "Clients", g_exp_syncClientsLabel );
		form->addRow( "Last Runtime", g_exp_syncRuntimeLabel );
		vbox->addLayout( form );

		g_exp_syncAutoStart = new QCheckBox( "Start automatically", root );
		g_exp_syncAutoStart->setChecked( QSettings().value( "Properties/Experimental/LiveSyncAutoStart", false ).toBool() );
		g_exp_syncAutoSync = new QCheckBox( "Broadcast scene, selection, and camera changes", root );
		g_exp_syncAutoSync->setChecked( QSettings().value( "Properties/Experimental/LiveSyncAutoSync", true ).toBool() );
		vbox->addWidget( g_exp_syncAutoStart );
		vbox->addWidget( g_exp_syncAutoSync );

		auto* buttons = new QWidget( root );
		auto* buttonsLayout = new QGridLayout( buttons );
		buttonsLayout->setContentsMargins( 0, 0, 0, 0 );
		auto* startButton = new QPushButton( "Start", root );
		auto* stopButton = new QPushButton( "Stop", root );
		auto* snapshotButton = new QPushButton( "Send Snapshot", root );
		auto* copyUrlButton = new QPushButton( "Copy URL", root );
		buttonsLayout->addWidget( startButton, 0, 0 );
		buttonsLayout->addWidget( stopButton, 0, 1 );
		buttonsLayout->addWidget( snapshotButton, 1, 0 );
		buttonsLayout->addWidget( copyUrlButton, 1, 1 );
		vbox->addWidget( buttons );

		g_exp_syncLog = new QListWidget( root );
		vbox->addWidget( g_exp_syncLog, 1 );

		QObject::connect( g_exp_syncPort, QOverload<int>::of( &QSpinBox::valueChanged ), []( int value ){
			QSettings().setValue( "Properties/Experimental/LiveSyncPort", value );
			if ( g_exp_liveSyncService != nullptr && g_exp_liveSyncService->isRunning() ) {
				g_exp_liveSyncService->start( static_cast<quint16>( value ) );
			}
		} );
		QObject::connect( g_exp_syncAutoStart, &QCheckBox::toggled, []( bool checked ){
			QSettings().setValue( "Properties/Experimental/LiveSyncAutoStart", checked );
		} );
		QObject::connect( g_exp_syncAutoSync, &QCheckBox::toggled, []( bool checked ){
			QSettings().setValue( "Properties/Experimental/LiveSyncAutoSync", checked );
			if ( g_exp_liveSyncService != nullptr ) {
				g_exp_liveSyncService->setAutoSync( checked );
			}
		} );
		QObject::connect( startButton, &QPushButton::clicked, [](){
			if ( g_exp_liveSyncService != nullptr && g_exp_syncPort != nullptr ) {
				g_exp_liveSyncService->start( static_cast<quint16>( g_exp_syncPort->value() ) );
			}
		} );
		QObject::connect( stopButton, &QPushButton::clicked, [](){
			if ( g_exp_liveSyncService != nullptr ) {
				g_exp_liveSyncService->stop();
			}
		} );
		QObject::connect( snapshotButton, &QPushButton::clicked, [](){
			if ( g_exp_liveSyncService != nullptr ) {
				Experimental_appendLiveSyncLog( "Manual snapshot requested." );
				g_exp_liveSyncService->sendSnapshot( "manual" );
			}
		} );
		QObject::connect( copyUrlButton, &QPushButton::clicked, [](){
			if ( g_exp_syncPort == nullptr ) {
				return;
			}
			const QString url = QString( "ws://127.0.0.1:%1" ).arg( g_exp_syncPort->value() );
			QGuiApplication::clipboard()->setText( url );
			Sys_Status( "Live Sync URL copied to clipboard" );
		} );

		g_exp_syncDock->setWidget( root );
	}
	window->addDockWidget( Qt::LeftDockWidgetArea, g_exp_syncDock );

	g_exp_usdDock = new QDockWidget( "Outliner", window );
	g_exp_usdDock->setObjectName( "dock_experimental_usd_structure" );
	{
		auto* root = new QWidget( g_exp_usdDock );
		auto* vbox = new QVBoxLayout( root );
		auto* btnRow = new QWidget( root );
		auto* btnLayout = new QGridLayout( btnRow );
		btnLayout->setContentsMargins( 0, 0, 0, 0 );
		auto* importButton = new QPushButton( "Import USD", root );
		auto* exportButton = new QPushButton( "Export to USDA", root );
		auto* importMayaButton = new QPushButton( "Import Maya", root );
		auto* exportMayaButton = new QPushButton( "Export .ma", root );
		btnLayout->addWidget( importButton, 0, 0 );
		btnLayout->addWidget( exportButton, 0, 1 );
		btnLayout->addWidget( importMayaButton, 1, 0 );
		btnLayout->addWidget( exportMayaButton, 1, 1 );
		g_exp_usdTree = new QTreeWidget( root );
		g_exp_usdTree->setHeaderLabels( QStringList() << "Node" << "Type" );
		vbox->addWidget( btnRow );
		vbox->addWidget( g_exp_usdTree );
		QObject::connect( importButton, &QPushButton::clicked, [](){ Experimental_importUSDStructure(); } );
		QObject::connect( exportButton, &QPushButton::clicked, [](){ Experimental_exportToUSDA(); } );
		QObject::connect( importMayaButton, &QPushButton::clicked, [](){ Experimental_importMayaASCII(); } );
		QObject::connect( exportMayaButton, &QPushButton::clicked, [](){ Experimental_exportToMayaASCII(); } );
		g_exp_usdDock->setWidget( root );
	}
	window->addDockWidget( Qt::LeftDockWidgetArea, g_exp_usdDock );

	g_exp_ecsDock = new QDockWidget( "Entity Palette", window );
	g_exp_ecsDock->setObjectName( "dock_experimental_ecs_authoring" );
	{
		auto* root = new QWidget( g_exp_ecsDock );
		auto* vbox = new QVBoxLayout( root );
		g_exp_ecsCategoryCombo = new QComboBox( root );
		g_exp_ecsCategoryCombo->addItems( QStringList()
			<< "All"
			<< "Fire & Environment"
			<< "Wind & Physics"
			<< "Props & Physics"
			<< "Gravity & Space"
			<< "Spawn & Streaming"
			<< "Level Streaming"
			<< "Vehicles"
			<< "Splines"
			<< "Other" );
		QObject::connect( g_exp_ecsCategoryCombo, QOverload<int>::of( &QComboBox::currentIndexChanged ), []( int ){ Experimental_refreshECSList(); } );
		g_exp_ecsEntityList = new QListWidget( root );
		auto* addButton = new QPushButton( "Add Entity at Camera", root );
		vbox->addWidget( new QLabel( "Category", root ) );
		vbox->addWidget( g_exp_ecsCategoryCombo );
		vbox->addWidget( new QLabel( "Entities", root ) );
		vbox->addWidget( g_exp_ecsEntityList );
		vbox->addWidget( addButton );
		QObject::connect( addButton, &QPushButton::clicked, [](){
			QListWidgetItem* item = g_exp_ecsEntityList != nullptr ? g_exp_ecsEntityList->currentItem() : nullptr;
			if ( item != nullptr ) {
				Experimental_ecsAddEntity( item->text().toLatin1().constData() );
			}
		} );
		QObject::connect( g_exp_ecsEntityList, &QListWidget::itemDoubleClicked, []( QListWidgetItem* item ){
			if ( item != nullptr ) {
				Experimental_ecsAddEntity( item->text().toLatin1().constData() );
			}
		} );
		g_exp_ecsDock->setWidget( root );
	}
	window->addDockWidget( Qt::LeftDockWidgetArea, g_exp_ecsDock );
	Experimental_refreshECSList();

	window->splitDockWidget( g_exp_propertiesDock, g_exp_previewDock, Qt::Vertical );
	window->splitDockWidget( g_exp_assetsDock, g_exp_usdDock, Qt::Vertical );
	window->tabifyDockWidget( g_exp_usdDock, g_exp_historyDock );
	window->tabifyDockWidget( g_exp_historyDock, g_exp_syncDock );
	window->tabifyDockWidget( g_exp_syncDock, g_exp_ecsDock );

	Experimental_refreshSelection();
	Experimental_refreshAssetLibrary();
	Experimental_refreshLiveSyncUi();
	OpenWysiwygWorkspace_impl();
	if ( g_exp_syncAutoStart != nullptr && g_exp_syncAutoStart->isChecked() && g_exp_liveSyncService != nullptr && g_exp_syncPort != nullptr ) {
		g_exp_liveSyncService->start( static_cast<quint16>( g_exp_syncPort->value() ) );
	}
}

void Experimental_destroyDocks(){
	Experimental_setUndoTrackerAttached( false );
	if ( g_exp_liveSyncService != nullptr ) {
		g_exp_liveSyncService->stop();
		g_exp_liveSyncService = nullptr;
	}
	g_exp_propertiesDock = nullptr;
	g_exp_previewDock = nullptr;
	g_exp_assetsDock = nullptr;
	g_exp_historyDock = nullptr;
	g_exp_usdDock = nullptr;
	g_exp_ecsDock = nullptr;
	g_exp_syncDock = nullptr;
	g_exp_previewHost = nullptr;
	g_exp_ecsCategoryCombo = nullptr;
	g_exp_ecsEntityList = nullptr;
	g_exp_selectedCountLabel = nullptr;
	g_exp_selectedComponentsLabel = nullptr;
	g_exp_selectionTypeLabel = nullptr;
	g_exp_selectionBoundsLabel = nullptr;
	g_exp_selectionShaderLabel = nullptr;
	g_exp_entityGroup = nullptr;
	g_exp_entityClassLabel = nullptr;
	g_exp_entityNameEdit = nullptr;
	g_exp_entityTargetnameEdit = nullptr;
	g_exp_entityTargetEdit = nullptr;
	g_exp_entitySoundEdit = nullptr;
	g_exp_entityModelEdit = nullptr;
	g_exp_worldGroup = nullptr;
	g_exp_worldMessageEdit = nullptr;
	g_exp_worldMusicEdit = nullptr;
	g_exp_worldGravityEdit = nullptr;
	g_exp_worldColorEdit = nullptr;
	g_exp_worldMinlight = nullptr;
	g_exp_gravityGroup = nullptr;
	g_exp_gravityDirectionEdit = nullptr;
	g_exp_gravityMagnitudeEdit = nullptr;
	g_exp_shaderEdit = nullptr;
	g_exp_pbrAlbedo = nullptr;
	g_exp_pbrNormal = nullptr;
	g_exp_pbrRoughness = nullptr;
	g_exp_pbrMetallic = nullptr;
	g_exp_pbrAO = nullptr;
	g_exp_locX = nullptr;
	g_exp_locY = nullptr;
	g_exp_locZ = nullptr;
	g_exp_rotX = nullptr;
	g_exp_rotY = nullptr;
	g_exp_rotZ = nullptr;
	g_exp_scaleX = nullptr;
	g_exp_scaleY = nullptr;
	g_exp_scaleZ = nullptr;
	g_exp_assetsList = nullptr;
	g_exp_historyList = nullptr;
	g_exp_usdTree = nullptr;
	g_exp_syncStateLabel = nullptr;
	g_exp_syncClientsLabel = nullptr;
	g_exp_syncRuntimeLabel = nullptr;
	g_exp_syncPort = nullptr;
	g_exp_syncAutoStart = nullptr;
	g_exp_syncAutoSync = nullptr;
	g_exp_syncLog = nullptr;
	g_exp_historyCounter = 0;
}

static QJsonArray Experimental_buildCameraBookmarksArray(){
	QJsonArray bookmarks;
	for ( int i = 0; i < 5; ++i ) {
		QJsonObject bookmark;
		bookmark.insert( "index", i );
		bookmark.insert( "valid", ::g_cameraBookmarks_valid[i] );
		if ( ::g_cameraBookmarks_valid[i] ) {
			bookmark.insert( "origin", Experimental_vector3ToJson( ::g_cameraBookmarks_origin[i] ) );
			bookmark.insert( "angles", Experimental_vector3ToJson( ::g_cameraBookmarks_angles[i] ) );
		}
		bookmarks.append( bookmark );
	}
	return bookmarks;
}

static void Experimental_restoreCameraBookmarks( const QJsonValue& value ){
	if ( !value.isArray() ) {
		return;
	}

	for ( bool& valid : ::g_cameraBookmarks_valid ) {
		valid = false;
	}

	for ( const QJsonValue& entryValue : value.toArray() ) {
		if ( !entryValue.isObject() ) {
			continue;
		}

		const QJsonObject entry = entryValue.toObject();
		const int index = entry.value( "index" ).toInt( -1 );
		if ( index < 0 || index >= 5 ) {
			continue;
		}
		if ( !entry.value( "valid" ).toBool( true ) ) {
			::g_cameraBookmarks_valid[index] = false;
			continue;
		}

		Vector3 origin;
		Vector3 angles;
		if ( Experimental_tryParseVector3( entry.value( "origin" ), origin )
			&& Experimental_tryParseVector3( entry.value( "angles" ), angles ) ) {
			::g_cameraBookmarks_origin[index] = origin;
			::g_cameraBookmarks_angles[index] = angles;
			::g_cameraBookmarks_valid[index] = true;
		}
	}
}
}



void create_layout_menu( QMenuBar *menubar, MainFrame::EViewStyle style ){
	QMenu *menu = menubar->addMenu( "&Layout" );
	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "&Regular", "LayoutRegular" );
	create_menu_item_with_mnemonic( menu, "Regular &Left", "LayoutRegularLeft" );
	create_menu_item_with_mnemonic( menu, "&4-pane", "LayoutHammerFourPane" );
	create_menu_item_with_mnemonic( menu, "&Floating viewports", "LayoutFloating" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Save workspace", "LayoutSaveWorkspace" );
	menu->addSeparator();
}

void create_file_menu( QMenuBar *menubar ){
	// File menu
	QMenu *menu = menubar->addMenu( "&File" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "&New Map", "NewMap" );
	menu->addSeparator();

	create_menu_item_with_mnemonic( menu, "&Open...", "OpenMap" );
	create_menu_item_with_mnemonic( menu, "&Import...", "ImportMap" );
	create_menu_item_with_mnemonic( menu, "Insert &Prefab...", "InsertPrefab" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Save", "SaveMap" );
	create_menu_item_with_mnemonic( menu, "Save &as...", "SaveMapAs" );
	create_menu_item_with_mnemonic( menu, "Save s&elected...", "SaveSelected" );
	create_menu_item_with_mnemonic( menu, "Save Prefab...", "SavePrefab" );
	create_menu_item_with_mnemonic( menu, "Save re&gion...", "SaveRegion" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Pointfile", "TogglePointfile" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Save wor&kspace", "LayoutSaveWorkspace" );
	menu->addSeparator();
	MRU_constructMenu( menu );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "E&xit", "Exit" );
}

void create_edit_menu( QMenuBar *menubar ){
	// Edit menu
	QMenu *menu = menubar->addMenu( "&Edit" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "&Undo", "Undo" );
	create_menu_item_with_mnemonic( menu, "&Redo", "Redo" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Copy", "Copy" );
	create_menu_item_with_mnemonic( menu, "&Paste", "Paste" );
	create_menu_item_with_mnemonic( menu, "P&aste To Camera", "PasteToCamera" );
	create_menu_item_with_mnemonic( menu, "Move To Camera", "MoveToCamera" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "&Duplicate", "CloneSelection" );
	create_menu_item_with_mnemonic( menu, "Duplicate, make uni&que", "CloneSelectionAndMakeUnique" );
	create_menu_item_with_mnemonic( menu, "D&elete", "DeleteSelection" );
	//create_menu_item_with_mnemonic( menu, "Pa&rent", "ParentSelection" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "C&lear Selection", "UnSelectSelection" );
	create_menu_item_with_mnemonic( menu, "&Invert Selection", "InvertSelection" );
	create_menu_item_with_mnemonic( menu, "Select i&nside", "SelectInside" );
	create_menu_item_with_mnemonic( menu, "Select &touching", "SelectTouching" );

	menu->addSeparator();

	create_menu_item_with_mnemonic( menu, "Select All Of Type", "SelectAllOfType" );
	create_menu_item_with_mnemonic( menu, "Select Textured", "SelectTextured" );
	create_menu_item_with_mnemonic( menu, "&Expand Selection To Primitives", "ExpandSelectionToPrimitives" );
	create_menu_item_with_mnemonic( menu, "&Expand Selection To Entities", "ExpandSelectionToEntities" );
	create_menu_item_with_mnemonic( menu, "&Expand Selection To Layers", "ExpandSelectionToLayers" );
	create_menu_item_with_mnemonic( menu, "Select Connected Entities", "SelectConnectedEntities" );

	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Command &Launcher", "CommandLauncher" );
	create_menu_item_with_mnemonic( menu, "&Shortcuts", "Shortcuts" );
	create_menu_item_with_mnemonic( menu, "Pre&ferences", "Preferences" );
}

Vector3 Add_entitySpawnOrigin(){
	if ( g_pParentWnd != nullptr && g_pParentWnd->GetCamWnd() != nullptr ) {
		return Camera_getOrigin( *g_pParentWnd->GetCamWnd() );
	}
	return g_vector3_identity;
}

void Add_createMiscModel();

void Add_createEntity( const char* classname ){
	if ( classname_equal( classname, "misc_model" ) ) {
		Add_createMiscModel();
		return;
	}
	Entity_createFromSelection( classname, Add_entitySpawnOrigin() );
}

void Add_createLight(){
	Add_createEntity( "light" );
}

void Add_createInfoPlayerStart(){
	Add_createEntity( "info_player_start" );
}

void Add_createInfoPlayerDeathmatch(){
	Add_createEntity( "info_player_deathmatch" );
}

void Add_createSpline(){
	Add_createEntity( "misc_spline" );
}

void Add_createMiscModel(){
	const char* modelPath = misc_model_dialog( MainFrame_getWindow() );
	if ( modelPath == nullptr ) {
		return;
	}

	UndoableCommand undo( "insertModel" );
	EntityClass* entityClass = GlobalEntityClassManager().findOrInsert( "misc_model", false );
	NodeSmartReference node( GlobalEntityCreator().createEntity( entityClass ) );

	Node_getTraversable( GlobalSceneGraph().root() )->insert( node );

	scene::Path entitypath( makeReference( GlobalSceneGraph().root() ) );
	entitypath.push( makeReference( node.get() ) );
	scene::Instance& instance = findInstance( entitypath );

	if ( Transformable* transform = Instance_getTransformable( instance ) ) {
		transform->setType( TRANSFORM_PRIMITIVE );
		transform->setTranslation( Add_entitySpawnOrigin() );
		transform->freezeTransform();
	}

	GlobalSelectionSystem().setSelectedAll( false );
	Instance_setSelected( instance, true );

	Node_getEntity( node )->setKeyValue( entityClass->miscmodel_key(), modelPath );
}

class AddEntityClassCollector final : public EntityClassVisitor
{
	QList<QString>& m_names;
public:
	AddEntityClassCollector( QList<QString>& names ) : m_names( names ){
	}
	void visit( EntityClass* entityClass ) override {
		m_names.push_back( entityClass->name() );
	}
};

void Add_openEntityDialog(){
	QDialog dialog( MainFrame_getWindow() );
	dialog.setWindowTitle( "Add Entity" );
	dialog.setModal( true );
	dialog.resize( 560, 480 );

	auto* layout = new QVBoxLayout( &dialog );
	auto* filterEdit = new QLineEdit( &dialog );
	filterEdit->setPlaceholderText( "Filter entity classes..." );
	layout->addWidget( filterEdit );

	auto* list = new QListWidget( &dialog );
	list->setSelectionMode( QAbstractItemView::SelectionMode::SingleSelection );
	layout->addWidget( list, 1 );

	auto* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog );
	buttons->button( QDialogButtonBox::Ok )->setText( "Add" );
	layout->addWidget( buttons );

	QList<QString> names;
	{
		AddEntityClassCollector collector( names );
		GlobalEntityClassManager().forEach( collector );
	}
	names.sort( Qt::CaseInsensitive );

	const auto refill = [&](){
		const auto filter = filterEdit->text();
		list->clear();
		for ( const auto& name : names )
		{
			if ( filter.isEmpty() || name.contains( filter, Qt::CaseInsensitive ) ) {
				list->addItem( name );
			}
		}
		if ( list->count() > 0 ) {
			list->setCurrentRow( 0 );
		}
	};

	QObject::connect( filterEdit, &QLineEdit::textChanged, [&](){ refill(); } );
	QObject::connect( buttons, &QDialogButtonBox::accepted, [&](){
		if ( list->currentItem() != nullptr ) {
			dialog.accept();
		}
	} );
	QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
	QObject::connect( list, &QListWidget::itemDoubleClicked, [&]( QListWidgetItem* item ){
		if ( item != nullptr ) {
			dialog.accept();
		}
	} );

	refill();
	filterEdit->setFocus();

	if ( dialog.exec() == QDialog::DialogCode::Accepted && list->currentItem() != nullptr ) {
		const auto classname = list->currentItem()->text().toLatin1();
		Add_createEntity( classname.constData() );
	}
}

Vector3 g_cameraBookmarks_origin[5];
Vector3 g_cameraBookmarks_angles[5];
bool g_cameraBookmarks_valid[5]{};

void CameraBookmark_store( std::size_t index ){
	if ( g_pParentWnd == nullptr || g_pParentWnd->GetCamWnd() == nullptr || index >= 5 ) {
		return;
	}
	g_cameraBookmarks_origin[index] = Camera_getOrigin( *g_pParentWnd->GetCamWnd() );
	g_cameraBookmarks_angles[index] = Camera_getAngles( *g_pParentWnd->GetCamWnd() );
	g_cameraBookmarks_valid[index] = true;
	if ( g_exp_liveSyncService != nullptr ) {
		g_exp_liveSyncService->broadcastBookmarksState( "editorStore" );
	}
	Sys_Status( StringStream( "Stored camera bookmark ", index + 1 ).c_str() );
}

void CameraBookmark_recall( std::size_t index ){
	if ( g_pParentWnd == nullptr || g_pParentWnd->GetCamWnd() == nullptr || index >= 5 ) {
		return;
	}
	if ( !g_cameraBookmarks_valid[index] ) {
		Sys_Status( StringStream( "Camera bookmark ", index + 1, " is empty" ).c_str() );
		return;
	}
	Camera_setOrigin( *g_pParentWnd->GetCamWnd(), g_cameraBookmarks_origin[index] );
	Camera_setAngles( *g_pParentWnd->GetCamWnd(), g_cameraBookmarks_angles[index] );
	UpdateAllWindows();
	if ( g_exp_liveSyncService != nullptr ) {
		g_exp_liveSyncService->broadcastCameraState( "bookmarkRecall" );
		g_exp_liveSyncService->broadcastBookmarksState( "bookmarkRecall" );
	}
	Sys_Status( StringStream( "Recalled camera bookmark ", index + 1 ).c_str() );
}


QStringList IdTech3Tool_candidateNames( const IdTech3ToolDef& tool ){
	QStringList names;
	const auto addUnique = [&names]( const QString& name ){
		if ( !name.isEmpty() && !names.contains( name ) ) {
			names.push_back( name );
		}
	};

#if defined( WIN32 )
	const QStringList suffixes = { ".exe", "", ".x86_64.exe", ".x86.exe" };
#else
	const QStringList suffixes = { "", ".x86_64", ".x86" };
#endif

	const auto base = QString::fromLatin1( tool.baseExecutable );
	for ( const auto& suffix : suffixes )
	{
		addUnique( base + suffix );
	}

	addUnique( tool.legacyExecutable );
#if defined( WIN32 )
	if ( !QString::fromLatin1( tool.legacyExecutable ).endsWith( ".exe", Qt::CaseInsensitive ) ) {
		addUnique( StringStream( tool.legacyExecutable, ".exe" ).c_str() );
	}
#endif

	return names;
}

QString IdTech3Tool_executablePath( const IdTech3ToolDef& tool ){
	const auto candidates = IdTech3Tool_candidateNames( tool );

	const QStringList installDirs = {
		QString::fromLatin1( AppPath_get() ),
		QString::fromLatin1( GameToolsPath_get() ),
		QString::fromLatin1( EnginePath_get() ),
	};

	for ( const auto& directory : installDirs )
	{
		if ( directory.isEmpty() ) {
			continue;
		}
		for ( const auto& candidate : candidates )
		{
			const auto absolute = QDir( directory ).filePath( candidate );
			if ( QFileInfo::exists( absolute ) ) {
				return absolute;
			}
		}
	}

	for ( const auto& candidate : candidates )
	{
		const auto fromPath = QStandardPaths::findExecutable( candidate );
		if ( !fromPath.isEmpty() ) {
			return fromPath;
		}
	}

	return {};
}



void Layout_setStyleAndRequestRestart( MainFrame::EViewStyle style, const char* name ){
	if ( g_Layout_viewStyle.m_latched == style ) {
		return;
	}

	g_Layout_viewStyle.import( style );
	Preferences_Save();

	const auto message = StringStream( name, " layout will apply after restart.\n\nRestart now?" );
	if ( QMessageBox::question( MainFrame_getWindow(), "Restart is required", message.c_str(), QMessageBox::Yes | QMessageBox::No, QMessageBox::No ) == QMessageBox::Yes ) {
		Radiant_Restart();
	}
}

void Layout_setRegular(){
	Layout_setStyleAndRequestRestart( MainFrame::eRegular, "Regular" );
}
void Layout_setRegularLeft(){
	Layout_setStyleAndRequestRestart( MainFrame::eRegularLeft, "Regular Left" );
}
void Layout_setHammerFourPane(){
	Layout_setStyleAndRequestRestart( MainFrame::eSplit, "4-pane" );
}
void Layout_setFloating(){
	Layout_setStyleAndRequestRestart( MainFrame::eFloating, "Floating viewports" );
}
void Layout_saveWorkspace(){
	if ( g_pParentWnd != nullptr ) {
		g_pParentWnd->SaveGuiState();
	}
	Preferences_Save();
}

void Lua_openScript( const CopiedString& scriptPath, const char* title, bool externalEditor ){
	if ( scriptPath.empty() ) {
		globalWarningStream() << "Lua script path is empty for " << SingleQuoted( title ) << '\n';
		return;
	}
	DoShaderView( scriptPath.c_str(), title, externalEditor );
}

void Lua_editProps(){
	Lua_openScript( g_luaScriptProps, "Lua Props", false );
}
void Lua_editEntities(){
	Lua_openScript( g_luaScriptEntities, "Lua Entities", false );
}
void Lua_editItems(){
	Lua_openScript( g_luaScriptItems, "Lua Items", false );
}
void OpenWysiwygWorkspace(){
	OpenWysiwygWorkspace_impl();
}

void Experimental_togglePropertiesDock(){
	Experimental_togglePropertiesDock_impl();
}
void Experimental_togglePreviewDock(){
	Experimental_togglePreviewDock_impl();
}
void Experimental_toggleAssetsDock(){
	Experimental_toggleAssetsDock_impl();
}
void Experimental_toggleHistoryDock(){
	Experimental_toggleHistoryDock_impl();
}
void Experimental_toggleSyncDock(){
	Experimental_toggleSyncDock_impl();
}
void Experimental_toggleUSDDock(){
	Experimental_toggleUSDDock_impl();
}
void Experimental_toggleECSDock(){
	Experimental_toggleECSDock_impl();
}
void Experimental_importUSDStructure(){
	Experimental_importUSDStructure_impl();
}
void Experimental_exportToUSDA(){
	Experimental_exportToUSDA_impl();
}
void Experimental_importMayaASCII(){
	Experimental_importMayaASCII_impl();
}
void Experimental_exportToMayaASCII(){
	Experimental_exportToMayaASCII_impl();
}
void Lua_editMain(){
	Lua_openScript( g_luaScriptMain, "Lua Main", false );
}
void Lua_editObjectives(){
	Lua_openScript( g_luaScriptObjectives, "Lua Objectives", false );
}
void Lua_editPropsExternal(){
	Lua_openScript( g_luaScriptProps, "Lua Props", true );
}

QAction* create_highlighted_view_menu_item( QMenu* menu, const char* mnemonic, const char* commandName ){
	auto* commandAction = create_menu_item_with_mnemonic( menu, mnemonic, commandName );
	menu->removeAction( commandAction );

	auto* highlightedAction = new QWidgetAction( menu );
	auto* button = new QPushButton( mnemonic, menu );
	button->setFlat( true );
	button->setStyleSheet(
		"QPushButton { color: #35ff6b; background: transparent; border: 0; text-align: left; padding: 4px 26px 4px 24px; }"
		"QPushButton:hover { background: rgba( 53, 255, 107, 0.18 ); }"
	);
	button->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
	highlightedAction->setDefaultWidget( button );
	QObject::connect( button, &QPushButton::clicked, menu, [menu, commandAction](){
		commandAction->trigger();
		menu->hide();
	} );
	menu->addAction( highlightedAction );
	return commandAction;
}

IdTech3ToolDef g_idTech3Tools[] = {
	{ "Q3Map2++", "q3map2", "q3map2.x86_64", "Primary id Tech 3 map compiler (BSP/VIS/LIGHT stages)." },
	{ "QData3++", "qdata3", "qdata3.x86_64", "Asset compile pipeline for models/sprites and game data." },
	{ "Q2Map++", "q2map", "q2map.x86_64", "Legacy Quake II style map compile utility." },
	{ "MBSPC++", "mbspc", "mbspc.x86_64", "Bot navigation compiler for BSP maps." },
};


void IdTech3Tool_copyHelpCommand( const IdTech3ToolDef& tool ){
	const auto executable = IdTech3Tool_executablePath( tool );
	if ( executable.isEmpty() ) {
		QMessageBox::warning( MainFrame_getWindow(), "Tool not found", StringStream( "Could not find ", tool.name, " executable in install paths or PATH." ).c_str() );
		return;
	}
	const auto command = StringStream( '"', executable.toUtf8().constData(), "\" --help" );
	QGuiApplication::clipboard()->setText( command.c_str() );
	Sys_Status( StringStream( "Copied command: ", tool.name ).c_str() );
}

void IdTech3Tool_runHelp( const IdTech3ToolDef& tool ){
	const auto executable = IdTech3Tool_executablePath( tool );
	if ( executable.isEmpty() ) {
		QMessageBox::warning( MainFrame_getWindow(), "Tool not found",
		                      StringStream( "Could not find ", tool.name, " executable.\\nSearched install directories and system PATH." ).c_str() );
		return;
	}
	if ( !QProcess::startDetached( executable, { "--help" }, QFileInfo( executable ).absolutePath() ) ) {
		QMessageBox::warning( MainFrame_getWindow(), "Launch failed", StringStream( "Failed to start:\\n", executable.toUtf8().constData() ).c_str() );
		return;
	}
	Sys_Status( StringStream( "Launched ", tool.name, " --help" ).c_str() );
}

void IdTech3Tool_openHubDialog(){
	QDialog dialog( MainFrame_getWindow() );
	dialog.setWindowTitle( "Id Tech 3 Tool Center" );
	dialog.setModal( true );
	dialog.resize( 860, 620 );

	auto* layout = new QVBoxLayout( &dialog );
	auto* tabs = new QTabWidget( &dialog );
	layout->addWidget( tabs, 1 );

	auto addTextTab = [tabs]( const char* title, const char* html ){
		auto* text = new QTextBrowser( tabs );
		text->setOpenExternalLinks( true );
		text->setHtml( html );
		tabs->addTab( text, title );
	};

	addTextTab( "Index", R"HTML(
<h2>Id Tech 3 Tool Center</h2>
<p><b>Scope:</b> id Tech 3 and mod workflows (including custom PBR shader pipelines), not Source/VTF.</p>
<p><b>Sections:</b> Index, Features, Updates, Documentation, Links, Download, Credits, Tools.</p>
<p><b>Website:</b> <a href="https://idtech3.com">idtech3.com</a></p>
<p><b>Discussion:</b> add your team Discord/community link in this tab if you want quick access from the editor.</p>
<p><b>Note:</b> compiler and external-tool redistribution policy remains up to tool authors and your project licenses.</p>
)HTML" );
	addTextTab( "Features", R"HTML(
<h3>Implemented</h3>
<ul>
<li>Add menu with direct placement actions and searchable <b>Add Entity...</b> picker.</li>
<li>Hammer-style 4-pane layout preset (3D + Top + Front + Side).</li>
<li>Camera bookmarks: <b>Ctrl+1..5</b> store, <b>Shift+1..5</b> recall.</li>
<li>Id Tech 3 Tool Center with compiler quick actions.</li>
<li>Model add flow hardened to avoid misc_model graph corruption/asserts.</li>
<li>Entity menu presets for Next-Gen <b>volumetric fog</b> and <b>Bullet physics</b> worldspawn keys.</li>
</ul>
<h3>Parity Track </h3>
<ul>
<li>Realtime lighting/material preview modes (legacy + PBR visualization).</li>
<li>Instance workflow equivalent for prefab-like reuse and live preview context.</li>
<li>Improved color picker (RGB/HSV presets, per-map palette behavior).</li>
<li>Editor-object visibility toggles (helpers, tool textures, debug sprites).</li>
<li>Model/material hot-reload and richer particle preview controls.</li>
<li>Gizmo + pivot workflow upgrades and local/global transform toggles.</li>
<li>Face/UV tooling upgrades and clipping/vertex editing QoL refinements.</li>
<li>Advanced browsers (model/material/particle) with better filtering.</li>
</ul>
<h3>Out of Scope / Engine-Specific Mapping</h3>
<ul>
<li>Source-only items (VMF/VTF/VBSP/VVIS/VRAD semantics) are replaced with id Tech 3 equivalents.</li>
<li>Skybox/fog/render-effect previews are targeted to id Tech 3 entity/shader conventions.</li>
</ul>
)HTML" );
	addTextTab( "Updates", R"HTML(
<h3>Recent</h3>
<ul>
<li>Added Add menu, entity picker, and safe model insertion path.</li>
<li>Added Hammer-style 4-pane preset and bookmark camera workflow.</li>
<li>Added Tools menu + Id Tech 3 Tool Center.</li>
<li>Added one-click Next-Gen volumetric fog + Bullet physics worldspawn presets.</li>
</ul>
<h3>Next Up</h3>
<ul>
<li>PBR shader workflow page and validation commands in this hub.</li>
<li>Toolbar toggles for helper visibility and preview channels.</li>
<li>First-pass lighting preview controls in 3D view.</li>
</ul>
)HTML" );
	addTextTab( "Documentation",
	            StringStream(
	                "<h3>Official Documentation</h3>"
	                "<p><a href=\"", c_idTech3DocumentationUrl, "\">", c_idTech3DocumentationUrl, "</a></p>"
	                "<p>Use this for editor and workflow docs hosted on idtech3.com.</p>"
	            ).c_str() );
	addTextTab( "Links",
	            StringStream(
	                "<h3>Official Links</h3>"
	                "<p><a href=\"", c_idTech3LinksUrl, "\">", c_idTech3LinksUrl, "</a></p>"
	                "<p>Hub page for related project/community links on idtech3.com.</p>"
	                "<p>Home: <a href=\"", c_idTech3WebsiteUrl, "\">", c_idTech3WebsiteUrl, "</a></p>"
	            ).c_str() );
	addTextTab( "Download",
	            StringStream( "<p><b>Install path:</b></p><pre>", AppPath_get(), "</pre>"
	                          "<p>Expected binary/tool location for this editor build.</p>"
	                          "<p>Current bundled compilers are id Tech 3 oriented (q3map2/qdata3/mbspc/etc).</p>"
	                          "<p><b>Project site:</b> <a href=\"", c_idTech3WebsiteUrl, "\">", c_idTech3WebsiteUrl, "</a></p>" ).c_str() );
	addTextTab( "Credits", R"HTML(
<p>Implementation adapted for id Tech 3 editing and compile workflows.</p>
<p>Thanks to Radiant maintainers, gamepack maintainers, and community tool authors.</p>
)HTML" );

	auto* toolsTab = new QWidget( tabs );
	auto* toolsLayout = new QVBoxLayout( toolsTab );
	auto* toolsList = new QListWidget( toolsTab );
	toolsLayout->addWidget( toolsList, 1 );
	auto* buttonRow = new QHBoxLayout();
	auto* runHelpButton = new QPushButton( "Run --help", toolsTab );
	auto* copyButton = new QPushButton( "Copy Command", toolsTab );
	buttonRow->addWidget( runHelpButton );
	buttonRow->addWidget( copyButton );
	buttonRow->addStretch( 1 );
	toolsLayout->addLayout( buttonRow );
	for ( std::size_t index = 0; index < std::size( g_idTech3Tools ); ++index )
	{
		const auto& tool = g_idTech3Tools[index];
		auto* item = new QListWidgetItem( StringStream( tool.name, " — ", tool.description ).c_str(), toolsList );
		item->setData( Qt::UserRole, int( index ) );
	}
	toolsList->setCurrentRow( 0 );
	QObject::connect( runHelpButton, &QPushButton::clicked, [toolsList](){
		if ( auto* item = toolsList->currentItem() ) {
			IdTech3Tool_runHelp( g_idTech3Tools[item->data( Qt::UserRole ).toInt()] );
		}
	} );
	QObject::connect( copyButton, &QPushButton::clicked, [toolsList](){
		if ( auto* item = toolsList->currentItem() ) {
			IdTech3Tool_copyHelpCommand( g_idTech3Tools[item->data( Qt::UserRole ).toInt()] );
		}
	} );
	tabs->addTab( toolsTab, "Tools" );

	auto* closeButtons = new QDialogButtonBox( QDialogButtonBox::Close, &dialog );
	layout->addWidget( closeButtons );
	QObject::connect( closeButtons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );

	dialog.exec();
}

void create_add_menu( QMenuBar *menubar ){
	QMenu *menu = menubar->addMenu( "&Add" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( menu, "Entity", "AddEntityByName" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Light", "AddLight" );
	create_menu_item_with_mnemonic( menu, "Player Start", "AddInfoPlayerStart" );
	create_menu_item_with_mnemonic( menu, "Player Deathmatch", "AddInfoPlayerDeathmatch" );
		create_menu_item_with_mnemonic( menu, "Model", "AddMiscModel" );
	menu->addSeparator();

	QMenu* brushMenu = menu->addMenu( "Brush Primitive" );
	brushMenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );
		create_menu_item_with_mnemonic( brushMenu, "Prism", "BrushPrism" );
		create_menu_item_with_mnemonic( brushMenu, "Cone", "BrushCone" );
		create_menu_item_with_mnemonic( brushMenu, "Sphere", "BrushSphere" );
}

void create_view_menu( QMenuBar *menubar, MainFrame::EViewStyle style ){
	// View menu
	QMenu *menu = menubar->addMenu( "Vie&w" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	if ( style == MainFrame::eFloating ) {
		create_check_menu_item_with_mnemonic( menu, "Camera View", "ToggleCamera" );
		create_check_menu_item_with_mnemonic( menu, "XY (Top) View", "ToggleView" );
		create_check_menu_item_with_mnemonic( menu, "XZ (Front) View", "ToggleFrontView" );
		create_check_menu_item_with_mnemonic( menu, "YZ (Side) View", "ToggleSideView" );
	}
	if ( style != MainFrame::eRegular && style != MainFrame::eRegularLeft ) {
		create_menu_item_with_mnemonic( menu, "Console", "ToggleConsole" );
	}
	create_menu_item_with_mnemonic( menu, "Switch to 4-pane layout", "LayoutHammerFourPane" );
	if ( ( style != MainFrame::eRegular && style != MainFrame::eRegularLeft ) || g_Layout_builtInGroupDialog.m_value ) {
		create_menu_item_with_mnemonic( menu, "Texture Browser", "ToggleTextures" );
	}
	create_menu_item_with_mnemonic( menu, "Sky Browser", "SkyBrowser" );
	create_menu_item_with_mnemonic( menu, "Time of Day", "TimeOfDay" );
	create_menu_item_with_mnemonic( menu, "Model Browser", "ToggleModelBrowser" );
	create_menu_item_with_mnemonic( menu, "Entity Inspector", "ToggleEntityInspector" );
	create_menu_item_with_mnemonic( menu, "Layers Browser", "ToggleLayersBrowser" );
	create_menu_item_with_mnemonic( menu, "&Surface Inspector", "SurfaceInspector" );
	create_menu_item_with_mnemonic( menu, "Entity List", "ToggleEntityList" );
	create_menu_item_with_mnemonic( menu, "Scenegraph Inspector", "ToggleScenegraphInspector" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Open WYSIWYG Workspace", "OpenWysiwygWorkspace" );
	create_menu_item_with_mnemonic( menu, "Inspector", "ToggleExperimentalProperties" );
	create_menu_item_with_mnemonic( menu, "Viewport Preview", "ToggleExperimentalPreview" );
	create_menu_item_with_mnemonic( menu, "Asset Browser", "ToggleExperimentalAssets" );
	create_menu_item_with_mnemonic( menu, "Outliner", "ToggleExperimentalUSD" );
	create_menu_item_with_mnemonic( menu, "History", "ToggleExperimentalHistory" );
	create_menu_item_with_mnemonic( menu, "Live Sync", "ToggleExperimentalSync" );
	create_menu_item_with_mnemonic( menu, "Entity Palette", "ToggleExperimentalECS" );

	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Camera" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Focus on Selected", "CameraFocusOnSelected" );
		create_menu_item_with_mnemonic( submenu, "Cycle &Background (Alt+B)", "CameraCycleBackground" );
		create_menu_item_with_mnemonic( submenu, "&Center", "CenterView" );
		create_menu_item_with_mnemonic( submenu, "&Up Floor", "UpFloor" );
		create_menu_item_with_mnemonic( submenu, "&Down Floor", "DownFloor" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Far Clip Plane In", "CubicClipZoomIn" );
		create_menu_item_with_mnemonic( submenu, "Far Clip Plane Out", "CubicClipZoomOut" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Next leak spot", "NextLeakSpot" );
		create_menu_item_with_mnemonic( submenu, "Previous leak spot", "PrevLeakSpot" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Store Bookmark 1", "CameraStoreBookmark1" );
		create_menu_item_with_mnemonic( submenu, "Store Bookmark 2", "CameraStoreBookmark2" );
		create_menu_item_with_mnemonic( submenu, "Store Bookmark 3", "CameraStoreBookmark3" );
		create_menu_item_with_mnemonic( submenu, "Store Bookmark 4", "CameraStoreBookmark4" );
		create_menu_item_with_mnemonic( submenu, "Store Bookmark 5", "CameraStoreBookmark5" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Recall Bookmark 1", "CameraRecallBookmark1" );
		create_menu_item_with_mnemonic( submenu, "Recall Bookmark 2", "CameraRecallBookmark2" );
		create_menu_item_with_mnemonic( submenu, "Recall Bookmark 3", "CameraRecallBookmark3" );
		create_menu_item_with_mnemonic( submenu, "Recall Bookmark 4", "CameraRecallBookmark4" );
		create_menu_item_with_mnemonic( submenu, "Recall Bookmark 5", "CameraRecallBookmark5" );
		//cameramodel is not implemented in instances, thus useless
//		submenu->addSeparator();
//		create_menu_item_with_mnemonic( submenu, "Look Through Selected", "LookThroughSelected" );
//		create_menu_item_with_mnemonic( submenu, "Look Through Camera", "LookThroughCamera" );
	}
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Orthographic" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		if ( style == MainFrame::eRegular || style == MainFrame::eRegularLeft || style == MainFrame::eFloating ) {
			create_menu_item_with_mnemonic( submenu, "&Next (XY, XZ, YZ)", "NextView" );
			create_menu_item_with_mnemonic( submenu, "XY (Top)", "ViewTop" );
			create_menu_item_with_mnemonic( submenu, "XZ (Front)", "ViewFront" );
			create_menu_item_with_mnemonic( submenu, "YZ (Side)", "ViewSide" );
			submenu->addSeparator();
		}
		else{
			create_menu_item_with_mnemonic( submenu, "Center on Selected", "CenterXYView" );
		}

		create_menu_item_with_mnemonic( submenu, "Focus on Selected", "XYFocusOnSelected" );
		create_menu_item_with_mnemonic( submenu, "Center on Selected", "CenterXYView" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "&XY 100%", "Zoom100" );
		create_menu_item_with_mnemonic( submenu, "XY Zoom &In", "ZoomIn" );
		create_menu_item_with_mnemonic( submenu, "XY Zoom &Out", "ZoomOut" );
	}

	menu->addSeparator();

	{
		QMenu* submenu = menu->addMenu( "Show" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_check_menu_item_with_mnemonic( submenu, "Show Entity &Angles", "ShowAngles" );
		create_check_menu_item_with_mnemonic( submenu, "Show Entity &Names", "ShowNames" );
		create_check_menu_item_with_mnemonic( submenu, "Show Light Radiuses", "ShowLightRadiuses" );
		create_check_menu_item_with_mnemonic( submenu, "Show Entity Boxes", "ShowBboxes" );
		create_check_menu_item_with_mnemonic( submenu, "Show Entity Connections", "ShowConnections" );

		submenu->addSeparator();

		create_check_menu_item_with_mnemonic( submenu, "Show 2D Size Info", "ShowSize2d" );
		create_check_menu_item_with_mnemonic( submenu, "Show 3D Size Info", "ShowSize3d" );
		create_check_menu_item_with_mnemonic( submenu, "Show Crosshair", "ToggleCrosshairs" );
		create_check_menu_item_with_mnemonic( submenu, "Show Grid", "ToggleGrid" );
		create_check_menu_item_with_mnemonic( submenu, "Show Blocks", "ShowBlocks" );
		create_check_menu_item_with_mnemonic( submenu, "Show C&oordinates", "ShowCoordinates" );
		create_check_menu_item_with_mnemonic( submenu, "Show Window Outline", "ShowWindowOutline" );
		create_check_menu_item_with_mnemonic( submenu, "Show Axes", "ShowAxes" );
		create_check_menu_item_with_mnemonic( submenu, "Show 2D Workzone", "ShowWorkzone2d" );
		create_check_menu_item_with_mnemonic( submenu, "Show 3D Workzone", "ShowWorkzone3d" );
		create_check_menu_item_with_mnemonic( submenu, "Show Renderer Stats", "ShowStats" );
	}

	{
		QMenu* submenu = menu->addMenu( "Filter" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		Filters_constructMenu( submenu );
	}
	menu->addSeparator();
	{
		create_check_menu_item_with_mnemonic( menu, "Hide Selected", "HideSelected" );
		create_menu_item_with_mnemonic( menu, "Show Hidden", "ShowHidden" );
	}
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Region" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "&Off", "RegionOff" );
		create_menu_item_with_mnemonic( submenu, "&Set XY", "RegionSetXY" );
		create_menu_item_with_mnemonic( submenu, "Set &Brush", "RegionSetBrush" );
		create_check_menu_item_with_mnemonic( submenu, "Set Se&lection", "RegionSetSelection" );
	}

	//command_connect_accelerator( "CenterXYView" );
}

void create_selection_menu( QMenuBar *menubar ){
	// Selection menu
	QMenu *menu = menubar->addMenu( "M&odify" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );
	{
		QMenu* submenu = menu->addMenu( "Components" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_check_menu_item_with_mnemonic( submenu, "&Primitives", "DragPrimitives" );
		create_check_menu_item_with_mnemonic( submenu, "&Edges", "DragEdges" );
		create_check_menu_item_with_mnemonic( submenu, "&Vertices", "DragVertices" );
		create_check_menu_item_with_mnemonic( submenu, "&Faces", "DragFaces" );
	}

	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Snap To Grid", "SnapToGrid" );

	menu->addSeparator();

	{
		QMenu* submenu = menu->addMenu( "Nudge" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Nudge Left", "SelectNudgeLeft" );
		create_menu_item_with_mnemonic( submenu, "Nudge Right", "SelectNudgeRight" );
		create_menu_item_with_mnemonic( submenu, "Nudge Up", "SelectNudgeUp" );
		create_menu_item_with_mnemonic( submenu, "Nudge Down", "SelectNudgeDown" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Nudge +Z", "MoveSelectionUP" );
		create_menu_item_with_mnemonic( submenu, "Nudge -Z", "MoveSelectionDOWN" );
	}
	{
		QMenu* submenu = menu->addMenu( "Rotate" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Rotate X", "RotateSelectionX" );
		create_menu_item_with_mnemonic( submenu, "Rotate Y", "RotateSelectionY" );
		create_menu_item_with_mnemonic( submenu, "Rotate Z", "RotateSelectionZ" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Rotate Clockwise", "RotateSelectionClockwise" );
		create_menu_item_with_mnemonic( submenu, "Rotate Anticlockwise", "RotateSelectionAnticlockwise" );
	}
	{
		QMenu* submenu = menu->addMenu( "Flip" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Flip &X", "MirrorSelectionX" );
		create_menu_item_with_mnemonic( submenu, "Flip &Y", "MirrorSelectionY" );
		create_menu_item_with_mnemonic( submenu, "Flip &Z", "MirrorSelectionZ" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Flip Horizontally", "MirrorSelectionHorizontally" );
		create_menu_item_with_mnemonic( submenu, "Flip Vertically", "MirrorSelectionVertically" );
	}
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Arbitrary rotation", "ArbitraryRotation" );
	create_menu_item_with_mnemonic( menu, "Arbitrary scale", "ArbitraryScale" );
	create_menu_item_with_mnemonic( menu, "&Transform (Position/Rotation/Scale)", "TransformDialog" );
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Repeat" );

		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

		create_menu_item_with_mnemonic( submenu, "Repeat Transforms", "RepeatTransforms" );

		using SetTextCB = PointerCaller<QAction, void(const char*), +[]( QAction *action, const char *text ){ action->setText( text ); }>;
		const auto addItem = [submenu]<SelectionSystem::EManipulatorMode mode>() -> SetTextCB {
			return SetTextCB( create_menu_item_with_mnemonic( submenu, "", makeCallbackF( +[](){ GlobalSelectionSystem().resetTransforms( mode ); } ) ) );
		};
		SelectionSystem_connectTransformsCallbacks( { addItem.operator()<SelectionSystem::eTranslate>(),
		                                              addItem.operator()<SelectionSystem::eRotate>(),
		                                              addItem.operator()<SelectionSystem::eScale>(),
		                                              addItem.operator()<SelectionSystem::eSkew>() } );
		GlobalSelectionSystem().resetTransforms(); // init texts immediately

		create_menu_item_with_mnemonic( submenu, "Reset Transforms", "ResetTransforms" );
	}
}

void create_bsp_menu( QMenuBar *menubar ){
	// BSP menu
	QMenu *menu = menubar->addMenu( "&Build" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "Customize", "BuildMenuCustomize" );
	create_menu_item_with_mnemonic( menu, "Run recent build", "Build_runRecentExecutedBuild" );

	menu->addSeparator();

	menu->setToolTipsVisible( true );
	Build_constructMenu( menu );

	g_bsp_menu = menu;
}

void create_grid_menu( QMenuBar *menubar ){
	// Grid menu
	QMenu *menu = menubar->addMenu( "&Grid" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	Grid_constructMenu( menu );
}

void create_misc_menu( QMenuBar *menubar ){
	// Misc menu
	QMenu *menu = menubar->addMenu( "M&isc" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );
#if 0
	create_menu_item_with_mnemonic( menu, "&Benchmark", makeCallbackF( GlobalCamera_Benchmark ) );
#endif
	create_colours_menu( menu );

	create_menu_item_with_mnemonic( menu, "Find brush", "FindBrush" );
	create_menu_item_with_mnemonic( menu, "Map Info", "MapInfo" );
	create_menu_item_with_mnemonic( menu, "&Refresh models", "RefreshReferences" );
	create_menu_item_with_mnemonic( menu, "Import USD structure", "ImportUSDStructure" );
	create_menu_item_with_mnemonic( menu, "Export to USDA", "ExportToUSDA" );
	create_menu_item_with_mnemonic( menu, "Import Maya ASCII", "ImportMayaASCII" );
	create_menu_item_with_mnemonic( menu, "Export to Maya ASCII", "ExportToMayaASCII" );
	create_menu_item_with_mnemonic( menu, "Set 2D &Background image", makeCallbackF( WXY_SetBackgroundImage ) );
	create_menu_item_with_mnemonic( menu, "Fullscreen", "Fullscreen" );
	create_menu_item_with_mnemonic( menu, "Maximize view", "MaximizeView" );
}

void create_entity_menu( QMenuBar *menubar ){
	// Entity menu
	QMenu *menu = menubar->addMenu( "E&ntity" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	Entity_constructMenu( menu );
}

void create_brush_menu( QMenuBar *menubar ){
	// Brush menu
	QMenu *menu = menubar->addMenu( "Brush" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	Brush_constructMenu( menu );
}

void create_patch_menu( QMenuBar *menubar ){
	// Curve menu
	QMenu *menu = menubar->addMenu( "&Curve" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	Patch_constructMenu( menu );
}

void create_splines_menu( QMenuBar *menubar ){
	QMenu *menu = menubar->addMenu( "Spli&nes" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "&Add Spline", "AddSpline" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Spline &Array", "SplineArray" );
}

void create_tools_menu( QMenuBar *menubar ){
	QMenu *menu = menubar->addMenu( "&Tools" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

	create_menu_item_with_mnemonic( menu, "Tool Center", "OpenIdTech3ToolCenter" );
	create_menu_item_with_mnemonic( menu, "Music Player / Playlist Editor", "OpenAudioWorkbench" );
	create_menu_item_with_mnemonic( menu, "Video Player", "OpenCinematicPlayer" );
	create_menu_item_with_mnemonic( menu, "Spreadsheet Editor", "OpenSpreadsheetWorkbench" );
	create_menu_item_with_mnemonic( menu, "Python Script Editor", "OpenPythonScript" );
	create_menu_item_with_mnemonic( menu, "AI Assistant", "OpenAIAssistant" );
	menu->addSeparator();
	create_menu_item_with_mnemonic( menu, "Q3Map2++ Help", "ToolQ3Map2Help" );
	create_menu_item_with_mnemonic( menu, "QData3++ Help", "ToolQData3Help" );
	create_menu_item_with_mnemonic( menu, "Q2Map++ Help", "ToolQ2MapHelp" );
	create_menu_item_with_mnemonic( menu, "MBSPC++ Help", "ToolMBSPCHelp" );
	menu->addSeparator();
	{
		QMenu* submenu = menu->addMenu( "Lua" );
		submenu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );
		create_menu_item_with_mnemonic( submenu, "Edit props.lua", "LuaEditProps" );
		create_menu_item_with_mnemonic( submenu, "Edit entities.lua", "LuaEditEntities" );
		create_menu_item_with_mnemonic( submenu, "Edit items.lua", "LuaEditItems" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Edit main.lua", "LuaEditMain" );
		create_menu_item_with_mnemonic( submenu, "Edit objectives.lua", "LuaEditObjectives" );
		submenu->addSeparator();
		create_menu_item_with_mnemonic( submenu, "Edit props.lua externally", "LuaEditPropsExternal" );
	}
}

void create_help_menu( QMenuBar *menubar ){
	// Help menu
	QMenu *menu = menubar->addMenu( "&Help" );

	menu->setTearOffEnabled( g_Layout_enableDetachableMenus.m_value );

//	create_menu_item_with_mnemonic( menu, "Manual", "OpenManual" );

	// this creates all the per-game drop downs for the game pack helps
	// it will take care of hooking the Sys_OpenURL calls etc.
	create_game_help_menu( menu );

	create_menu_item_with_mnemonic( menu, "Bug report", makeCallbackF( OpenBugReportURL ) );
	create_menu_item_with_mnemonic( menu, "Check for update", "CheckForUpdate" );
	create_menu_item_with_mnemonic( menu, "&About", makeCallbackF( DoAbout ) );
}

void create_main_menu( QMenuBar *menubar, MainFrame::EViewStyle style ){
	create_file_menu( menubar );
	create_edit_menu( menubar );
	create_add_menu( menubar );
	create_view_menu( menubar, style );
	create_selection_menu( menubar );
	create_bsp_menu( menubar );
	create_grid_menu( menubar );
	create_misc_menu( menubar );
	create_entity_menu( menubar );
	create_brush_menu( menubar );
	if ( !string_equal( g_pGameDescription->getKeyValue( "no_patch" ), "1" ) )
		create_patch_menu( menubar );
	if ( Layout_experimentalFeaturesEnabled() )
		create_splines_menu( menubar );
	create_tools_menu( menubar );
	create_plugins_menu( menubar );
	create_layout_menu( menubar, style );
	create_help_menu( menubar );
}


void Patch_registerShortcuts(){
	command_connect_accelerator( "InvertCurveTextureX" );
	command_connect_accelerator( "InvertCurveTextureY" );
	command_connect_accelerator( "PatchInsertInsertColumn" );
	command_connect_accelerator( "PatchInsertInsertRow" );
	command_connect_accelerator( "PatchDeleteLastColumn" );
	command_connect_accelerator( "PatchDeleteLastRow" );
	command_connect_accelerator( "NaturalizePatch" );
}

void Manipulators_registerShortcuts(){
	command_connect_accelerator( "MouseRotateOrScale" );
	command_connect_accelerator( "MouseDragOrTransform" );
}

void TexdefNudge_registerShortcuts(){
	command_connect_accelerator( "TexRotateClock" );
	command_connect_accelerator( "TexRotateCounter" );
	command_connect_accelerator( "TexScaleUp" );
	command_connect_accelerator( "TexScaleDown" );
	command_connect_accelerator( "TexScaleLeft" );
	command_connect_accelerator( "TexScaleRight" );
	command_connect_accelerator( "TexShiftUp" );
	command_connect_accelerator( "TexShiftDown" );
	command_connect_accelerator( "TexShiftLeft" );
	command_connect_accelerator( "TexShiftRight" );
}

void SelectNudge_registerShortcuts(){
	command_connect_accelerator( "MoveSelectionDOWN" );
	command_connect_accelerator( "MoveSelectionUP" );
	command_connect_accelerator( "SelectNudgeLeft" );
	command_connect_accelerator( "SelectNudgeRight" );
	command_connect_accelerator( "SelectNudgeUp" );
	command_connect_accelerator( "SelectNudgeDown" );
}

void SnapToGrid_registerShortcuts(){
	command_connect_accelerator( "SnapToGrid" );
}

void SelectByType_registerShortcuts(){
	command_connect_accelerator( "SelectAllOfType" );
}

void SurfaceInspector_registerShortcuts(){
	command_connect_accelerator( "FitTexture" );
	command_connect_accelerator( "FitTextureWidth" );
	command_connect_accelerator( "FitTextureHeight" );
	command_connect_accelerator( "FitTextureWidthOnly" );
	command_connect_accelerator( "FitTextureHeightOnly" );
	command_connect_accelerator( "TextureProjectAxial" );
	command_connect_accelerator( "TextureProjectOrtho" );
	command_connect_accelerator( "TextureProjectCam" );
}

void TexBro_registerShortcuts(){
	toggle_add_accelerator( "SearchFromStart" );
}

void Misc_registerShortcuts(){
	command_connect_accelerator( "FrameSelection" );
	command_connect_accelerator( "Redo2" );
	command_connect_accelerator( "UnSelectSelection2" );
	command_connect_accelerator( "DeleteSelection2" );
	command_connect_accelerator( "DeleteSelection3" );
}


void register_shortcuts(){
//	Patch_registerShortcuts();
	Grid_registerShortcuts();
//	XYWnd_registerShortcuts();
	CamWnd_registerShortcuts();
	Manipulators_registerShortcuts();
	SurfaceInspector_registerShortcuts();
	TexdefNudge_registerShortcuts();
//	SelectNudge_registerShortcuts();
//	SnapToGrid_registerShortcuts();
//	SelectByType_registerShortcuts();
	TexBro_registerShortcuts();
	Misc_registerShortcuts();
	Entity_registerShortcuts();
	Layers_registerShortcuts();
}

void File_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Open an existing map", "file_open.png", "OpenMap" );
	toolbar_append_button( toolbar, "Save the active map", "file_save.png", "SaveMap" );
	toolbar_append_button( toolbar, "Make Room", "selection_makeroom.png", "CSGroom" );
}

void UndoRedo_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Undo", "undo.png", "Undo" );
	toolbar_append_button( toolbar, "Redo", "redo.png", "Redo" );
}

void RotateFlip_constructToolbar( QToolBar* toolbar ){
//	toolbar_append_button( toolbar, "x-axis Flip", "brush_flipx.png", "MirrorSelectionX" );
//	toolbar_append_button( toolbar, "x-axis Rotate", "brush_rotatex.png", "RotateSelectionX" );
//	toolbar_append_button( toolbar, "y-axis Flip", "brush_flipy.png", "MirrorSelectionY" );
//	toolbar_append_button( toolbar, "y-axis Rotate", "brush_rotatey.png", "RotateSelectionY" );
//	toolbar_append_button( toolbar, "z-axis Flip", "brush_flipz.png", "MirrorSelectionZ" );
//	toolbar_append_button( toolbar, "z-axis Rotate", "brush_rotatez.png", "RotateSelectionZ" );
	toolbar_append_button( toolbar, "Flip Horizontally", "brush_flip_hor.png", "MirrorSelectionHorizontally" );
	toolbar_append_button( toolbar, "Flip Vertically", "brush_flip_vert.png", "MirrorSelectionVertically" );

	toolbar_append_button( toolbar, "Rotate Anticlockwise", "brush_rotate_anti.png", "RotateSelectionAnticlockwise" );
	toolbar_append_button( toolbar, "Rotate Clockwise", "brush_rotate_clock.png", "RotateSelectionClockwise" );
}

void Select_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Select touching", "selection_selecttouching.png", "SelectTouching" );
	toolbar_append_button( toolbar, "Select inside", "selection_selectinside.png", "SelectInside" );
}

void CSG_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "CSG Subtract", "selection_csgsubtract.png", "CSGSubtract" );
	toolbar_append_button( toolbar, "CSG Wrap Merge", "selection_csgmerge.png", "CSGWrapMerge" );
	toolbar_append_button( toolbar, "Room", "selection_makeroom.png", "CSGroom" );
	toolbar_append_button( toolbar, "CSG Tool", "ellipsis.png", "CSGTool" );
	toolbar_append_button( toolbar, "Auto-Caulk Selected", "f-caulk.png", "AutoCaulkSelected" );
}

void ComponentModes_constructToolbar( QToolBar* toolbar ){
	toolbar_append_toggle_button( toolbar, "Select Primitives", "status_brush.png", "DragPrimitives" );
	toolbar_append_toggle_button( toolbar, "Select Vertices", "modify_vertices.png", "DragVertices" );
	toolbar_append_toggle_button( toolbar, "Select Edges", "modify_edges.png", "DragEdges" );
	toolbar_append_toggle_button( toolbar, "Select Faces", "modify_faces.png", "DragFaces" );
}

void XYWnd_constructToolbar( QToolBar* toolbar ){
	toolbar_append_button( toolbar, "Change views", "view_change.png", "NextView" );
}

void Manipulators_constructToolbar( QToolBar* toolbar ){
	toolbar_append_toggle_button( toolbar, "Resize", "select_mouseresize.png", "MouseDrag" );
	toolbar_append_toggle_button( toolbar, "Clipper", "select_clipper.png", "ToggleClipper" );
	toolbar_append_toggle_button( toolbar, "Translate", "select_mousetranslate.png", "MouseTranslate" );
	toolbar_append_toggle_button( toolbar, "Rotate", "select_mouserotate.png", "MouseRotate" );
	toolbar_append_toggle_button( toolbar, "Scale", "select_mousescale.png", "MouseScale" );
	toolbar_append_toggle_button( toolbar, "Transform", "select_mousetransform.png", "MouseTransform" );
//	toolbar_append_toggle_button( toolbar, "Build", "select_mouserotate.png", "MouseBuild" );
	toolbar_append_toggle_button( toolbar, "UV Tool", "select_mouseuv.png", "MouseUV" );
}

extern CopiedString g_toolbarHiddenButtons;

#include <QSvgGenerator>
void create_main_toolbar( QToolBar *toolbar,  MainFrame::EViewStyle style ){
	QSvgGenerator dummy; // reference symbol, so that Qt5Svg.dll required dependency is explicit, also install-dlls-msys2-mingw.sh will find it

 	File_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	UndoRedo_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	RotateFlip_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	Select_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	CSG_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	ComponentModes_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	if ( style != MainFrame::eSplit ) {
		XYWnd_constructToolbar( toolbar );
		toolbar_append_separator( toolbar );
	}

	CamWnd_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	Manipulators_constructToolbar( toolbar );
	toolbar_append_separator( toolbar );

	if ( !string_equal( g_pGameDescription->getKeyValue( "no_patch" ), "1" ) ) {
		Patch_constructToolbar( toolbar );
		toolbar_append_separator( toolbar );
	}

	toolbar_append_toggle_button( toolbar, "Texture Lock", "texture_lock.png", "TogTexLock" );
	toolbar_append_toggle_button( toolbar, "Texture Vertex Lock", "texture_vertexlock.png", "TogTexVertexLock" );
	toolbar_append_separator( toolbar );

	toolbar_append_button( toolbar, "Entities", "entities.png", "ToggleEntityInspector" );
	// disable the console and texture button in the regular layouts
	if ( style != MainFrame::eRegular && style != MainFrame::eRegularLeft ) {
		toolbar_append_button( toolbar, "Console", "console.png", "ToggleConsole" );
	}
	if ( ( style != MainFrame::eRegular && style != MainFrame::eRegularLeft ) || g_Layout_builtInGroupDialog.m_value ) {
		toolbar_append_button( toolbar, "Texture Browser", "texture_browser.png", "ToggleTextures" );
	}

		toolbar_append_button( toolbar, "Light Inspector", "lightinspector.png", "SurfaceInspector" );

	toolbar_append_separator( toolbar );
	toolbar_append_button( toolbar, "Refresh Models", "refresh_models.png", "RefreshReferences" );
}


namespace
{
QWidget* g_saveStatusWidget = nullptr;
QProgressBar* g_saveStatusBar = nullptr;
QLabel* g_saveStatusLabel = nullptr;
}

void SaveStatus_notifySaving(){
	if ( g_saveStatusBar && g_saveStatusLabel ) {
		g_saveStatusBar->setRange( 0, 0 ); // indeterminate
		g_saveStatusBar->setVisible( true );
		g_saveStatusLabel->setText( "Saving..." );
		g_saveStatusLabel->setToolTip( "Saving map..." );
		g_saveStatusWidget->setVisible( true );
		QApplication::processEvents();
	}
}

void SaveStatus_notifySaved( const char* filename ){
	if ( g_saveStatusBar && g_saveStatusLabel ) {
		g_saveStatusBar->setRange( 0, 100 );
		g_saveStatusBar->setValue( 100 );
		g_saveStatusBar->setVisible( true );
		const QDateTime now = QDateTime::currentDateTime();
		const QString timeStr = now.toString( "hh:mm:ss" );
		const QString dateStr = now.toString( "yyyy-MM-dd" );
		g_saveStatusLabel->setText( "Saved" );
		g_saveStatusLabel->setToolTip(
			QString( "Last saved at %1 on %2\n%3" )
				.arg( timeStr )
				.arg( dateStr )
				.arg( filename ? QString::fromUtf8( path_get_filename_start( filename ) ) : QString() )
		);
		g_saveStatusWidget->setToolTip(
			QString( "Last saved at %1 on %2\n%3" )
				.arg( timeStr )
				.arg( dateStr )
				.arg( filename ? QString::fromUtf8( path_get_filename_start( filename ) ) : QString() )
		);
		g_saveStatusWidget->setVisible( true );
	}
}

void create_main_statusbar( QStatusBar *statusbar, QLabel *pStatusLabel[c_status__count] ){
	statusbar->setSizeGripEnabled( false );
	{
		g_saveStatusWidget = new QWidget;
		auto *hbox = new QHBoxLayout( g_saveStatusWidget );
		hbox->setContentsMargins( 0, 0, 8, 0 );
		hbox->setSpacing( 4 );
		g_saveStatusBar = new QProgressBar;
		g_saveStatusBar->setRange( 0, 100 );
		g_saveStatusBar->setValue( 0 );
		g_saveStatusBar->setTextVisible( false );
		g_saveStatusBar->setMinimumWidth( 60 );
		g_saveStatusBar->setMaximumWidth( 80 );
		g_saveStatusBar->setFixedHeight( 12 );
		g_saveStatusBar->setVisible( false );
		g_saveStatusLabel = new QLabel( "—" );
		g_saveStatusLabel->setMinimumWidth( g_saveStatusLabel->fontMetrics().horizontalAdvance( "Saved" ) );
		g_saveStatusLabel->setToolTip( "Save status" );
		hbox->addWidget( g_saveStatusLabel );
		hbox->addWidget( g_saveStatusBar );
		g_saveStatusWidget->setToolTip( "Save status — last saved time shown in tooltip" );
		statusbar->addWidget( g_saveStatusWidget );
	}
	{
		auto *label = new QLabel;
		statusbar->addPermanentWidget( label, 1 );
		pStatusLabel[c_status_command] = label;
	}

	for ( int i = 1; i < c_status__count; ++i )
	{
		if( i == c_status_brushcount ){
			auto *widget = new QWidget;
			auto *hbox = new QHBoxLayout( widget );
			hbox->setMargin( 0 );
			statusbar->addPermanentWidget( widget, 0 );
			const char* imgs[3] = { "status_brush.png", "status_patch.png", "status_entity.png" };
			for( ; i < c_status_brushcount + 3; ++i ){
				auto *label = new QLabel();
				auto pixmap = new_local_image( imgs[i - c_status_brushcount] );
				pixmap.setDevicePixelRatio( label->devicePixelRatio() );
				label->setPixmap( pixmap.scaledToHeight( 16 * label->devicePixelRatio() * label->logicalDpiX() / 96, Qt::TransformationMode::SmoothTransformation ) );
				hbox->addWidget( label );

				label = new QLabel();
				label->setMinimumWidth( label->fontMetrics().horizontalAdvance( "99999" ) );
				hbox->addWidget( label );
				pStatusLabel[i] = label;
			}
			--i;
		}
		else{
			auto *label = new QLabel;
			if( i == c_status_grid ){
				statusbar->addPermanentWidget( label, 0 );
				label->setToolTip( " <b>G</b>: <u>G</u>rid size<br> <b>F</b>: map <u>F</u>ormat<br> <b>C</b>: camera <u>C</u>lip distance <br> <b>L</b>: texture <u>L</u>ock" );
			}
			else if( i == c_status_brushsize ){
				statusbar->addPermanentWidget( label, 0 );
				label->setToolTip( "Size of selection bounds (W×H×D)" );
			}
			else
				statusbar->addPermanentWidget( label, 1 );
			pStatusLabel[i] = label;
		}
	}
}

SignalHandlerId XYWindowDestroyed_connect( const SignalHandler& handler ){
	return g_pParentWnd->GetXYWnd()->onDestroyed.connectFirst( handler );
}

void XYWindowDestroyed_disconnect( SignalHandlerId id ){
	g_pParentWnd->GetXYWnd()->onDestroyed.disconnect( id );
}

MouseEventHandlerId XYWindowMouseDown_connect( const MouseEventHandler& handler ){
	return g_pParentWnd->GetXYWnd()->onMouseDown.connectFirst( handler );
}

void XYWindowMouseDown_disconnect( MouseEventHandlerId id ){
	g_pParentWnd->GetXYWnd()->onMouseDown.disconnect( id );
}

// =============================================================================
// MainFrame class

MainFrame* g_pParentWnd = 0;

QWidget* MainFrame_getWindow(){
	return g_pParentWnd == 0? 0 : g_pParentWnd->m_window;
}

MainFrame::MainFrame() : m_idleRedrawStatusText( RedrawStatusTextCaller( *this ) ){
	Create();
}

MainFrame::~MainFrame(){
	SaveGuiState();

	m_window->hide(); // hide to avoid resize events during content deletion

	Shutdown();

	delete m_window;
}

void MainFrame::SetActiveXY( XYWnd* p ){
	if ( m_pActiveXY ) {
		m_pActiveXY->SetActive( false );
	}

	m_pActiveXY = p;

	if ( m_pActiveXY ) {
		m_pActiveXY->SetActive( true );
	}
}

#ifdef WIN32
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif
void MainFrame_toggleFullscreen(){
	QWidget *w = MainFrame_getWindow();
#ifdef WIN32 // https://doc.qt.io/qt-5.15/windows-issues.html#fullscreen-opengl-based-windows
	QWindowsWindowFunctions::setHasBorderInFullScreen( w->windowHandle(), true );
#endif
	w->setWindowState( w->windowState() ^ Qt::WindowState::WindowFullScreen );
}

class MaximizeView
{
	bool m_maximized{};
	QList<int> m_vSplitSizes;
	QList<int> m_vSplit2Sizes;
	QList<int> m_hSplitSizes;

	void maximize(){
		m_maximized = true;
		m_vSplitSizes = g_pParentWnd->m_vSplit->sizes();
		m_vSplit2Sizes = g_pParentWnd->m_vSplit2->sizes();
		m_hSplitSizes = g_pParentWnd->m_hSplit->sizes();

		const QPoint cursor = g_pParentWnd->m_hSplit->mapFromGlobal( QCursor::pos() );

		if( cursor.y() < m_vSplitSizes[0] )
			g_pParentWnd->m_vSplit->setSizes( { 9999, 0 } );
		else
			g_pParentWnd->m_vSplit->setSizes( { 0, 9999 } );

		if( cursor.y() < m_vSplit2Sizes[0] )
			g_pParentWnd->m_vSplit2->setSizes( { 9999, 0 } );
		else
			g_pParentWnd->m_vSplit2->setSizes( { 0, 9999 } );

		if( cursor.x() < m_hSplitSizes[0] )
			g_pParentWnd->m_hSplit->setSizes( { 9999, 0 } );
		else
			g_pParentWnd->m_hSplit->setSizes( { 0, 9999 } );
	}
public:
	void unmaximize(){
		if( m_maximized ){
			m_maximized = false;
			g_pParentWnd->m_vSplit->setSizes( m_vSplitSizes );
			g_pParentWnd->m_vSplit2->setSizes( m_vSplit2Sizes );
			g_pParentWnd->m_hSplit->setSizes( m_hSplitSizes );
		}
	}
	void toggle(){
		m_maximized ? unmaximize() : maximize();
	}
};

MaximizeView g_maximizeview;

void Maximize_View(){
	if( g_pParentWnd != 0 && g_pParentWnd->m_vSplit != 0 && g_pParentWnd->m_vSplit2 != 0 && g_pParentWnd->m_hSplit != 0 )
		g_maximizeview.toggle();
}



class RadiantQMainWindow : public QMainWindow
{
protected:
	void closeEvent( QCloseEvent *event ) override {
		event->ignore();
		if ( g_minimizeToTray && TrayIcon_isAvailable() && g_trayIconEnabled ) {
			hide();
		}
		else {
			Exit();
		}
	}
	bool event( QEvent *event ) override {
		if( event->type() == QEvent::ShortcutOverride && !QGuiApplication::mouseButtons().testFlag( Qt::MouseButton::NoButton ) ){
			event->accept(); // block shortcuts while mouse buttons are pressed
		}
		return QMainWindow::event( event );
	}
public:
	QMenu* createPopupMenu() override {
		auto *menu = QMainWindow::createPopupMenu();
		if( menu == nullptr )
			menu = new QMenu;
		else
			menu->addSeparator();
		toolbar_construct_control_menu( menu );
		return menu;
	}
};


QSplashScreen *create_splash(){
	auto *splash = new QSplashScreen( new_local_image( "splash.png" ) );
	splash->show();
	return splash;
}

static QSplashScreen *splash_screen = 0;

void show_splash(){
	splash_screen = create_splash();

	process_gui();
}

void hide_splash(){
//.	splash_screen->finish();
	delete splash_screen;
}


void user_shortcuts_init(){
	const auto path = StringStream( SettingsPath_get(), g_pGameDescription->mGameFile, '/' );
	LoadCommandMap( path );
	SaveCommandMap( path );
}

void user_shortcuts_save(){
	const auto path = StringStream( SettingsPath_get(), g_pGameDescription->mGameFile, '/' );
	SaveCommandMap( path );
}


void MainFrame::Create(){
	QMainWindow *window = m_window = new RadiantQMainWindow();

	GlobalWindowObservers_connectTopLevel( window );

	/* GlobalCommands_insert plugins commands */
	GetPlugInMgr().Init( window );
	/* then load shortcuts cfg */
	user_shortcuts_init();

	GlobalPressedKeys_connect( window );
	GlobalShortcuts_setWidget( window );
	register_shortcuts();

	m_nCurrentStyle = (EViewStyle)g_Layout_viewStyle.m_value;

	create_main_menu( window->menuBar(), CurrentStyle() );

	{
		{
			auto *toolbar = new QToolBar( "Main Toolbar" );
			toolbar->setObjectName( "Main_Toolbar" ); // required for proper state save/restore
			window->addToolBar( Qt::ToolBarArea::TopToolBarArea, toolbar );
			create_main_toolbar( toolbar, CurrentStyle() );
		}
		{
			auto *toolbar = new QToolBar( "Filter Toolbar" );
			toolbar->setObjectName( "Filter_Toolbar" ); // required for proper state save/restore
			window->addToolBar( Qt::ToolBarArea::RightToolBarArea, toolbar );
			create_filter_toolbar( toolbar );
		}
		{
			auto *toolbar = new QToolBar( "Plugin Toolbar" );
			toolbar->setObjectName( "Plugin_Toolbar" ); // required for proper state save/restore
			window->addToolBar( Qt::ToolBarArea::RightToolBarArea, toolbar );
			create_plugin_toolbar( toolbar );
		}
	}

	create_main_statusbar( window->statusBar(), m_statusLabel );

	GroupDialog_constructWindow( window );

	g_page_entity = GroupDialog_addPage( "Entities", EntityInspector_constructWindow( GroupDialog_getWindow() ), RawStringExportCaller( "Entities" ) );

	auto* consoleWindow = Console_constructWindow();
	g_page_console = nullptr;
	g_consoleDock = nullptr;
	if ( CurrentStyle() == eFloating ) {
		g_page_console = GroupDialog_addPage( "Console", consoleWindow, RawStringExportCaller( "Console" ) );
		g_page_textures = GroupDialog_addPage( "Textures", TextureBrowser_constructWindow( GroupDialog_getWindow() ), TextureBrowserExportTitleCaller() );
	}
	else{
		g_consoleDock = new QDockWidget( "Console", window );
		g_consoleDock->setObjectName( "dock_console" );
		g_consoleDock->setAllowedAreas( Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea );
		g_consoleDock->setWidget( consoleWindow );
		window->addDockWidget( Qt::BottomDockWidgetArea, g_consoleDock );
	}

	g_page_models = GroupDialog_addPage( "Models", ModelBrowser_constructWindow( GroupDialog_getWindow() ), RawStringExportCaller( "Models" ) );

	g_page_layers = GroupDialog_addPage( "Layers", LayersBrowser_constructWindow( GroupDialog_getWindow() ), RawStringExportCaller( "Layers" ) );

	window->show();

	if ( CurrentStyle() == eRegular || CurrentStyle() == eRegularLeft ) {
		window->setCentralWidget( m_hSplit = new QSplitter() );
		{
			m_vSplit = new QSplitter( Qt::Vertical );
			m_vSplit2 = new QSplitter( Qt::Vertical );
			if ( CurrentStyle() == eRegular ){
				m_hSplit->addWidget( m_vSplit );
				m_hSplit->addWidget( m_vSplit2 );
			}
			else{
				m_hSplit->addWidget( m_vSplit2 );
				m_hSplit->addWidget( m_vSplit );
			}

			// xy
			m_pXYWnd = new XYWnd();
			m_pXYWnd->SetViewType( XY );
			m_vSplit->insertWidget( 0, m_pXYWnd->GetWidget() );
			{
				// camera
				m_pCamWnd = NewCamWnd();
				GlobalCamera_setCamWnd( *m_pCamWnd );
				CamWnd_setParent( *m_pCamWnd, window );
				m_vSplit2->addWidget( CamWnd_getWidget( *m_pCamWnd ) );

				// textures
				if( g_Layout_builtInGroupDialog.m_value )
					g_page_textures = GroupDialog_addPage( "Textures", TextureBrowser_constructWindow( GroupDialog_getWindow() ), TextureBrowserExportTitleCaller() );
				else
					m_vSplit2->addWidget( TextureBrowser_constructWindow( window ) );
			}
		}
	}
	else if ( CurrentStyle() == eFloating ) {
		{
			auto *window = new QWidget( m_window, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
			window->setWindowTitle( "Camera" );
			g_guiSettings.addWindow( window, "floating/cam", 400, 300, 50, 100 );

			m_pCamWnd = NewCamWnd();
			GlobalCamera_setCamWnd( *m_pCamWnd );

			{
				auto *box = new QHBoxLayout( window );
				box->setContentsMargins( 1, 1, 1, 1 );
				box->addWidget( CamWnd_getWidget( *m_pCamWnd ) );
			}

			CamWnd_setParent( *m_pCamWnd, window );
			GlobalPressedKeys_connect( window );
			GlobalWindowObservers_connectTopLevel( window );
			CamWnd_Shown_Construct( window );
		}

		{
			auto *window = new QWidget( m_window, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
			g_guiSettings.addWindow( window, "floating/xy", 400, 300, 500, 100 );

			m_pXYWnd = new XYWnd();
			m_pXYWnd->m_parent = window;
			m_pXYWnd->SetViewType( XY );

			{
				auto *box = new QHBoxLayout( window );
				box->setContentsMargins( 1, 1, 1, 1 );
				box->addWidget( m_pXYWnd->GetWidget() );
			}

			GlobalWindowObservers_connectTopLevel( window );
			XY_Top_Shown_Construct( window );
		}

		{
			auto *window = new QWidget( m_window, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
			g_guiSettings.addWindow( window, "floating/xz", 400, 300, 500, 450 );

			m_pXZWnd = new XYWnd();
			m_pXZWnd->m_parent = window;
			m_pXZWnd->SetViewType( XZ );

			{
				auto *box = new QHBoxLayout( window );
				box->setContentsMargins( 1, 1, 1, 1 );
				box->addWidget( m_pXZWnd->GetWidget() );
			}

			GlobalWindowObservers_connectTopLevel( window );
			XZ_Front_Shown_Construct( window );
		}

		{
			auto *window = new QWidget( m_window, Qt::Dialog | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint );
			g_guiSettings.addWindow( window, "floating/yz", 400, 300, 50, 450 );

			m_pYZWnd = new XYWnd();
			m_pYZWnd->m_parent = window;
			m_pYZWnd->SetViewType( YZ );

			{
				auto *box = new QHBoxLayout( window );
				box->setContentsMargins( 1, 1, 1, 1 );
				box->addWidget( m_pYZWnd->GetWidget() );
			}

			GlobalWindowObservers_connectTopLevel( window );
			YZ_Side_Shown_Construct( window );
		}

		GroupDialog_show();
	}
	else // 4 way
	{
		window->setCentralWidget( m_hSplit = new QSplitter() );
		m_hSplit->addWidget( m_vSplit = new QSplitter( Qt::Vertical ) );
		m_hSplit->addWidget( m_vSplit2 = new QSplitter( Qt::Vertical ) );

		m_pCamWnd = NewCamWnd();
		GlobalCamera_setCamWnd( *m_pCamWnd );
		CamWnd_setParent( *m_pCamWnd, window );

		m_vSplit->addWidget( CamWnd_getWidget( *m_pCamWnd ) );

		m_pXZWnd = new XYWnd();
		m_pXZWnd->SetViewType( XZ );

		m_vSplit->addWidget( m_pXZWnd->GetWidget() );

		m_pXYWnd = new XYWnd();
		m_pXYWnd->SetViewType( XY );

		m_vSplit2->addWidget( m_pXYWnd->GetWidget() );

		m_pYZWnd = new XYWnd();
		m_pYZWnd->SetViewType( YZ );

		m_vSplit2->addWidget( m_pYZWnd->GetWidget() );
	}

	if( g_Layout_builtInGroupDialog.m_value && CurrentStyle() != eFloating ){
		m_hSplit->addWidget( GroupDialog_getWindow() );
		m_hSplit->setStretchFactor( 0, 2222 ); // set relative splitter sizes for eSplit (no sizes are restored)
		m_hSplit->setStretchFactor( 1, 2222 );
		m_hSplit->setStretchFactor( 2, 0 );
	}
	else{ // floating group dialog
		GlobalWindowObservers_connectTopLevel( GroupDialog_getWindow() ); // for layers browser icons toggle
	}

	EntityList_constructWindow( window );
	PreferencesDialog_constructWindow( window );
	FindTextureDialog_constructWindow( window );
	SurfaceInspector_constructWindow( window );

	SetActiveXY( m_pXYWnd );

	AddGridChangeCallback( SetGridStatusCaller( *this ) );
	AddGridChangeCallback( FreeCaller<void(), XY_UpdateAllWindows>() );

	Experimental_createDocks( window );
	AudioWorkbench_createDock( window );
	VideoWorkbench_createDock( window );
	Spreadsheet_createDock( window );
	PythonScript_createDock( window );
	ScenegraphInspector_createDock( window );
	AIAssistant_createDock( window );

	s_qe_every_second_timer.enable();

	toolbar_importState( g_toolbarHiddenButtons.c_str() );
	RestoreGuiState();

	TrayIcon_construct();

	//GlobalShortcuts_reportUnregistered();
	GlobalShortcuts_reportDuplicates();
}

void MainFrame::SaveGuiState(){
	//restore good state first
	g_maximizeview.unmaximize();

	g_guiSettings.save();
}

void MainFrame::RestoreGuiState(){
	g_guiSettings.addWindow( m_window, "MainFrame/geometry", 962, 480 );
	g_guiSettings.addMainWindow( m_window, "MainFrame/state" );

	if( !FloatingGroupDialog() && m_hSplit != nullptr && m_vSplit != nullptr && m_vSplit2 != nullptr ){
		g_guiSettings.addSplitter( m_hSplit, "MainFrame/m_hSplit", { 384, 576 } );
		g_guiSettings.addSplitter( m_vSplit, "MainFrame/m_vSplit", CurrentStyle() == eSplit ? QList<int>{ 250, 250 } : QList<int>{ 377, 20 } );
		g_guiSettings.addSplitter( m_vSplit2, "MainFrame/m_vSplit2", CurrentStyle() == eSplit ? QList<int>{ 250, 250 } : QList<int>{ 250, 150 } );
	}
}

void MainFrame::Shutdown(){
	TrayIcon_destroy();

	s_qe_every_second_timer.disable();

	EntityList_destroyWindow();

	delete std::exchange( m_pXYWnd, nullptr );
	delete std::exchange( m_pYZWnd, nullptr );
	delete std::exchange( m_pXZWnd, nullptr );

	ModelBrowser_destroyWindow();
	LayersBrowser_destroyWindow();
	TextureBrowser_destroyWindow();

	DeleteCamWnd( m_pCamWnd );
	m_pCamWnd = 0;

	PreferencesDialog_destroyWindow();
	SurfaceInspector_destroyWindow();
	FindTextureDialog_destroyWindow();

	g_DbgDlg.destroyWindow();

	// destroying group-dialog last because it may contain texture-browser
	GroupDialog_destroyWindow();

	Experimental_destroyDocks();
	AudioWorkbench_stopAndRelease();
	VideoWorkbench_stopAndRelease();
	Spreadsheet_stopAndRelease();
	PythonScript_stopAndRelease();
	ScenegraphInspector_destroyDock();
	AIAssistant_destroy();
	g_consoleDock = nullptr;
	g_page_console = nullptr;

	user_shortcuts_save();
}

void MainFrame::RedrawStatusText(){
	for( int i = 0; i < c_status__count; ++i )
		m_statusLabel[i]->setText( m_status[i].c_str() );
}

void MainFrame::UpdateStatusText(){
	m_idleRedrawStatusText.queueDraw();
}

void MainFrame::SetStatusText( int status_n, const char* status ){
	m_status[status_n] = status;
	UpdateStatusText();
}

void Sys_Status( const char* status ){
	if ( g_pParentWnd )
		g_pParentWnd->SetStatusText( c_status_command, status );
}

void brushCountChanged( const Selectable& selectable ){
	QE_brushCountChanged();
}

//int getRotateIncrement(){
//	return static_cast<int>( g_si_globals.rotate );
//}

int getFarClipDistance(){
	return g_camwindow_globals.m_nCubicScale;
}

float ( *GridStatus_getGridSize )() = GetGridSize;
//int ( *GridStatus_getRotateIncrement )() = getRotateIncrement;
int ( *GridStatus_getFarClipDistance )() = getFarClipDistance;
bool ( *GridStatus_getTextureLockEnabled )();
const char* ( *GridStatus_getTexdefTypeIdLabel )();

void MainFrame::SetGridStatus(){
	StringOutputStream status( 64 );
	const char* lock = ( GridStatus_getTextureLockEnabled() ) ? "ON   " : "OFF  ";
	status << ( GetSnapGridSize() > 0 ? "G:" : "g:" ) << GridStatus_getGridSize()
	       << "  F:" << GridStatus_getTexdefTypeIdLabel()
	       << "  C:" << GridStatus_getFarClipDistance()
	       << "  L:" << lock;
	SetStatusText( c_status_grid, status );
}

void GridStatus_changed(){
	if ( g_pParentWnd != 0 ) {
		g_pParentWnd->SetGridStatus();
	}
}

CopiedString g_OpenGLFont( "Myriad Pro" );
int g_OpenGLFontSize = 8;

void OpenGLFont_select(){
	CopiedString newfont;
	int newsize;
	if( OpenGLFont_dialog( MainFrame_getWindow(), g_OpenGLFont.c_str(), g_OpenGLFontSize, newfont, newsize ) ){
		{
			ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Changing OpenGL Font" );
			delete GlobalOpenGL().m_font;
			g_OpenGLFont = newfont;
			g_OpenGLFontSize = newsize;
			GlobalOpenGL().m_font = glfont_create( g_OpenGLFont.c_str(), g_OpenGLFontSize, g_strAppPath.c_str() );
		}
		UpdateAllWindows();
	}
}


void GlobalGL_sharedContextCreated(){
	// report OpenGL information
	globalOutputStream() << "GL_VENDOR: " << reinterpret_cast<const char*>( gl().glGetString( GL_VENDOR ) ) << '\n';
	globalOutputStream() << "GL_RENDERER: " << reinterpret_cast<const char*>( gl().glGetString( GL_RENDERER ) ) << '\n';
	globalOutputStream() << "GL_VERSION: " << reinterpret_cast<const char*>( gl().glGetString( GL_VERSION ) ) << '\n';
	globalOutputStream() << "GL_EXTENSIONS: " << reinterpret_cast<const char*>( gl().glGetString( GL_EXTENSIONS ) ) << '\n';

	QGL_sharedContextCreated( GlobalOpenGL() );

	ShaderCache_extensionsInitialised();

	GlobalShaderCache().realise();
	Textures_Realise();

	GlobalOpenGL().m_font = glfont_create( g_OpenGLFont.c_str(), g_OpenGLFontSize, g_strAppPath.c_str() );
}

void GlobalGL_sharedContextDestroyed(){
	Textures_Unrealise();
	GlobalShaderCache().unrealise();

	QGL_sharedContextDestroyed( GlobalOpenGL() );
}


void TrayIconEnabled_import( bool value ){
	g_trayIconEnabled = value;
	TrayIcon_setVisible( value );
}

void Layout_constructPreferences( PreferencesPage& page ){
	{
		const char* layouts[] = { "window1.png", "window2.png", "window3.png", "window4.png" };
		page.appendRadioIcons(
		    "Window Layout",
		    StringArrayRange( layouts ),
		    LatchedImportCaller( g_Layout_viewStyle ),
		    IntExportCaller( g_Layout_viewStyle.m_latched )
		);
	}
	page.appendCheckBox(
	    "", "Detachable Menus",
	    LatchedImportCaller( g_Layout_enableDetachableMenus ),
	    BoolExportCaller( g_Layout_enableDetachableMenus.m_latched )
	);
	page.appendCheckBox(
	    "", "Built-In Group Dialog",
	    LatchedImportCaller( g_Layout_builtInGroupDialog ),
	    BoolExportCaller( g_Layout_builtInGroupDialog.m_latched )
	);
	page.appendCheckBox(
	    "", "Experimental Features",
	    LatchedImportCaller( g_Layout_experimentalFeatures ),
	    BoolExportCaller( g_Layout_experimentalFeatures.m_latched )
	);
	page.appendCheckBox( "", "Industry Standard (Maya-style) navigation", g_bMayaNavigation );
	{
		const char* tools[] = { "Drag (Q)", "Translate (W)", "Rotate (E)", "Scale (R)" };
		page.appendRadio( "Default startup tool", StringArrayRange( tools ), IntImportCaller( g_defaultStartupToolPref ), IntExportCaller( g_defaultStartupToolPref ) );
	}
	QCheckBox* trayCheck = page.appendCheckBox( "", "Show system tray / menu bar icon",
		FreeCaller<void(bool), TrayIconEnabled_import>(),
		BoolExportCaller( g_trayIconEnabled ) );
	QCheckBox* minimizeCheck = page.appendCheckBox( "", "Minimize to tray on close (instead of quit)", g_minimizeToTray );
	Widget_connectToggleDependency( minimizeCheck, trayCheck );
}

void Layout_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Layout", "Layout Preferences" ) );
	Layout_constructPreferences( page );
}

void Layout_registerPreferencesPage(){
	PreferencesDialog_addInterfacePage( makeCallbackF( Layout_constructPage ) );
}


void FocusAllViews(){
	XY_Focus();
	GlobalCamera_FocusOnSelected();
}

#include "preferencesystem.h"
#include "stringio.h"

void MainFrame_Construct(){
	MainFrame_registerCommands();

	GlobalPreferenceSystem().registerPreference( "DetachableMenus", makeBoolStringImportCallback( LatchedAssignCaller( g_Layout_enableDetachableMenus ) ), BoolExportStringCaller( g_Layout_enableDetachableMenus.m_latched ) );
	GlobalPreferenceSystem().registerPreference( "QE4StyleWindows", makeIntStringImportCallback( LatchedAssignCaller( g_Layout_viewStyle ) ), IntExportStringCaller( g_Layout_viewStyle.m_latched ) );
	GlobalPreferenceSystem().registerPreference( "BuiltInGroupDialog", makeBoolStringImportCallback( LatchedAssignCaller( g_Layout_builtInGroupDialog ) ), BoolExportStringCaller( g_Layout_builtInGroupDialog.m_latched ) );
	GlobalPreferenceSystem().registerPreference( "ExperimentalFeatures", makeBoolStringImportCallback( LatchedAssignCaller( g_Layout_experimentalFeatures ) ), BoolExportStringCaller( g_Layout_experimentalFeatures.m_latched ) );
	GlobalPreferenceSystem().registerPreference( "ExpiramentalFeatures", makeBoolStringImportCallback( LatchedAssignCaller( g_Layout_experimentalFeatures ) ), BoolExportStringCaller( g_Layout_experimentalFeatures.m_latched ) );
	GlobalPreferenceSystem().registerPreference( "MayaNavigation", BoolImportStringCaller( g_bMayaNavigation ), BoolExportStringCaller( g_bMayaNavigation ) );
	GlobalPreferenceSystem().registerPreference( "TrayIconEnabled", BoolImportStringCaller( g_trayIconEnabled ), BoolExportStringCaller( g_trayIconEnabled ) );
	GlobalPreferenceSystem().registerPreference( "MinimizeToTray", BoolImportStringCaller( g_minimizeToTray ), BoolExportStringCaller( g_minimizeToTray ) );
	GlobalPreferenceSystem().registerPreference( "ToolbarHiddenButtons", CopiedStringImportStringCaller( g_toolbarHiddenButtons ), CopiedStringExportStringCaller( g_toolbarHiddenButtons ) );
	GlobalPreferenceSystem().registerPreference( "OpenGLFont", CopiedStringImportStringCaller( g_OpenGLFont ), CopiedStringExportStringCaller( g_OpenGLFont ) );
	GlobalPreferenceSystem().registerPreference( "OpenGLFontSize", IntImportStringCaller( g_OpenGLFontSize ), IntExportStringCaller( g_OpenGLFontSize ) );

	for( size_t i = 0; i < g_strExtraResourcePaths.size(); ++i )
		GlobalPreferenceSystem().registerPreference( StringStream<32>( "ExtraResourcePath", i ),
			CopiedStringImportStringCaller( g_strExtraResourcePaths[i] ), CopiedStringExportStringCaller( g_strExtraResourcePaths[i] ) );

	GlobalPreferenceSystem().registerPreference( "EnginePath", CopiedStringImportStringCaller( g_strEnginePath ), CopiedStringExportStringCaller( g_strEnginePath ) );
	GlobalPreferenceSystem().registerPreference( "InstalledDevFilesPath", CopiedStringImportStringCaller( g_installedDevFilesPath ), CopiedStringExportStringCaller( g_installedDevFilesPath ) );
	if ( g_strEnginePath.empty() )
	{
		g_strEnginePath_was_empty_1st_start = true;
		const char* ENGINEPATH_ATTRIBUTE =
#if defined( WIN32 )
		    "enginepath_win32"
#elif defined( __APPLE__ )
		    "enginepath_macos"
#elif defined( __linux__ ) || defined ( __FreeBSD__ )
		    "enginepath_linux"
#else
#error "unknown platform"
#endif
		    ;
		g_strEnginePath = StringStream( DirectoryCleaned( g_pGameDescription->getRequiredKeyValue( ENGINEPATH_ATTRIBUTE ) ) );
	}


	Layout_registerPreferencesPage();
	Paths_registerPreferencesPage();

	g_brushCount.setCountChangedCallback( makeCallbackF( QE_brushCountChanged ) );
	g_patchCount.setCountChangedCallback( makeCallbackF( QE_brushCountChanged ) );
	g_entityCount.setCountChangedCallback( makeCallbackF( QE_brushCountChanged ) );
	GlobalEntityCreator().setCounter( &g_entityCount );
	GlobalSelectionSystem().addSelectionChangeCallback( FreeCaller<void(const Selectable&), brushCountChanged>() );
	GlobalSelectionSystem().addSelectionChangeCallback( FreeCaller<void(const Selectable&), Experimental_selectionChanged>() );
	AddSceneChangeCallback( FreeCaller<void(), Experimental_liveSyncSceneChanged>() );
	AddCameraMovedCallback( FreeCaller<void(), Experimental_liveSyncCameraChanged>() );

	GLWidget_sharedContextCreated = GlobalGL_sharedContextCreated;
	GLWidget_sharedContextDestroyed = GlobalGL_sharedContextDestroyed;

	GlobalEntityClassManager().attach( g_WorldspawnColourEntityClassObserver );
}

void MainFrame_Destroy(){
	GlobalEntityClassManager().detach( g_WorldspawnColourEntityClassObserver );

	GlobalEntityCreator().setCounter( 0 );
	g_entityCount.setCountChangedCallback( Callback<void()>() );
	g_patchCount.setCountChangedCallback( Callback<void()>() );
	g_brushCount.setCountChangedCallback( Callback<void()>() );
}
