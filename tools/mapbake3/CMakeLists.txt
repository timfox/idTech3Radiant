cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(mapbake3 C CXX)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
# Enable Debug by default, can be changed with -DCMAKE_BUILD_TYPE=Release
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

#-----------------------------------------------------------------------
# Version
#-----------------------------------------------------------------------

# CMake 3.0+ would allow this in project()
set(Radiant_VERSION_MAJOR 1)
set(Radiant_VERSION_MINOR 5)
set(Radiant_VERSION_PATCH 0)
set(Radiant_VERSION "${Radiant_VERSION_MAJOR}.${Radiant_VERSION_MINOR}.${Radiant_VERSION_PATCH}")


#-----------------------------------------------------------------------
# Language standard
#-----------------------------------------------------------------------

set(MAPBAKE3_C_STANDARD 17 CACHE STRING "C language standard for mapbake3 (17 or 23)")
set_property(CACHE MAPBAKE3_C_STANDARD PROPERTY STRINGS 17 23)

set(MAPBAKE3_CXX_STANDARD 17 CACHE STRING "C++ language standard for mapbake3 (17 or 23)")
set_property(CACHE MAPBAKE3_CXX_STANDARD PROPERTY STRINGS 17 23)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

check_c_compiler_flag(--std=c${MAPBAKE3_C_STANDARD} STD_C)
if (STD_C)
    list(APPEND CMAKE_C_FLAGS --std=c${MAPBAKE3_C_STANDARD})
else ()
    message(SEND_ERROR "Requires C${MAPBAKE3_C_STANDARD} or better")
endif ()

check_cxx_compiler_flag(--std=c++${MAPBAKE3_CXX_STANDARD} STD_CXX)
if (STD_CXX)
    list(APPEND CMAKE_CXX_FLAGS --std=c++${MAPBAKE3_CXX_STANDARD})
else ()
    message(SEND_ERROR "Requires C++${MAPBAKE3_CXX_STANDARD} or better")
endif ()

#-----------------------------------------------------------------------
# Flags
#-----------------------------------------------------------------------

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}   -fno-strict-aliasing -fcommon")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing -fno-exceptions -fno-rtti")
set(CMAKE_POSITION_INDEPENDENT_CODE 1)

#-----------------------------------------------------------------------
# Defs
#-----------------------------------------------------------------------

add_definitions(-DRADIANT_VERSION="${Radiant_VERSION}")
add_definitions(-DRADIANT_MAJOR_VERSION="${Radiant_VERSION_MAJOR}")
add_definitions(-DRADIANT_MINOR_VERSION="${Radiant_VERSION_MINOR}")

add_definitions(-DRADIANT_ABOUTMSG="Radiant")

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-D_DEBUG=1)
endif ()

include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories("${PROJECT_SOURCE_DIR}/libs")

#-----------------------------------------------------------------------
# Libraries
#-----------------------------------------------------------------------

add_subdirectory(libs)
add_subdirectory(include)

#-----------------------------------------------------------------------
# Tools
#-----------------------------------------------------------------------

add_subdirectory(tools)
