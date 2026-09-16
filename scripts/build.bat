@echo off
rem ---------------------------------------------------------------------------
rem  mdview - one-command Windows build using MSVC from the command line.
rem
rem    scripts\build.bat                     configure + build Release using the
rem                                          newest Visual Studio generator that
rem                                          CMake can actually find
rem    scripts\build.bat --ninja             use the Ninja generator (build-ninja\)
rem    scripts\build.bat --generator "X"     use a specific CMake generator
rem    scripts\build.bat --debug             build the Debug configuration
rem    scripts\build.bat --test              run the test suite afterwards
rem    scripts\build.bat --clean             delete the build directory first
rem
rem  No Visual Studio IDE and no MinGW are required: the script locates
rem  vcvars64.bat itself through vswhere, so `cl.exe` works from any shell.
rem
rem  The Visual Studio generator is never hard-coded.  Omitting -G makes CMake
rem  choose the newest Visual Studio it supports, and the script then reads the
rem  generator back out of CMakeCache.txt so you can see what was actually used.
rem ---------------------------------------------------------------------------
setlocal enabledelayedexpansion

set "ROOT=%~dp0.."
pushd "%ROOT%" || exit /b 1

set "BUILD_DIR=build"
set "BUILD_TYPE=Release"
set "GENERATOR="
set "WANT_TEST=0"
set "WANT_CLEAN=0"
set "PREFER_NINJA=0"

rem --- parse arguments ------------------------------------------------------
rem A shift-based loop rather than `for %%a in (%*)` so that --generator can
rem consume the argument that follows it.
:parse
if "%~1"=="" goto parsed
if /I "%~1"=="--ninja"     ( set "PREFER_NINJA=1" & set "BUILD_DIR=build-ninja" & shift & goto parse )
if /I "%~1"=="--generator" ( set "GENERATOR=%~2"  & shift & shift & goto parse )
if /I "%~1"=="--debug"     ( set "BUILD_TYPE=Debug" & shift & goto parse )
if /I "%~1"=="--test"      ( set "WANT_TEST=1" & shift & goto parse )
if /I "%~1"=="--clean"     ( set "WANT_CLEAN=1" & shift & goto parse )
if /I "%~1"=="--help"      goto usage
echo [mdview] Unknown option "%~1".
goto usage
:parsed

rem --- locate and load the MSVC environment --------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VCVARS="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
)
if defined VSPATH if exist "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=!VSPATH!\VC\Auxiliary\Build\vcvars64.bat"

if not defined VCVARS (
    echo [mdview] Could not find vcvars64.bat. Install the "Desktop development with C++"
    echo [mdview] workload, or run this script from a Developer Command Prompt.
    popd & exit /b 1
)

echo [mdview] Using "!VCVARS!"
call "!VCVARS!" 1>NUL 2>&1
if errorlevel 1 (
    echo [mdview] Failed to initialise the MSVC environment.
    popd & exit /b 1
)

for /f "delims=" %%v in ('cl 2^>^&1 ^| findstr /C:"Version"') do echo [mdview] %%v

rem --ninja is shorthand for --generator Ninja.  Collapsing it here means the
rem reconcile step below has exactly one notion of "the requested generator".
if "%PREFER_NINJA%"=="1" if not defined GENERATOR set "GENERATOR=Ninja"

set "GEN_FLAG="
if defined GENERATOR set "GEN_FLAG=-G "!GENERATOR!""
if defined GEN_FLAG echo [mdview] Generator: !GENERATOR!

rem --- reconcile an existing build directory with the requested generator ---
rem Configuring a directory with a different generator than the one already in
rem its cache is a hard CMake error.  Detecting that up front turns a confusing
rem failure into an automatic, explained clean.
if not exist "%BUILD_DIR%\CMakeCache.txt" goto generator-ok

set "CACHED_GEN="
for /f "usebackq tokens=1,* delims==" %%a in (`findstr /b /c:"CMAKE_GENERATOR:INTERNAL=" "%BUILD_DIR%\CMakeCache.txt"`) do set "CACHED_GEN=%%b"
if not defined CACHED_GEN goto generator-ok

set "GEN_MISMATCH=0"
if defined GENERATOR (
    if /I not "!CACHED_GEN!"=="!GENERATOR!" set "GEN_MISMATCH=1"
) else (
    rem No generator requested at all: CMake will pick the newest Visual Studio,
    rem so the existing cache only needs to hold a Visual Studio generator too.
    findstr /i /b /c:"CMAKE_GENERATOR:INTERNAL=Visual Studio" "%BUILD_DIR%\CMakeCache.txt" >NUL
    if errorlevel 1 set "GEN_MISMATCH=1"
)

if "!GEN_MISMATCH!"=="1" (
    echo [mdview] "!BUILD_DIR!" was configured with "!CACHED_GEN!".
    echo [mdview] Re-configuring it because that is not the requested generator.
    set "WANT_CLEAN=1"
)
:generator-ok

if "%WANT_CLEAN%"=="1" (
    if exist "!BUILD_DIR!" (
        echo [mdview] Removing "!BUILD_DIR!"...
        rem Windows may still hold a handle on a just-linked executable, so retry
        rem a few times.  Success is judged by the directory actually being gone:
        rem `rmdir /s /q` can report a failure errorlevel even when it worked.
        set "REMOVED=0"
        for /l %%n in (1,1,5) do (
            if "!REMOVED!"=="0" (
                rmdir /s /q "!BUILD_DIR!" 2>NUL
                if not exist "!BUILD_DIR!" set "REMOVED=1"
            )
        )
        if "!REMOVED!"=="0" (
            echo [mdview] Could not remove "!BUILD_DIR!". Close anything using it and retry.
            popd & exit /b 1
        )
    )
)

rem --- configure + build ---------------------------------------------------
echo [mdview] Configuring in "!BUILD_DIR!"...
cmake -S . -B "!BUILD_DIR!" !GEN_FLAG!
if errorlevel 1 (
    echo.
    echo [mdview] Configuration failed.  `cmake --help` lists every generator this
    echo [mdview] CMake knows about; the default is flagged with a leading "*".
    popd & exit /b 1
)

set "ACTIVE_GEN="
for /f "usebackq tokens=1,* delims==" %%a in (`findstr /b /c:"CMAKE_GENERATOR:INTERNAL=" "!BUILD_DIR!\CMakeCache.txt"`) do set "ACTIVE_GEN=%%b"
if defined ACTIVE_GEN echo [mdview] Configured with "!ACTIVE_GEN!"

echo [mdview] Building %BUILD_TYPE%...
cmake --build "!BUILD_DIR!" --config %BUILD_TYPE%
if errorlevel 1 ( popd & exit /b 1 )

echo.
echo [mdview] Executable:
if exist "!BUILD_DIR!\%BUILD_TYPE%\mdview.exe" echo         !BUILD_DIR!\%BUILD_TYPE%\mdview.exe
if exist "!BUILD_DIR!\mdview.exe" echo         !BUILD_DIR!\mdview.exe

rem --- tests ---------------------------------------------------------------
if "%WANT_TEST%"=="1" (
    echo.
    echo [mdview] Running tests...
    ctest --test-dir "!BUILD_DIR!" -C %BUILD_TYPE% --output-on-failure
    if errorlevel 1 ( popd & exit /b 1 )
)

popd
endlocal
exit /b 0

:usage
echo.
echo Usage: scripts\build.bat [--ninja] [--generator "NAME"] [--debug] [--test] [--clean]
echo.
echo   --ninja             use the Ninja generator and build-ninja\
echo   --generator "NAME"  use a specific CMake generator
echo   --debug             build the Debug configuration instead of Release
echo   --test              run the test suite after building
echo   --clean             delete the build directory before configuring
echo.
popd
endlocal
exit /b 2
