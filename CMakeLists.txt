cmake_minimum_required(VERSION 3.20)

project(radiant-suite LANGUAGES C CXX)

option(RADIANT_BUILD_EDITOR "Build the GtkRadiant editor" ON)
option(RADIANT_BUILD_Q3MAP2 "Build the q3map2 BSP compiler" ON)
option(RADIANT_BUILD_Q3DATA "Build the q3data asset tool" ON)
option(RADIANT_BUILD_BSPC "Build the bspc tool (id Tech 2 bsp compiler)" OFF)
option(RADIANT_BUILD_PLUGINS "Build core editor plugins" ON)
option(RADIANT_BUILD_CONTRIB "Build contrib plugins (bobtoolz, gensurf, etc.)" ON)
option(RADIANT_BUILD_QT "Build Qt-based Radiant" OFF)
option(RADIANT_USE_ENGINE_RENDERER_VK "Link and expose the engine Vulkan renderer inside Qt Radiant (WYSIWYG viewport groundwork)" OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(RADIANT_OUTPUT_ROOT "${CMAKE_BINARY_DIR}/install")
set(RADIANT_MODULE_OUTPUT "${RADIANT_OUTPUT_ROOT}/modules")
foreach(kind RUNTIME LIBRARY ARCHIVE)
	set(CMAKE_${kind}_OUTPUT_DIRECTORY "${RADIANT_OUTPUT_ROOT}")
endforeach()

# Local fallback libdir for vendored GtkGLExt/GLU/Xmu (when not installed system-wide)
set(GTK_LOCAL_LIBDIR "${CMAKE_SOURCE_DIR}/build-deps/gtkglext-root/usr/lib/x86_64-linux-gnu")
if(EXISTS "${GTK_LOCAL_LIBDIR}")
	link_directories(${GTK_LOCAL_LIBDIR})
endif()

find_package(PkgConfig)

set(GTK_INCLUDE_DIRS "" CACHE STRING "GTK include dirs (when pkg-config is unavailable)")
set(GTK_LIBRARIES "" CACHE STRING "GTK libs (when pkg-config is unavailable)")
set(GTKGLEXT_LIBRARIES "" CACHE STRING "GtkGLExt libs (when pkg-config is unavailable)")
set(GLIB_LIBRARIES "" CACHE STRING "GLib libs (when pkg-config is unavailable)")
set(GDKPIXBUF_LIBRARIES "" CACHE STRING "GdkPixbuf libs (when pkg-config is unavailable)")

if(PKG_CONFIG_FOUND)
	pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
	pkg_check_modules(GTKGLEXT REQUIRED gtkglext-1.0)
	pkg_check_modules(GLIB REQUIRED glib-2.0)
	pkg_check_modules(GDKPIXBUF REQUIRED gdk-pixbuf-2.0)

	list(APPEND GTK_INCLUDE_DIRS
		${GTK2_INCLUDE_DIRS}
		${GTKGLEXT_INCLUDE_DIRS}
		${GLIB_INCLUDE_DIRS}
		${GDKPIXBUF_INCLUDE_DIRS}
	)

	list(APPEND GTK_LIBRARIES
		${GTK2_LIBRARIES}
		${GLIB_LIBRARIES}
		${GDKPIXBUF_LIBRARIES}
		${GTKGLEXT_LIBRARIES}
	)
else()
	message(STATUS "pkg-config not found; set GTK_INCLUDE_DIRS/GTK_LIBRARIES manually (needed for the editor and GUI-dependent tools)")
endif()

find_package(OpenGL REQUIRED)
find_package(X11 REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(LibXml2 REQUIRED)

# Optional Qt UI (Qt6 preferred, Qt5 fallback)
if(RADIANT_BUILD_QT)
	find_package(Qt6 COMPONENTS Widgets QUIET)
	if(NOT Qt6_FOUND)
		find_package(Qt5 COMPONENTS Widgets QUIET)
	endif()
	if(NOT (Qt6_FOUND OR Qt5_FOUND))
		message(FATAL_ERROR "RADIANT_BUILD_QT=ON but neither Qt6 nor Qt5 Widgets found")
	endif()
endif()

set(RADIANT_GL_LIBS OpenGL::GL)
if(TARGET OpenGL::GLU)
	list(APPEND RADIANT_GL_LIBS OpenGL::GLU)
endif()

set(RADIANT_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

set(RADIANT_INCLUDE_DIRS
	${RADIANT_SOURCE_ROOT}/include
	${RADIANT_SOURCE_ROOT}/libs
	${RADIANT_SOURCE_ROOT}/libs/picomodel
	${RADIANT_SOURCE_ROOT}/libs/splines
	${RADIANT_SOURCE_ROOT}/libs/md5lib
	${RADIANT_SOURCE_ROOT}/tools/quake3/common
)

set(RADIANT_COMMON_DEFINES
	_CRT_SECURE_NO_WARNINGS
	_CRT_SECURE_NO_DEPRECATE
)

function(radiant_set_module_output target_name)
	set_target_properties(${target_name} PROPERTIES
		PREFIX ""
		LIBRARY_OUTPUT_DIRECTORY "${RADIANT_MODULE_OUTPUT}"
		RUNTIME_OUTPUT_DIRECTORY "${RADIANT_MODULE_OUTPUT}"
		ARCHIVE_OUTPUT_DIRECTORY "${RADIANT_MODULE_OUTPUT}"
	)
endfunction()

add_library(cmdlib STATIC libs/cmdlib/cmdlib.cpp)
target_include_directories(cmdlib PUBLIC ${RADIANT_INCLUDE_DIRS})
target_compile_definitions(cmdlib PUBLIC ${RADIANT_COMMON_DEFINES})

add_library(ddslib STATIC libs/ddslib/ddslib.c)
target_include_directories(ddslib PUBLIC ${RADIANT_INCLUDE_DIRS})
target_compile_definitions(ddslib PUBLIC ${RADIANT_COMMON_DEFINES})

set(L_NET_SOURCES libs/l_net/l_net.c)
if(WIN32)
	list(APPEND L_NET_SOURCES libs/l_net/l_net_wins.c)
else()
	list(APPEND L_NET_SOURCES libs/l_net/l_net_berkeley.c)
endif()
add_library(l_net STATIC ${L_NET_SOURCES})
target_include_directories(l_net PUBLIC ${RADIANT_INCLUDE_DIRS} ${RADIANT_SOURCE_ROOT}/libs/l_net)
target_compile_definitions(l_net PUBLIC ${RADIANT_COMMON_DEFINES})

add_library(mathlib STATIC
	libs/mathlib/bbox.c
	libs/mathlib/m4x4.c
	libs/mathlib/mathlib.c
	libs/mathlib/ray.c
)
target_include_directories(mathlib PUBLIC ${RADIANT_INCLUDE_DIRS})
target_compile_definitions(mathlib PUBLIC ${RADIANT_COMMON_DEFINES})

add_library(md5lib STATIC
	libs/md5lib/md4.c
	libs/md5lib/md5lib.c
)
target_include_directories(md5lib PUBLIC ${RADIANT_INCLUDE_DIRS})
target_compile_definitions(md5lib PUBLIC ${RADIANT_COMMON_DEFINES})

file(GLOB PICOMODEL_SOURCES CONFIGURE_DEPENDS
	libs/picomodel/*.c
	libs/picomodel/lwo/*.c
)
add_library(picomodel STATIC ${PICOMODEL_SOURCES})
target_include_directories(picomodel PUBLIC ${RADIANT_INCLUDE_DIRS})
target_compile_definitions(picomodel PUBLIC ${RADIANT_COMMON_DEFINES})
target_link_libraries(picomodel PUBLIC ZLIB::ZLIB)

file(GLOB SPLINES_SOURCES CONFIGURE_DEPENDS
	libs/splines/*.cpp
)
add_library(splines STATIC ${SPLINES_SOURCES})
target_include_directories(splines PUBLIC ${RADIANT_INCLUDE_DIRS})
target_compile_definitions(splines PUBLIC ${RADIANT_COMMON_DEFINES})

add_library(synapse STATIC libs/synapse/synapse.cpp)
target_include_directories(synapse PUBLIC ${RADIANT_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS} ${LIBXML2_INCLUDE_DIR})
target_compile_definitions(synapse PUBLIC ${RADIANT_COMMON_DEFINES})

add_library(quake3-common STATIC
	tools/quake3/common/cmdlib.c
	tools/quake3/common/imagelib.c
	tools/quake3/common/inout.c
	tools/quake3/common/jpeg.c
	tools/quake3/common/mutex.c
	tools/quake3/common/polylib.c
	tools/quake3/common/scriplib.c
	tools/quake3/common/threads.c
	tools/quake3/common/unzip.c
	tools/quake3/common/vfs.c
)
target_include_directories(quake3-common PUBLIC ${RADIANT_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS} ${LIBXML2_INCLUDE_DIR})
target_compile_definitions(quake3-common PUBLIC ${RADIANT_COMMON_DEFINES})
target_link_libraries(quake3-common PUBLIC ZLIB::ZLIB PNG::PNG JPEG::JPEG)

add_library(radiant_core INTERFACE)
target_include_directories(radiant_core INTERFACE ${RADIANT_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS} ${LIBXML2_INCLUDE_DIR})
target_compile_definitions(radiant_core INTERFACE ${RADIANT_COMMON_DEFINES})
target_link_libraries(radiant_core INTERFACE
	cmdlib ddslib l_net mathlib md5lib picomodel splines synapse quake3-common
	ZLIB::ZLIB PNG::PNG JPEG::JPEG LibXml2::LibXml2 Threads::Threads X11::X11
)

if(RADIANT_BUILD_Q3MAP2)
	file(GLOB Q3MAP2_SOURCES CONFIGURE_DEPENDS tools/quake3/q3map2/*.c)
	add_executable(q3map2 ${Q3MAP2_SOURCES})
	target_include_directories(q3map2 PRIVATE ${RADIANT_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS})
	target_link_libraries(q3map2 PRIVATE radiant_core ${GTK_LIBRARIES} ${RADIANT_GL_LIBS})
	if(WIN32 AND EXISTS "${RADIANT_SOURCE_ROOT}/tools/quake3/q3map2/q3map2.rc")
		target_sources(q3map2 PRIVATE tools/quake3/q3map2/q3map2.rc)
	endif()
	if(NOT WIN32)
		target_link_libraries(q3map2 PRIVATE m)
	endif()
endif()

if(RADIANT_BUILD_Q3DATA)
	file(GLOB Q3DATA_SOURCES CONFIGURE_DEPENDS tools/quake3/q3data/*.c)
	add_executable(q3data ${Q3DATA_SOURCES})
	target_include_directories(q3data PRIVATE ${RADIANT_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS})
	target_link_libraries(q3data PRIVATE radiant_core ${GTK_LIBRARIES})
	if(NOT WIN32)
		target_link_libraries(q3data PRIVATE m)
	endif()
endif()

if(RADIANT_BUILD_BSPC)
	if(EXISTS "${RADIANT_SOURCE_ROOT}/bspc")
		file(GLOB BSPC_SOURCES CONFIGURE_DEPENDS bspc/*.c)
		add_executable(bspc ${BSPC_SOURCES})
		target_include_directories(bspc PRIVATE ${RADIANT_INCLUDE_DIRS})
		target_link_libraries(bspc PRIVATE radiant_core)
	else()
		message(WARNING "RADIANT_BUILD_BSPC is ON but ${RADIANT_SOURCE_ROOT}/bspc was not found; skipping bspc target")
	endif()
endif()

if(RADIANT_BUILD_EDITOR)
	file(GLOB RADIANT_SOURCES CONFIGURE_DEPENDS
		radiant/*.cpp
		radiant/*.c
	)
	if(WIN32)
		list(APPEND RADIANT_SOURCES radiant/radiant.rc)
	endif()
	add_executable(radiant ${RADIANT_SOURCES})
	target_include_directories(radiant PRIVATE ${RADIANT_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS})
	target_link_libraries(radiant PRIVATE radiant_core ${GTK_LIBRARIES} ${RADIANT_GL_LIBS})
endif()

if(RADIANT_BUILD_QT)
	set(QT_TARGET radiant_qt)
	set(QT_SOURCES
		qt/main_qt.cpp
		qt/qt_env.cpp
		qt/vk_viewport.cpp
		qt/main_window.cpp
		qt/renderer_bridge.cpp
		qt/renderer_import.cpp
	)

	add_executable(${QT_TARGET} ${QT_SOURCES})
	set_target_properties(${QT_TARGET} PROPERTIES AUTOMOC ON)

	if(Qt6_FOUND)
		target_link_libraries(${QT_TARGET} PRIVATE Qt6::Widgets)
		target_compile_definitions(${QT_TARGET} PRIVATE QT_NO_KEYWORDS)
	elseif(Qt5_FOUND)
		target_link_libraries(${QT_TARGET} PRIVATE Qt5::Widgets)
		target_compile_definitions(${QT_TARGET} PRIVATE QT_NO_KEYWORDS)
	endif()

	if(RADIANT_USE_ENGINE_RENDERER_VK)
		# Engine root defaults to the parent of the Radiant submodule
		set(ENGINE_ROOT "${CMAKE_SOURCE_DIR}/.." CACHE PATH "Path to the engine source root (idtech3)")

		find_library(ENGINE_RENDERER_VK_LIB
			NAMES idtech3_renderer_vk idtech3_vulkan_x86_64 idtech3_vulkan
			HINTS
				"${ENGINE_ROOT}/build"
				"${ENGINE_ROOT}/build/Release"
				"${ENGINE_ROOT}/build/Debug"
				"${ENGINE_ROOT}/build/bin"
		)

		find_path(ENGINE_RENDERER_INCLUDE
			NAMES tr_public.h
			HINTS
				"${ENGINE_ROOT}/src/renderercommon"
				"${ENGINE_ROOT}/src"
		)

		if(NOT ENGINE_RENDERER_VK_LIB OR NOT ENGINE_RENDERER_INCLUDE)
			message(FATAL_ERROR "RADIANT_USE_ENGINE_RENDERER_VK=ON but engine renderer Vulkan library or headers were not found. Set ENGINE_ROOT or ENGINE_RENDERER_VK_LIB/ENGINE_RENDERER_INCLUDE.")
		endif()

		find_package(Vulkan REQUIRED)

		target_include_directories(${QT_TARGET} PRIVATE
			${ENGINE_RENDERER_INCLUDE}
			${ENGINE_ROOT}/src
			${ENGINE_ROOT}/src/renderercommon
		)

		target_link_libraries(${QT_TARGET} PRIVATE
			${ENGINE_RENDERER_VK_LIB}
			Vulkan::Vulkan
		)

		target_compile_definitions(${QT_TARGET} PRIVATE RADIANT_USE_ENGINE_RENDERER_VK)
	endif()

	# Output alongside other install binaries
	set_target_properties(${QT_TARGET} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${RADIANT_OUTPUT_ROOT}"
	)
endif()

function(radiant_add_plugin plugin_name plugin_dir)
	file(GLOB PLUGIN_SRCS CONFIGURE_DEPENDS
		${plugin_dir}/*.c
		${plugin_dir}/*.cpp
	)
	if(NOT PLUGIN_SRCS)
		message(WARNING "No sources found for plugin ${plugin_name} in ${plugin_dir}")
		return()
	endif()
	add_library(${plugin_name} MODULE ${PLUGIN_SRCS})
	target_include_directories(${plugin_name} PRIVATE ${RADIANT_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS})
	target_link_libraries(${plugin_name} PRIVATE radiant_core ${GTK_LIBRARIES} ${RADIANT_GL_LIBS})
	radiant_set_module_output(${plugin_name})
endfunction()

if(RADIANT_BUILD_PLUGINS)
	radiant_add_plugin(entity       "${RADIANT_SOURCE_ROOT}/plugins/entity")
	radiant_add_plugin(image        "${RADIANT_SOURCE_ROOT}/plugins/image")
	radiant_add_plugin(imagepng     "${RADIANT_SOURCE_ROOT}/plugins/imagepng")
	radiant_add_plugin(map          "${RADIANT_SOURCE_ROOT}/plugins/map")
	radiant_add_plugin(mapxml       "${RADIANT_SOURCE_ROOT}/plugins/mapxml")
	radiant_add_plugin(model        "${RADIANT_SOURCE_ROOT}/plugins/model")
	radiant_add_plugin(shaders      "${RADIANT_SOURCE_ROOT}/plugins/shaders")
	radiant_add_plugin(surface      "${RADIANT_SOURCE_ROOT}/plugins/surface")
	radiant_add_plugin(vfspk3       "${RADIANT_SOURCE_ROOT}/plugins/vfspk3")
	radiant_add_plugin(vfspak       "${RADIANT_SOURCE_ROOT}/plugins/vfspak")
	radiant_add_plugin(vfswad       "${RADIANT_SOURCE_ROOT}/plugins/vfswad")
	radiant_add_plugin(fgd          "${RADIANT_SOURCE_ROOT}/plugins/eclassfgd")
	radiant_add_plugin(imagewal     "${RADIANT_SOURCE_ROOT}/plugins/imagewal")
	radiant_add_plugin(imagehl      "${RADIANT_SOURCE_ROOT}/plugins/imagehl")
	radiant_add_plugin(imagem8      "${RADIANT_SOURCE_ROOT}/plugins/imagem8")
	radiant_add_plugin(spritemodel  "${RADIANT_SOURCE_ROOT}/plugins/spritemodel")
	radiant_add_plugin(textool      "${RADIANT_SOURCE_ROOT}/plugins/textool")
	radiant_add_plugin(surface_idtech2 "${RADIANT_SOURCE_ROOT}/plugins/surface_idtech2")
endif()

if(RADIANT_BUILD_CONTRIB)
	radiant_add_plugin(camera    "${RADIANT_SOURCE_ROOT}/contrib/camera")
	radiant_add_plugin(prtview   "${RADIANT_SOURCE_ROOT}/contrib/prtview")
	radiant_add_plugin(hydratoolz "${RADIANT_SOURCE_ROOT}/contrib/hydratoolz")
	radiant_add_plugin(bobtoolz  "${RADIANT_SOURCE_ROOT}/contrib/bobtoolz")
	radiant_add_plugin(gtkgensurf "${RADIANT_SOURCE_ROOT}/contrib/gtkgensurf")
	radiant_add_plugin(bkgrnd2d  "${RADIANT_SOURCE_ROOT}/contrib/bkgrnd2d")
endif()
