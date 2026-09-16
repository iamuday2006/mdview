cmake_minimum_required(VERSION 3.20)

# Record the user's intent before project() runs.  CMake gives
# CMAKE_BUILD_TYPE a default of its own for single-configuration generators, so
# after project() an empty value no longer means "the user did not choose".
set(MDVIEW_REQUESTED_BUILD_TYPE "${CMAKE_BUILD_TYPE}")

project(mdview
    VERSION 0.1.0
    DESCRIPTION "mdview - a cross-platform terminal Markdown workspace"
    LANGUAGES CXX)

set(MDVIEW_AUTHOR "iamuday2006")
set(MDVIEW_HOMEPAGE "https://github.com/iamuday2006/mdview")
set(MDVIEW_REPOSITORY "https://github.com/iamuday2006/mdview")
set(MDVIEW_LICENSE "MIT")
set(MDVIEW_COPYRIGHT "Copyright (c) 2026 iamuday2006. All rights reserved.")

# ---------------------------------------------------------------------------
# One language dialect and one build abstraction, on every platform.
#
#   cmake -S . -B build
#   cmake --build build --config Release
#
# The Visual Studio generator is deliberately not hard-coded: CMake picks the
# newest toolchain that is actually installed.  Use -G to override.
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Single-configuration generators (Ninja, Unix Makefiles) default to Release so
# that the plain two-line build above behaves like the multi-config Visual
# Studio generators, where --config Release does the same job.  An explicit
# -DCMAKE_BUILD_TYPE=... always wins.
if(NOT CMAKE_CONFIGURATION_TYPES)
    if(MDVIEW_REQUESTED_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "${MDVIEW_REQUESTED_BUILD_TYPE}" CACHE STRING "Build type" FORCE)
    elseif(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "" OR
           NOT DEFINED MDVIEW_BUILD_TYPE_DEFAULTED)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    endif()
    set(MDVIEW_BUILD_TYPE_DEFAULTED ON CACHE INTERNAL "mdview: build type default applied")
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
    message(STATUS "mdview: single-configuration build type is ${CMAKE_BUILD_TYPE}")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# windows.h is used in exactly one file; keep it lean and keep min/max macros
# out of the standard headers.
if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(MSVC)
    # We use the portable C and C++ APIs on purpose -- std::getenv rather than
    # _dupenv_s, std::fopen rather than fopen_s -- so the same source builds
    # with GCC and Clang.  MSVC's deprecation warnings for those are noise.
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# ---------------------------------------------------------------------------
# Warning level, applied to every target we build.
# ---------------------------------------------------------------------------
function(mdview_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-          # no MSVC-specific laxity: keep the code portable
            /utf-8                # source and execution character set are UTF-8
            /Zc:__cplusplus       # correct __cplusplus under /std:c++20
            /EHsc)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Portable core: every source file here builds unchanged on all three
# platforms.  No platform headers, no #ifdef _WIN32.
# ---------------------------------------------------------------------------
add_library(mdview_core STATIC
    src/app/application.cpp
    src/app/layout.cpp
    src/cli/options.cpp
    src/filesystem/file_io.cpp
    src/filesystem/file_tree.cpp
    src/filesystem/polling_file_watcher.cpp
    src/markdown/highlight.cpp
    src/markdown/parser.cpp
    src/markdown/renderer.cpp
    src/search/search.cpp
    src/terminal/input.cpp
    src/terminal/screen.cpp
    src/terminal/style.cpp
    src/terminal/terminal.cpp
    src/ui/chrome.cpp
    src/ui/markdown_view.cpp
    src/ui/tree_view.cpp
    src/utils/string_utils.cpp
    src/utils/unicode.cpp
)

target_include_directories(mdview_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(mdview_core PUBLIC
    MDVIEW_VERSION="${PROJECT_VERSION}"
    MDVIEW_AUTHOR="${MDVIEW_AUTHOR}"
    MDVIEW_HOMEPAGE="${MDVIEW_HOMEPAGE}"
    MDVIEW_REPOSITORY="${MDVIEW_REPOSITORY}"
    MDVIEW_COPYRIGHT="${MDVIEW_COPYRIGHT}"
    MDVIEW_LICENSE="${MDVIEW_LICENSE}"
    MDVIEW_YEAR="2026"
)
mdview_enable_warnings(mdview_core)

# ---------------------------------------------------------------------------
# Platform layer.
#
# The terminal is the only genuinely OS-specific subsystem: Windows uses the
# Win32 console API, Linux and macOS share one termios/poll implementation
# (src/platform/posix) because their interfaces are identical.  Each platform
# also owns its identity and the FileWatcher factory.
# ---------------------------------------------------------------------------
if(WIN32)
    target_sources(mdview_core PRIVATE
        src/platform/windows/platform_windows.cpp
        src/platform/windows/terminal_windows.cpp
    )
elseif(APPLE)
    target_sources(mdview_core PRIVATE
        src/platform/macos/platform_macos.cpp
        src/platform/posix/terminal_posix.cpp
    )
elseif(UNIX)
    target_sources(mdview_core PRIVATE
        src/platform/linux/platform_linux.cpp
        src/platform/posix/terminal_posix.cpp
    )
else()
    message(FATAL_ERROR
        "mdview targets Windows (MSVC), Linux (GCC/Clang) and macOS (Clang).")
endif()

# ---------------------------------------------------------------------------
# The executable.  With the Visual Studio generator this lands in
# build/Release/mdview.exe; with a single-config generator, in build/mdview.
# ---------------------------------------------------------------------------
add_executable(mdview src/main.cpp)
target_link_libraries(mdview PRIVATE mdview_core)

# Windows resource file with product metadata
if(WIN32)
    set(MDVIEW_RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/mdview.rc")
    if(EXISTS "${MDVIEW_RC_FILE}")
        target_sources(mdview PRIVATE "${MDVIEW_RC_FILE}")
    endif()
endif()

mdview_enable_warnings(mdview)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(MDVIEW_BUILD_TESTS "Build the mdview test suite" ON)
if(MDVIEW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
