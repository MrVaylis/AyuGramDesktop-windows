# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

cmake_minimum_required(VERSION 3.25...3.31)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(cmake/validate_special_target.cmake)
include(cmake/version.cmake)
desktop_app_parse_version(Telegram/build/version)

if (NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(configuration_types_init 1)
endif()

project(Telegram
    LANGUAGES C CXX
    VERSION ${desktop_app_version_cmake}
    DESCRIPTION "AyuGram Desktop"
    HOMEPAGE_URL "https://ayugram.one"
)

if (APPLE)
    enable_language(OBJC OBJCXX)
endif()

if (configuration_types_init
    AND CMAKE_CONFIGURATION_TYPES
    AND NOT MinSizeRel IN_LIST CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES};MinSizeRel" CACHE STRING "" FORCE)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Telegram)

get_filename_component(third_party_loc "Telegram/ThirdParty" REALPATH)
get_filename_component(submodules_loc "Telegram" REALPATH)
get_filename_component(cmake_helpers_loc "cmake" REALPATH)

set(AYUGRAM_LIBRARIES_DIR "" CACHE PATH "Directory containing prepared Windows libraries for the target architecture.")
set(AYUGRAM_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty" CACHE PATH "Directory containing prepared build tools.")
file(REAL_PATH "${AYUGRAM_THIRD_PARTY_DIR}" ayugram_third_party_dir BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

if (WIN32 AND AYUGRAM_LIBRARIES_DIR)
    set(DESKTOP_APP_USE_PACKAGED OFF)
endif()

if (NOT DESKTOP_APP_USE_PACKAGED AND WIN32)
    set(Python3_EXECUTABLE "${ayugram_third_party_dir}/python/Scripts/python.exe")
    list(PREPEND CMAKE_PROGRAM_PATH "${ayugram_third_party_dir}/NuGet")
endif()

option(AYUGRAM_USE_CCACHE "Cache C and C++ compilation with ccache (Ninja)." OFF)
if (AYUGRAM_USE_CCACHE)
    if (NOT CMAKE_GENERATOR MATCHES "^Ninja")
        message(FATAL_ERROR "AYUGRAM_USE_CCACHE requires a Ninja generator.")
    endif()
    find_program(AYUGRAM_CCACHE_EXECUTABLE NAMES ccache HINTS "${ayugram_third_party_dir}/ccache" REQUIRED)
    set(CMAKE_C_COMPILER_LAUNCHER "${AYUGRAM_CCACHE_EXECUTABLE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${AYUGRAM_CCACHE_EXECUTABLE}")
    if (MSVC)
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
    endif()
endif()

include(cmake/variables.cmake)
if (WIN32 AND AYUGRAM_LIBRARIES_DIR)
    file(REAL_PATH "${AYUGRAM_LIBRARIES_DIR}" libs_loc BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    if (NOT IS_DIRECTORY "${libs_loc}")
        message(FATAL_ERROR "Prepared libraries directory does not exist: ${libs_loc}")
    endif()
endif()
include(cmake/nice_target_sources.cmake)
include(cmake/target_compile_options_if_exists.cmake)
include(cmake/target_link_frameworks.cmake)
include(cmake/target_link_options_if_exists.cmake)
include(cmake/init_target.cmake)
include(cmake/generate_target.cmake)
include(cmake/nuget.cmake)
include(cmake/validate_d3d_compiler.cmake)
include(cmake/target_prepare_qrc.cmake)

set(DESKTOP_APP_DISABLE_AUTOUPDATE ON CACHE BOOL "Custom AyuGram releases use the repository release channel." FORCE)

include(cmake/options.cmake)

include(cmake/external/qt/package.cmake)

set(desktop_app_skip_libs
    glibmm
    variant
)

add_subdirectory(cmake)
add_subdirectory(Telegram)
