cmake_minimum_required(VERSION 3.16)
project(NetRadiantCustom VERSION 1.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------
set(BUILD_TYPE "release" CACHE STRING "Build type: release debug extradebug profile native")
option(DOWNLOAD_GAMEPACKS "Download gamepacks: no yes all allinone" "no")
option(INSTALL_DATA "Install data files (themes, bitmaps, docs)" ON)
option(ASSIMP_INTERNAL "Use bundled assimp instead of system" OFF)

set(INSTALLDIR "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install directory")
set(RADIANT_ABOUTMSG "Custom build" CACHE STRING "About message")

# --------------------------------------------------------------------------
# Version info from git
# --------------------------------------------------------------------------
execute_process(COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GIT_VERSION)
    set(GIT_VERSION "unknown")
endif()
set(RADIANT_VERSION "1.6.0n-git-${GIT_VERSION}")
set(RADIANT_MAJOR_VERSION "6")
set(RADIANT_MINOR_VERSION "0")
set(Q3MAP_VERSION "2.5.17n-git-${GIT_VERSION}")
execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT ARCH)
    set(ARCH "x86_64")
endif()
set(RADIANT_EXECUTABLE "${ARCH}")

# --------------------------------------------------------------------------
# Platform detection
# --------------------------------------------------------------------------
if(WIN32)
    set(PLATFORM_DEFINES -DWIN32 -D_WIN32 -D_inline=inline)
    set(PLATFORM_LIBS ws2_32 user32 gdi32 ole32)
    set(DLL_EXT ".dll")
    set(EXE_EXT ".exe")
else()
    set(PLATFORM_DEFINES -DPOSIX -DXWINDOWS)
    set(PLATFORM_LIBS pthread)
    set(DLL_EXT ".so")
    set(EXE_EXT ".${ARCH}")
    if(APPLE)
        set(DLL_EXT ".dylib")
        list(APPEND PLATFORM_LIBS dl)
    else()
        list(APPEND PLATFORM_LIBS dl)
    endif()
endif()

# --------------------------------------------------------------------------
# Build configuration flags
# --------------------------------------------------------------------------
set(COMMON_C_FLAGS "-W -Wall -Wcast-align -Wcast-qual -Wno-unused-parameter -Wno-unused-function -fno-strict-aliasing")
set(COMMON_CXX_FLAGS "${COMMON_C_FLAGS} -Wreorder -fno-exceptions -fno-rtti")

if(BUILD_TYPE STREQUAL "release" OR BUILD_TYPE STREQUAL "native")
    set(OPT_FLAGS "-O3")
    set(DEF_FLAGS "-DNDEBUG")
    set(LINK_FLAGS "-s")
    if(BUILD_TYPE STREQUAL "native")
        set(OPT_FLAGS "${OPT_FLAGS} -march=native -mtune=native")
    endif()
elseif(BUILD_TYPE STREQUAL "debug")
    set(OPT_FLAGS "-Og -ggdb")
    set(DEF_FLAGS "")
    set(LINK_FLAGS "")
elseif(BUILD_TYPE STREQUAL "extradebug")
    set(OPT_FLAGS "-ggdb3")
    set(DEF_FLAGS "-D_DEBUG")
    set(LINK_FLAGS "")
elseif(BUILD_TYPE STREQUAL "profile")
    set(OPT_FLAGS "-O -g -pg")
    set(DEF_FLAGS "")
    set(LINK_FLAGS "-pg")
else()
    set(OPT_FLAGS "-O3")
    set(DEF_FLAGS "-DNDEBUG")
    set(LINK_FLAGS "-s")
endif()

set(VERSION_DEFINES
    -DRADIANT_VERSION="${RADIANT_VERSION}"
    -DRADIANT_MAJOR_VERSION="${RADIANT_MAJOR_VERSION}"
    -DRADIANT_MINOR_VERSION="${RADIANT_MINOR_VERSION}"
    -DRADIANT_ABOUTMSG="${RADIANT_ABOUTMSG}"
    -DQ3MAP_VERSION="${Q3MAP_VERSION}"
    -DRADIANT_EXECUTABLE="${RADIANT_EXECUTABLE}"
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_C_FLAGS} ${OPT_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_CXX_FLAGS} ${OPT_FLAGS} -Wno-deprecated-copy")
add_compile_definitions(${PLATFORM_DEFINES} ${VERSION_DEFINES})

if(DEF_FLAGS)
    add_compile_definitions(${DEF_FLAGS})
endif()

# --------------------------------------------------------------------------
# Find packages
# --------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(XML2 REQUIRED libxml-2.0)
pkg_check_modules(PNG REQUIRED libpng)
pkg_check_modules(QT5CORE REQUIRED Qt5Core)
pkg_check_modules(QT5GUI REQUIRED Qt5Gui)
pkg_check_modules(QT5WIDGETS REQUIRED Qt5Widgets)
pkg_check_modules(QT5SVG REQUIRED Qt5Svg)

# Optional
pkg_check_modules(QT5MULTIMEDIA Qt5Multimedia)
pkg_check_modules(QT5MULTIMEDIAWIDGETS Qt5MultimediaWidgets)

find_package(JPEG)
find_package(ZLIB REQUIRED)
find_package(OpenGL REQUIRED)

if(NOT ASSIMP_INTERNAL)
    pkg_check_modules(ASSIMP REQUIRED assimp)
endif()

# Generate assimp include config header
if(ASSIMP_INTERNAL)
    file(WRITE "${CMAKE_BINARY_DIR}/assimp_config.h" "#define ASSIMP_INCLUDE(x) #x\n")
else()
    file(WRITE "${CMAKE_BINARY_DIR}/assimp_config.h" "#define ASSIMP_INCLUDE(x) <x>\n")
endif()

# Qt needs -DQT_NO_KEYWORDS added to all Qt pkg-config results
set(QT_NO_KEYWORDS_DEF -DQT_NO_KEYWORDS)

# --------------------------------------------------------------------------
# Helper: collect common include dirs and flags
# --------------------------------------------------------------------------
set(RADIANT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs
)

# --------------------------------------------------------------------------
# Static libraries
# --------------------------------------------------------------------------

# mathlib
add_library(mathlib STATIC
    libs/mathlib/bbox.c libs/mathlib/line.c libs/mathlib/m4x4.c
    libs/mathlib/mathlib.c libs/mathlib/ray.c)
target_include_directories(mathlib PRIVATE libs)

# l_net
add_library(l_net STATIC libs/l_net/l_net.c libs/l_net/l_net_wins.c)
target_include_directories(l_net PRIVATE libs)

# commandlib
add_library(commandlib STATIC libs/commandlib.cpp)
target_include_directories(commandlib PRIVATE ${RADIANT_INCLUDE_DIRS}
    ${QT5CORE_INCLUDE_DIRS} ${QT5GUI_INCLUDE_DIRS} ${QT5WIDGETS_INCLUDE_DIRS}
    ${QT5SVG_INCLUDE_DIRS} ${XML2_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS})
target_compile_definitions(commandlib PRIVATE ${QT_NO_KEYWORDS_DEF})

# filematch
add_library(filematch STATIC libs/filematch.c)
target_include_directories(filematch PRIVATE libs)

# crnlib
add_library(crnlib STATIC libs/crnlib/crnlib.cpp)
target_include_directories(crnlib PRIVATE libs)

# ddslib
add_library(ddslib STATIC libs/ddslib/ddslib.c)
target_include_directories(ddslib PRIVATE libs)

# etclib
add_library(etclib STATIC libs/etclib.c)
target_include_directories(etclib PRIVATE libs)

# webplib
add_library(webplib STATIC libs/webplib/webplib.cpp)
target_include_directories(webplib PRIVATE libs)

# quickhull
add_library(quickhull STATIC libs/quickhull/QuickHull.cpp)
target_include_directories(quickhull PRIVATE libs)

# gtkutil
file(GLOB GTKUTIL_SRCS libs/gtkutil/*.cpp)
add_library(gtkutil STATIC ${GTKUTIL_SRCS})
target_include_directories(gtkutil PRIVATE ${RADIANT_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIRS} ${QT5WIDGETS_INCLUDE_DIRS} ${QT5GUI_INCLUDE_DIRS}
    ${QT5CORE_INCLUDE_DIRS} ${QT5SVG_INCLUDE_DIRS}
    ${XML2_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS})
target_compile_definitions(gtkutil PRIVATE ${QT_NO_KEYWORDS_DEF})

# xmllib
file(GLOB XMLLIB_SRCS libs/xml/*.cpp)
add_library(xmllib STATIC ${XMLLIB_SRCS})
target_include_directories(xmllib PRIVATE ${RADIANT_INCLUDE_DIRS}
    ${XML2_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS}
    ${QT5CORE_INCLUDE_DIRS} ${QT5GUI_INCLUDE_DIRS} ${QT5WIDGETS_INCLUDE_DIRS}
    ${QT5SVG_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS})
target_compile_definitions(xmllib PRIVATE ${QT_NO_KEYWORDS_DEF})

# Recast libraries
file(GLOB RECAST_SRCS libs/recast/Recast/Source/*.cpp)
add_library(recast STATIC ${RECAST_SRCS})
target_include_directories(recast PRIVATE libs/recast/Recast/Include libs/recast/Detour/Include)

file(GLOB DETOUR_SRCS libs/recast/Detour/Source/*.cpp)
add_library(detour STATIC ${DETOUR_SRCS})
target_include_directories(detour PRIVATE libs/recast/Detour/Include libs/recast/Recast/Include)

file(GLOB DETOURCROWD_SRCS libs/recast/DetourCrowd/Source/*.cpp)
add_library(detourcrowd STATIC ${DETOURCROWD_SRCS})
target_include_directories(detourcrowd PRIVATE
    libs/recast/DetourCrowd/Include libs/recast/Detour/Include libs/recast/Recast/Include)

file(GLOB DETOURTILECACHE_SRCS libs/recast/DetourTileCache/Source/*.cpp)
add_library(detourtilecache STATIC ${DETOURTILECACHE_SRCS})
target_include_directories(detourtilecache PRIVATE
    libs/recast/DetourTileCache/Include libs/recast/Detour/Include libs/recast/Recast/Include)

# --------------------------------------------------------------------------
# radiant (main editor)
# --------------------------------------------------------------------------
file(GLOB RADIANT_SRCS radiant/*.cpp)
add_executable(radiant ${RADIANT_SRCS})
target_include_directories(radiant PRIVATE ${RADIANT_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS} ${XML2_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS}
    ${QT5WIDGETS_INCLUDE_DIRS} ${QT5GUI_INCLUDE_DIRS} ${QT5CORE_INCLUDE_DIRS}
    ${QT5SVG_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS}
    libs/recast/Recast/Include libs/recast/Detour/Include
    libs/recast/DetourCrowd/Include libs/recast/DetourTileCache/Include)
if(QT5MULTIMEDIA_FOUND)
    target_include_directories(radiant PRIVATE ${QT5MULTIMEDIA_INCLUDE_DIRS} ${QT5MULTIMEDIAWIDGETS_INCLUDE_DIRS})
    target_link_libraries(radiant PRIVATE ${QT5MULTIMEDIA_LIBRARIES} ${QT5MULTIMEDIAWIDGETS_LIBRARIES})
endif()
target_compile_definitions(radiant PRIVATE ${QT_NO_KEYWORDS_DEF})
target_compile_options(radiant PRIVATE -fPIC)
target_link_libraries(radiant PRIVATE
    commandlib gtkutil l_net quickhull xmllib
    recast detour detourcrowd detourtilecache
    ${OPENGL_LIBRARIES} ${XML2_LIBRARIES} ${GLIB2_LIBRARIES}
    ${QT5WIDGETS_LIBRARIES} ${QT5GUI_LIBRARIES} ${QT5CORE_LIBRARIES}
    ${QT5SVG_LIBRARIES} ${ZLIB_LIBRARIES}
    ${PLATFORM_LIBS})
if(NOT WIN32)
    target_link_libraries(radiant PRIVATE dl)
endif()
set_target_properties(radiant PROPERTIES
    OUTPUT_NAME "radiant.${ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${INSTALLDIR}")

# --------------------------------------------------------------------------
# Modules (shared libraries)
# --------------------------------------------------------------------------
macro(add_module name)
    add_library(${name} MODULE ${ARGN})
    target_include_directories(${name} PRIVATE ${RADIANT_INCLUDE_DIRS}
        ${QT5CORE_INCLUDE_DIRS} ${QT5GUI_INCLUDE_DIRS} ${QT5WIDGETS_INCLUDE_DIRS}
        ${QT5SVG_INCLUDE_DIRS} ${XML2_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS})
    target_compile_definitions(${name} PRIVATE ${QT_NO_KEYWORDS_DEF})
    target_link_libraries(${name} PRIVATE ${PLATFORM_LIBS})
    if(NOT WIN32)
        target_link_libraries(${name} PRIVATE dl)
    endif()
    set_target_properties(${name} PROPERTIES
        PREFIX "" SUFFIX "${DLL_EXT}"
        LIBRARY_OUTPUT_DIRECTORY "${INSTALLDIR}/modules")
endmacro()

# archivezip
add_module(archivezip
    plugins/archivezip/archive.cpp plugins/archivezip/pkzip.cpp
    plugins/archivezip/plugin.cpp plugins/archivezip/zlibstream.cpp)
target_link_libraries(archivezip PRIVATE ${ZLIB_LIBRARIES})

# archivewad
add_module(archivewad
    plugins/archivewad/archive.cpp plugins/archivewad/plugin.cpp plugins/archivewad/wad.cpp)

# archivepak
add_module(archivepak
    plugins/archivepak/archive.cpp plugins/archivepak/pak.cpp plugins/archivepak/plugin.cpp)

# entity
file(GLOB ENTITY_SRCS plugins/entity/*.cpp)
add_module(entity ${ENTITY_SRCS})

# image
file(GLOB IMAGE_SRCS plugins/image/*.cpp)
add_module(image ${IMAGE_SRCS})
if(JPEG_FOUND)
    target_link_libraries(image PRIVATE ${JPEG_LIBRARIES})
    target_include_directories(image PRIVATE ${JPEG_INCLUDE_DIRS})
endif()

# imageq2
add_module(imageq2 plugins/imageq2/imageq2.cpp plugins/imageq2/wal32.cpp plugins/imageq2/wal.cpp)

# imagehl
add_module(imagehl plugins/imagehl/hlw.cpp plugins/imagehl/imagehl.cpp
    plugins/imagehl/mip.cpp plugins/imagehl/sprite.cpp)

# imagepng
add_module(imagepng plugins/imagepng/plugin.cpp)
target_link_libraries(imagepng PRIVATE ${PNG_LIBRARIES})

# mapq3
add_module(mapq3
    plugins/mapq3/maparse.cpp plugins/mapq3/parse.cpp plugins/mapq3/plugin.cpp
    plugins/mapq3/usdparse.cpp plugins/mapq3/write.cpp)

# mapxml
add_module(mapxml plugins/mapxml/plugin.cpp plugins/mapxml/xmlparse.cpp plugins/mapxml/xmlwrite.cpp)
target_link_libraries(mapxml PRIVATE ${XML2_LIBRARIES} ${GLIB2_LIBRARIES})

# assmodel
add_module(assmodel
    plugins/assmodel/mayaascii.cpp plugins/assmodel/mdlimage.cpp
    plugins/assmodel/model.cpp plugins/assmodel/plugin.cpp)
target_compile_options(assmodel PRIVATE "-include${CMAKE_BINARY_DIR}/assimp_config.h")
if(NOT ASSIMP_INTERNAL)
    target_include_directories(assmodel PRIVATE ${ASSIMP_INCLUDE_DIRS})
    target_link_libraries(assmodel PRIVATE ${ASSIMP_LIBRARIES})
endif()

# shaders
add_module(shaders plugins/shaders/plugin.cpp plugins/shaders/shaders.cpp)
target_link_libraries(shaders PRIVATE commandlib ${GLIB2_LIBRARIES})

# vfspk3
add_module(vfspk3 plugins/vfspk3/archive.cpp plugins/vfspk3/vfs.cpp plugins/vfspk3/vfspk3.cpp)
target_link_libraries(vfspk3 PRIVATE filematch ${GLIB2_LIBRARIES})

# --------------------------------------------------------------------------
# Plugins (shared libraries)
# --------------------------------------------------------------------------
macro(add_plugin name)
    add_library(${name} MODULE ${ARGN})
    target_include_directories(${name} PRIVATE ${RADIANT_INCLUDE_DIRS}
        ${GLIB2_INCLUDE_DIRS} ${QT5WIDGETS_INCLUDE_DIRS} ${QT5GUI_INCLUDE_DIRS}
        ${QT5CORE_INCLUDE_DIRS} ${QT5SVG_INCLUDE_DIRS}
        ${XML2_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS})
    target_compile_definitions(${name} PRIVATE ${QT_NO_KEYWORDS_DEF})
    target_link_libraries(${name} PRIVATE ${GLIB2_LIBRARIES} ${QT5WIDGETS_LIBRARIES}
        ${QT5GUI_LIBRARIES} ${QT5CORE_LIBRARIES} ${PLATFORM_LIBS})
    if(NOT WIN32)
        target_link_libraries(${name} PRIVATE dl)
    endif()
    set_target_properties(${name} PROPERTIES
        PREFIX "" SUFFIX "${DLL_EXT}"
        LIBRARY_OUTPUT_DIRECTORY "${INSTALLDIR}/plugins")
endmacro()

# bobtoolz
set(BOBTOOLZ_SRCS
    contrib/bobtoolz/bobToolz-GTK.cpp contrib/bobtoolz/bsploader.cpp
    contrib/bobtoolz/cportals.cpp contrib/bobtoolz/DBobView.cpp
    contrib/bobtoolz/DBrush.cpp contrib/bobtoolz/DEntity.cpp
    contrib/bobtoolz/DEPair.cpp contrib/bobtoolz/dialogs/dialogs-gtk.cpp
    contrib/bobtoolz/DMap.cpp contrib/bobtoolz/DPatch.cpp
    contrib/bobtoolz/DPlane.cpp contrib/bobtoolz/DPoint.cpp
    contrib/bobtoolz/DShape.cpp contrib/bobtoolz/DTrainDrawer.cpp
    contrib/bobtoolz/DTreePlanter.cpp contrib/bobtoolz/DVisDrawer.cpp
    contrib/bobtoolz/DWinding.cpp contrib/bobtoolz/funchandlers-GTK.cpp
    contrib/bobtoolz/lists.cpp contrib/bobtoolz/misc.cpp
    contrib/bobtoolz/ScriptParser.cpp contrib/bobtoolz/shapes.cpp
    contrib/bobtoolz/visfind.cpp)
add_plugin(bobtoolz ${BOBTOOLZ_SRCS})
target_link_libraries(bobtoolz PRIVATE commandlib mathlib)

# brushexport
file(GLOB BRUSHEXPORT_SRCS contrib/brushexport/*.cpp)
add_plugin(brushexport ${BRUSHEXPORT_SRCS})

# prtview
file(GLOB PRTVIEW_SRCS contrib/prtview/*.cpp)
add_plugin(prtview ${PRTVIEW_SRCS})

# shaderplug
add_plugin(shaderplug contrib/shaderplug/shaderplug.cpp)
target_link_libraries(shaderplug PRIVATE xmllib ${XML2_LIBRARIES})

# sunplug
add_plugin(sunplug contrib/sunplug/sunplug.cpp)

# ufoaiplug
file(GLOB UFOAI_SRCS contrib/ufoaiplug/*.cpp)
add_plugin(ufoaiplug ${UFOAI_SRCS})

# meshtex
file(GLOB MESHTEX_SRCS contrib/meshtex/*.cpp)
add_plugin(meshtex ${MESHTEX_SRCS})

# --------------------------------------------------------------------------
# Tools
# --------------------------------------------------------------------------

# q3map2
file(GLOB Q3MAP2_COMMON_SRCS tools/quake3/common/*.cpp tools/quake3/common/*.c)
file(GLOB Q3MAP2_SRCS tools/quake3/q3map2/*.cpp)
add_executable(q3map2 ${Q3MAP2_COMMON_SRCS} ${Q3MAP2_SRCS})
target_include_directories(q3map2 PRIVATE
    ${RADIANT_INCLUDE_DIRS} tools/quake3/common
    ${XML2_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS})
if(JPEG_FOUND)
    target_include_directories(q3map2 PRIVATE ${JPEG_INCLUDE_DIRS})
endif()
target_compile_options(q3map2 PRIVATE "-include${CMAKE_BINARY_DIR}/assimp_config.h")
target_compile_definitions(q3map2 PRIVATE ${QT_NO_KEYWORDS_DEF})
target_link_libraries(q3map2 PRIVATE
    crnlib ddslib etclib filematch l_net webplib
    ${XML2_LIBRARIES} ${GLIB2_LIBRARIES} ${PNG_LIBRARIES}
    ${ZLIB_LIBRARIES} ${PLATFORM_LIBS})
if(JPEG_FOUND)
    target_link_libraries(q3map2 PRIVATE ${JPEG_LIBRARIES})
endif()
if(NOT ASSIMP_INTERNAL AND ASSIMP_FOUND)
    target_include_directories(q3map2 PRIVATE ${ASSIMP_INCLUDE_DIRS})
    target_link_libraries(q3map2 PRIVATE ${ASSIMP_LIBRARIES})
endif()
set_target_properties(q3map2 PROPERTIES
    OUTPUT_NAME "q3map2.${ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${INSTALLDIR}")

# q2map
file(GLOB Q2MAP_COMMON_SRCS tools/quake2/common/*.c)
set(Q2MAP_SRCS
    tools/quake2/q2map/brushbsp.c tools/quake2/q2map/csg.c
    tools/quake2/q2map/faces.c tools/quake2/q2map/flow.c
    tools/quake2/q2map/glfile.c tools/quake2/q2map/leakfile.c
    tools/quake2/q2map/lightmap.c tools/quake2/q2map/main.c
    tools/quake2/q2map/map.c tools/quake2/q2map/nodraw.c
    tools/quake2/q2map/patches.c tools/quake2/q2map/portals.c
    tools/quake2/q2map/prtfile.c tools/quake2/q2map/qbsp.c
    tools/quake2/q2map/qrad.c tools/quake2/q2map/qvis.c
    tools/quake2/q2map/textures.c tools/quake2/q2map/trace.c
    tools/quake2/q2map/tree.c tools/quake2/q2map/writebsp.c)
add_executable(q2map ${Q2MAP_COMMON_SRCS} ${Q2MAP_SRCS})
target_include_directories(q2map PRIVATE tools/quake2/common libs include ${XML2_INCLUDE_DIRS})
target_compile_options(q2map PRIVATE -Wno-format-overflow)
target_link_libraries(q2map PRIVATE l_net ${XML2_LIBRARIES} ${PLATFORM_LIBS} m)
set_target_properties(q2map PROPERTIES
    OUTPUT_NAME "q2map.${ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${INSTALLDIR}")

# qdata3
file(GLOB QDATA3_SRCS tools/quake2/qdata/*.c)
add_executable(qdata3 ${Q2MAP_COMMON_SRCS} ${QDATA3_SRCS})
target_include_directories(qdata3 PRIVATE tools/quake2/common libs include ${XML2_INCLUDE_DIRS})
target_compile_options(qdata3 PRIVATE -Wno-format-overflow)
target_link_libraries(qdata3 PRIVATE l_net ${XML2_LIBRARIES} ${PLATFORM_LIBS} m)
set_target_properties(qdata3 PROPERTIES
    OUTPUT_NAME "qdata3.${ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${INSTALLDIR}")

# h2data
file(GLOB H2DATA_COMMON_SRCS tools/quake2/qdata_heretic2/common/*.c)
file(GLOB H2DATA_QCOMMON_SRCS tools/quake2/qdata_heretic2/qcommon/*.c)
file(GLOB H2DATA_SRCS tools/quake2/qdata_heretic2/*.c)
add_executable(h2data ${H2DATA_COMMON_SRCS} ${H2DATA_QCOMMON_SRCS} ${H2DATA_SRCS})
target_include_directories(h2data PRIVATE
    tools/quake2/qdata_heretic2/common tools/quake2/qdata_heretic2/qcommon
    tools/quake2/qdata_heretic2 tools/quake2/common libs include
    ${XML2_INCLUDE_DIRS})
target_compile_options(h2data PRIVATE -Wno-format-overflow)
target_link_libraries(h2data PRIVATE l_net ${XML2_LIBRARIES} ${PLATFORM_LIBS} m)
set_target_properties(h2data PROPERTIES
    OUTPUT_NAME "h2data.${ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${INSTALLDIR}")

# mbspc
file(GLOB MBSPC_BOTLIB_SRCS tools/mbspc/botlib/*.c)
file(GLOB MBSPC_QCOMMON_SRCS tools/mbspc/qcommon/*.c)
set(MBSPC_MAIN_SRCS
    tools/mbspc/mbspc/aas_areamerging.c tools/mbspc/mbspc/aas_cfg.c
    tools/mbspc/mbspc/aas_create.c tools/mbspc/mbspc/aas_edgemelting.c
    tools/mbspc/mbspc/aas_facemerging.c tools/mbspc/mbspc/aas_file.c
    tools/mbspc/mbspc/aas_gsubdiv.c tools/mbspc/mbspc/aas_map.c
    tools/mbspc/mbspc/aas_prunenodes.c tools/mbspc/mbspc/aas_store.c
    tools/mbspc/mbspc/be_aas_bspc.c tools/mbspc/mbspc/brushbsp.c
    tools/mbspc/mbspc/bspc.c tools/mbspc/mbspc/csg.c
    tools/mbspc/mbspc/faces.c tools/mbspc/mbspc/glfile.c
    tools/mbspc/mbspc/l_bsp_ent.c tools/mbspc/mbspc/l_bsp_hl.c
    tools/mbspc/mbspc/l_bsp_q1.c tools/mbspc/mbspc/l_bsp_q2.c
    tools/mbspc/mbspc/l_bsp_q3.c tools/mbspc/mbspc/l_bsp_sin.c
    tools/mbspc/mbspc/l_cmd.c tools/mbspc/mbspc/l_log.c
    tools/mbspc/mbspc/l_math.c tools/mbspc/mbspc/l_mem.c
    tools/mbspc/mbspc/l_poly.c tools/mbspc/mbspc/l_qfiles.c
    tools/mbspc/mbspc/l_threads.c tools/mbspc/mbspc/l_utils.c
    tools/mbspc/mbspc/leakfile.c tools/mbspc/mbspc/map.c
    tools/mbspc/mbspc/map_hl.c tools/mbspc/mbspc/map_q1.c
    tools/mbspc/mbspc/map_q2.c tools/mbspc/mbspc/map_q3.c
    tools/mbspc/mbspc/map_sin.c tools/mbspc/mbspc/nodraw.c
    tools/mbspc/mbspc/portals.c tools/mbspc/mbspc/prtfile.c
    tools/mbspc/mbspc/textures.c tools/mbspc/mbspc/tree.c
    tools/mbspc/mbspc/writebsp.c)
add_executable(mbspc ${MBSPC_BOTLIB_SRCS} ${MBSPC_MAIN_SRCS} ${MBSPC_QCOMMON_SRCS})
target_include_directories(mbspc PRIVATE libs tools/mbspc/botlib tools/mbspc/mbspc tools/mbspc/qcommon)
target_compile_definitions(mbspc PRIVATE BSPC BSPCINCLUDE NDEBUG)
target_compile_options(mbspc PRIVATE -Wstrict-prototypes -Wno-format-overflow)
target_link_libraries(mbspc PRIVATE ${PLATFORM_LIBS} m)
set_target_properties(mbspc PROPERTIES
    OUTPUT_NAME "mbspc.${ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${INSTALLDIR}")

# --------------------------------------------------------------------------
# Install data
# --------------------------------------------------------------------------
if(INSTALL_DATA)
    add_custom_target(install-data ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALLDIR}/gamepacks/games"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/setup/data/tools" "${INSTALLDIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALLDIR}/docs"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/docs" "${INSTALLDIR}/docs"
        COMMAND ${CMAKE_COMMAND} -E echo "0" > "${INSTALLDIR}/RADIANT_MINOR"
        COMMAND ${CMAKE_COMMAND} -E echo "6" > "${INSTALLDIR}/RADIANT_MAJOR"
        COMMENT "Installing data files"
    )
endif()

# --------------------------------------------------------------------------
# Symlinks (Linux/macOS)
# --------------------------------------------------------------------------
if(NOT WIN32)
    foreach(tool radiant q3map2 q2map qdata3 h2data mbspc)
        add_custom_command(TARGET ${tool} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink "${tool}.${ARCH}" "${INSTALLDIR}/${tool}"
            COMMENT "Creating symlink ${tool}")
    endforeach()
endif()

# --------------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------------
message(STATUS "")
message(STATUS "NetRadiant-Custom CMake Build")
message(STATUS "  Version:    ${RADIANT_VERSION}")
message(STATUS "  Build type: ${BUILD_TYPE}")
message(STATUS "  Install:    ${INSTALLDIR}")
message(STATUS "  Platform:   ${CMAKE_SYSTEM_NAME} ${ARCH}")
message(STATUS "  Assimp:     ${ASSIMP_INTERNAL}")
message(STATUS "")
