#include "renderer_import.h"

#ifdef RADIANT_USE_ENGINE_RENDERER_VK

#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QGuiApplication>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <qpa/qplatformnativeinterface.h>
#include <QNativeInterface>
#endif

#if defined(Q_OS_UNIX)
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
#endif

// Basic logging sink to a Qt-side log file.
static void Qt_R_Printf( const char *fmt, ... ){
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
static void *Qt_Z_Malloc( int size, memtag_t, qboolean, int ) {
	return calloc(1, size);
}

static void Qt_Z_Free( void *ptr ) {
	free(ptr);
}

static void Qt_Sys_Error( const char *fmt, ... ) {
	char buffer[2048];
	va_list argptr;
	va_start( argptr, fmt );
	vsnprintf( buffer, sizeof(buffer), fmt, argptr );
	va_end( argptr );
	fprintf(stderr, "Renderer error: %s\n", buffer);
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
	QDir dir(QString::fromUtf8(name));
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

	auto *native = QGuiApplication::platformNativeInterface();
	if ( !native ) {
		return qfalse;
	}

	// XCB path
	xcb_connection_t* conn = nullptr;
	xcb_window_t xcbWin = static_cast<xcb_window_t>(g_winId);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	if ( auto x11 = QNativeInterface::QX11Application::instance() ) {
		conn = reinterpret_cast<xcb_connection_t*>( x11->connection() );
	}
#endif

	if ( !conn ) {
		return qfalse;
	}

	VkXcbSurfaceCreateInfoKHR info{ VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR };
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
	refimport_t ri {};

	ri.Printf = Qt_R_Printf;
	ri.Error = Qt_Sys_Error;

	ri.Milliseconds = Qt_Milliseconds;
	ri.Microseconds = Qt_Microseconds;

	ri.Z_Malloc = Qt_Z_Malloc;
	ri.Free = Qt_Z_Free;

	// File helpers (still basic; real VFS integration TBD)
	ri.FS_ReadFile = Qt_FS_ReadFile;
	ri.FS_FreeFile = Qt_FS_FreeFile;
	ri.FS_FileExists = Qt_FS_FileExists;
	ri.FS_ListFiles = Qt_FS_ListFiles;
	ri.FS_FreeFileList = Qt_FS_FreeFileList;
	ri.FS_WriteFile = nullptr;

	// Vulkan surface hooks (only CreateSurface for now)
	ri.VKimp_Init = nullptr;
	ri.VKimp_Shutdown = nullptr;
	ri.VK_GetInstanceProcAddr = nullptr;
	ri.VK_CreateSurface = Qt_VK_CreateSurface;

	return ri;
}

#endif
