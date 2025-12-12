#include "renderer_import.h"

#ifdef RADIANT_USE_ENGINE_RENDERER_VK

#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QDir>
#include <dlfcn.h>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  if __has_include(<QtGui/qnativeinterface.h>)
#    include <QtGui/qnativeinterface.h>
#    define RADIANT_HAVE_QNATIVE_INTERFACE 1
#  else
#    define RADIANT_HAVE_QNATIVE_INTERFACE 0
#  endif
#else
#  define RADIANT_HAVE_QNATIVE_INTERFACE 0
#endif

#if defined(Q_OS_UNIX)
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
#endif

// Basic logging sink to a Qt-side log file.
static void Qt_R_Printf( printParm_t, const char *fmt, ... ){
	char buffer[2048];
	va_list argptr;
	va_start( argptr, fmt );
	vsnprintf( buffer, sizeof(buffer), fmt, argptr );
	va_end( argptr );

	QFile f(QStringLiteral("/tmp/radiant_qt_renderer.log"));
	if ( f.open(QIODevice::Append | QIODevice::Text) ) {
		QTextStream ts(&f);
		ts << buffer;
	}
}

// Minimal stub implementations
static void Qt_Sys_Error( errorParm_t, const char *fmt, ... ) {
	char buffer[2048];
	va_list argptr;
	va_start( argptr, fmt );
	vsnprintf( buffer, sizeof(buffer), fmt, argptr );
	va_end( argptr );
	fprintf(stderr, "Renderer error: %s\n", buffer);
}

static void* Qt_Malloc( int bytes ){
	return calloc(1, static_cast<size_t>(bytes));
}

static void Qt_Free( void *ptr ){
	free(ptr);
}

static void Qt_FreeAll( void ){
}

static void Qt_GLimp_InitGamma( glconfig_t* ) {}

static void Qt_CL_SetScaling( float, int, int ) {}

static void* g_vulkanLib = nullptr;
static PFN_vkGetInstanceProcAddr g_vkGetInstanceProcAddr = nullptr;

static void Qt_VKimp_Shutdown( qboolean ){
	if ( g_vulkanLib ) {
		dlclose( g_vulkanLib );
		g_vulkanLib = nullptr;
		g_vkGetInstanceProcAddr = nullptr;
	}
}

static void Qt_VKimp_Init( glconfig_t *config ){
	if ( !g_vkGetInstanceProcAddr ) {
		g_vulkanLib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
		if ( !g_vulkanLib ) {
			fprintf(stderr, "Failed to load libvulkan.so.1: %s\n", dlerror());
			return;
		}
		g_vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>( dlsym(g_vulkanLib, "vkGetInstanceProcAddr") );
		if ( !g_vkGetInstanceProcAddr ) {
			fprintf(stderr, "Failed to resolve vkGetInstanceProcAddr\n");
			return;
		}
	}

	if ( config ) {
		config->vidWidth = 1280;
		config->vidHeight = 720;
		config->colorBits = 24;
		config->depthBits = 24;
		config->stencilBits = 8;
		config->displayFrequency = 60;
		config->driverType = GLDRV_ICD;
		config->hardwareType = GLHW_GENERIC;
		config->deviceSupportsGamma = qfalse;
		config->isFullscreen = qfalse;
		config->stereoEnabled = qfalse;
		config->smpActive = qfalse;
		config->textureEnvAddAvailable = qtrue;
		memset(config->renderer_string, 0, sizeof(config->renderer_string));
		memset(config->vendor_string, 0, sizeof(config->vendor_string));
		memset(config->version_string, 0, sizeof(config->version_string));
		memset(config->extensions_string, 0, sizeof(config->extensions_string));
	}
}

static PFN_vkVoidFunction Qt_VK_GetInstanceProcAddr( VkInstance instance, const char *name ){
	if ( !g_vkGetInstanceProcAddr ) return nullptr;
	return g_vkGetInstanceProcAddr( instance, name );
}

static void* Qt_Hunk_Alloc( int size, ha_pref ){
	return Qt_Malloc(size);
}

static void* Qt_Hunk_AllocateTempMemory( int size ){
	return Qt_Malloc(size);
}

static void Qt_Hunk_FreeTempMemory( void *block ){
	Qt_Free(block);
}

static int Qt_Milliseconds(){
	static QElapsedTimer timer;
	if ( !timer.isValid() ) {
		timer.start();
		return 0;
	}
	return static_cast<int>( timer.elapsed() );
}

static int64_t Qt_Microseconds(){
	static QElapsedTimer timer;
	if ( !timer.isValid() ) {
		timer.start();
		return 0;
	}
	return timer.nsecsElapsed() / 1000;
}

static void Qt_FS_FreeFile( void *buf ) {
	if ( buf ) free(buf);
}

static qboolean Qt_FS_FileExists( const char * ) {
	return qfalse;
}

static int Qt_FS_ReadFile( const char *name, void **buf ){
	if ( !name || !buf ) {
		return -1;
	}
	QFile f(QString::fromUtf8(name));
	if ( !f.exists() ) {
		*buf = nullptr;
		return -1;
	}
	if ( !f.open(QIODevice::ReadOnly) ) {
		*buf = nullptr;
		return -1;
	}
	const QByteArray data = f.readAll();
	f.close();
	if ( data.isEmpty() ) {
		*buf = nullptr;
		return 0;
	}
	void* out = malloc(static_cast<size_t>(data.size()));
	if ( !out ) {
		*buf = nullptr;
		return -1;
	}
	memcpy(out, data.constData(), static_cast<size_t>(data.size()));
	*buf = out;
	return data.size();
}

static char** Qt_FS_ListFiles( const char *name, const char *extension, int *numfilesfound ){
	if ( !numfilesfound ) {
		return nullptr;
	}
	QDir dir(QString::fromUtf8(name ? name : ""));
	QStringList filters;
	if ( extension && extension[0] ) {
		filters << QStringLiteral("*.") + QString::fromUtf8(extension);
	}
	QStringList entries = dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);
	*numfilesfound = entries.size();
	if ( entries.isEmpty() ) {
		return nullptr;
	}
	char** list = (char**)malloc(sizeof(char*) * entries.size());
	if ( !list ) {
		*numfilesfound = 0;
		return nullptr;
	}
	for (int i = 0; i < entries.size(); ++i) {
		QByteArray ba = entries[i].toUtf8();
		list[i] = (char*)malloc(static_cast<size_t>(ba.size()) + 1);
		if ( list[i] ) {
			memcpy(list[i], ba.constData(), static_cast<size_t>(ba.size()) + 1);
		}
	}
	return list;
}

static void Qt_FS_FreeFileList( char **filelist ){
	if ( !filelist ) return;
	// Caller should track count; left as no-op to avoid free without count.
}

// ----------------------
// Cvar / Cmd stubs (minimal; enough to let renderer init)
// ----------------------
static cvar_t g_dummyCvar;
static cvar_t* Qt_Cvar_Get( const char *name, const char *value, int flags ){
	memset(&g_dummyCvar, 0, sizeof(g_dummyCvar));
	g_dummyCvar.name = const_cast<char*>(name ? name : "");
	g_dummyCvar.string = const_cast<char*>(value ? value : "");
	g_dummyCvar.flags = flags;
	return &g_dummyCvar;
}
static void Qt_Cvar_Set( const char *, const char * ) {}
static void Qt_Cvar_SetValue( const char *, float ) {}
static void Qt_Cvar_CheckRange( cvar_t *, const char *, const char *, cvarValidator_t ) {}
static void Qt_Cvar_SetDescription( cvar_t *, const char * ) {}
static void Qt_Cvar_SetGroup( cvar_t *, cvarGroup_t ) {}
static int  Qt_Cvar_CheckGroup( cvarGroup_t ){ return 0; }
static void Qt_Cvar_ResetGroup( cvarGroup_t, qboolean ) {}
static void Qt_Cvar_VariableStringBuffer( const char *, char *buffer, int bufsize ){
	if ( buffer && bufsize > 0 ) buffer[0] = '\0';
}
static const char *Qt_Cvar_VariableString( const char * ){ return ""; }
static int  Qt_Cvar_VariableIntegerValue( const char * ){ return 0; }

static void Qt_Cmd_AddCommand( const char *, void(*)(void) ) {}
static void Qt_Cmd_RemoveCommand( const char * ) {}
static int  Qt_Cmd_Argc( void ){ return 0; }
static const char *Qt_Cmd_Argv( int ){ return ""; }
static void Qt_Cmd_ExecuteText( cbufExec_t, const char * ) {}

// ----------------------
// Vulkan surface support (X11 only for now)
// ----------------------

#if defined(Q_OS_UNIX)
static quintptr g_winId = 0;

void SetVkSurfaceWindow(quintptr winId){
	g_winId = winId;
}

static qboolean Qt_VK_CreateSurface( VkInstance instance, VkSurfaceKHR *pSurface ){
	if ( !g_winId ) {
		return qfalse;
	}

	// XCB path
	xcb_connection_t* conn = nullptr;
	xcb_window_t xcbWin = static_cast<xcb_window_t>(g_winId);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) && RADIANT_HAVE_QNATIVE_INTERFACE
	if ( auto x11 = QNativeInterface::QX11Application::instance() ) {
		conn = reinterpret_cast<xcb_connection_t*>( x11->connection() );
	}
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	auto *native = QGuiApplication::platformNativeInterface();
	if ( native ) {
		conn = reinterpret_cast<xcb_connection_t*>( native->nativeResourceForIntegration("connection") );
	}
#else
	Q_UNUSED(instance);
	Q_UNUSED(pSurface);
	return qfalse;
#endif

	if ( !conn ) {
		return qfalse;
	}

	VkXcbSurfaceCreateInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	info.pNext = nullptr;
	info.flags = 0;
	info.connection = conn;
	info.window = xcbWin;

	auto fpCreateXcbSurfaceKHR = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(
		vkGetInstanceProcAddr( instance, "vkCreateXcbSurfaceKHR" ) );

	if ( !fpCreateXcbSurfaceKHR ) {
		return qfalse;
	}

	VkResult res = fpCreateXcbSurfaceKHR( instance, &info, nullptr, pSurface );
	return res == VK_SUCCESS ? qtrue : qfalse;
}
#else
void SetVkSurfaceWindow(quintptr){ }
static qboolean Qt_VK_CreateSurface( VkInstance, VkSurfaceKHR * ){ return qfalse; }
#endif

refimport_t MakeQtRefImport(const QString& logPath){
	Q_UNUSED(logPath);

	refimport_t imp {};

	imp.Printf = Qt_R_Printf;
	imp.Error = Qt_Sys_Error;

	imp.Milliseconds = Qt_Milliseconds;
	imp.Microseconds = Qt_Microseconds;

	// Memory hooks (simple heap-backed versions)
	imp.Hunk_Alloc = Qt_Hunk_Alloc;
	imp.Hunk_AllocateTempMemory = Qt_Hunk_AllocateTempMemory;
	imp.Hunk_FreeTempMemory = Qt_Hunk_FreeTempMemory;
	imp.Malloc = Qt_Malloc;
	imp.Free = Qt_Free;
	imp.FreeAll = Qt_FreeAll;
	imp.GLimp_InitGamma = Qt_GLimp_InitGamma;
	imp.CL_SetScaling = Qt_CL_SetScaling;

	// File helpers (still basic; real VFS integration TBD)
	imp.FS_ReadFile = Qt_FS_ReadFile;
	imp.FS_FreeFile = Qt_FS_FreeFile;
	imp.FS_FileExists = Qt_FS_FileExists;
	imp.FS_ListFiles = Qt_FS_ListFiles;
	imp.FS_FreeFileList = Qt_FS_FreeFileList;
	imp.FS_WriteFile = nullptr;

	// Minimal cvar/cmd plumbing (placeholder)
	imp.Cvar_Get = Qt_Cvar_Get;
	imp.Cvar_Set = Qt_Cvar_Set;
	imp.Cvar_SetValue = Qt_Cvar_SetValue;
	imp.Cvar_CheckRange = Qt_Cvar_CheckRange;
	imp.Cvar_SetDescription = Qt_Cvar_SetDescription;
	imp.Cvar_SetGroup = Qt_Cvar_SetGroup;
	imp.Cvar_CheckGroup = Qt_Cvar_CheckGroup;
	imp.Cvar_ResetGroup = Qt_Cvar_ResetGroup;
	imp.Cvar_VariableStringBuffer = Qt_Cvar_VariableStringBuffer;
	imp.Cvar_VariableString = Qt_Cvar_VariableString;
	imp.Cvar_VariableIntegerValue = Qt_Cvar_VariableIntegerValue;

	imp.Cmd_AddCommand = Qt_Cmd_AddCommand;
	imp.Cmd_RemoveCommand = Qt_Cmd_RemoveCommand;
	imp.Cmd_Argc = Qt_Cmd_Argc;
	imp.Cmd_Argv = Qt_Cmd_Argv;
	imp.Cmd_ExecuteText = Qt_Cmd_ExecuteText;

	// Vulkan surface hooks (only CreateSurface for now)
	imp.VKimp_Init = Qt_VKimp_Init;
	imp.VKimp_Shutdown = Qt_VKimp_Shutdown;
	imp.VK_GetInstanceProcAddr = Qt_VK_GetInstanceProcAddr;
	imp.VK_CreateSurface = Qt_VK_CreateSurface;

	return imp;
}

#endif
