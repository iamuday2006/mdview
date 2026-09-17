@echo off
rem ---------------------------------------------------------------------------
rem  mdview - build the Inno Setup installer
rem
rem  Prerequisites:
rem    1. mdview must be built first: scripts\build.bat
rem    2. Inno Setup 6.3+ must be installed and ISCC.exe in PATH,
rem       or set ISCC below to the full path of ISCC.exe
rem ---------------------------------------------------------------------------
setlocal

set "ROOT=%~dp0.."
set "ISCC=ISCC.exe"
set "ISS=%ROOT%\installer\mdview.iss"

rem --- check that the exe exists ---
if not exist "%ROOT%\build\Release\mdview.exe" (
    echo [mdview] mdview.exe not found. Run scripts\build.bat first.
    exit /b 1
)

rem --- check that ISCC is available ---
where "%ISCC%" >NUL 2>&1
if errorlevel 1 (
    echo [mdview] ISCC.exe not found in PATH.
    echo [mdview] Install Inno Setup 6 from https://jrsoftware.org/isdl.php
    echo [mdview] Or set ISCC in this script to the full path of ISCC.exe
    exit /b 1
)

echo [mdview] Building installer...
"%ISCC%" "%ISS%"
if errorlevel 1 (
    echo [mdview] Installer build failed.
    exit /b 1
)

echo.
echo [mdview] Installer created:
echo         %ROOT%\installer\output\mdview-0.1.0-setup.exe
echo.

endlocal
exit /b 0
