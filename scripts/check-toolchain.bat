@echo off
rem ---------------------------------------------------------------------------
rem  mdview - report which compilers and generators this machine can offer.
rem
rem  Nothing here is hard-coded to one Visual Studio version: the install is
rem  located through vswhere, and the generator list comes from CMake itself.
rem  Useful as the first thing to run when a build fails on a new machine.
rem ---------------------------------------------------------------------------
setlocal enabledelayedexpansion
set "ROOT=%~dp0.."
pushd "%ROOT%" || exit /b 1

rem --- locate Visual Studio -------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
set "VCVARS="

if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
)
if defined VSPATH if exist "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=!VSPATH!\VC\Auxiliary\Build\vcvars64.bat"

echo === VISUAL STUDIO ===
if not defined VSPATH (
    echo   not found - install the "Desktop development with C++" workload
    popd & endlocal & exit /b 1
)
echo   !VSPATH!
if not defined VCVARS (
    echo   vcvars64.bat is missing from that install
    popd & endlocal & exit /b 1
)

rem --- load the environment first ------------------------------------------
rem vcvars64.bat is what puts cl.exe - and the CMake that ships with Visual
rem Studio - on PATH, so everything below depends on it having run.
call "!VCVARS!" 1>NUL 2>&1
if errorlevel 1 (
    echo   failed to initialise the MSVC environment
    popd & endlocal & exit /b 1
)

echo.
echo === WHERE CMAKE / NINJA ===
where cmake 2>NUL || echo   cmake not on PATH
where ninja 2>NUL || echo   ninja not on PATH ^(fine unless you build with -G Ninja^)

echo.
echo === VISUAL STUDIO GENERATORS CMake KNOWS ===
rem CMake flags the generator it would pick by default with a leading "*".
cmake --help 2>NUL | findstr /R /C:"Visual Studio 1[0-9]"
echo   ^(* marks the default; scripts\build.bat uses that when -G is omitted^)

echo.
echo === MSVC TOOLCHAIN ===
for /f "delims=" %%v in ('cl 2^>^&1 ^| findstr /C:"Version"') do echo   %%v
where link 2>NUL || echo   link.exe not on PATH
rem Git for Windows ships its own link.exe; if it ever precedes the MSVC one on
rem PATH, linking fails in confusing ways.  Worth flagging loudly.
set "LINK_COUNT=0"
for /f "delims=" %%l in ('where link 2^>NUL') do set /a LINK_COUNT+=1
if !LINK_COUNT! GTR 1 echo   note: !LINK_COUNT! link.exe on PATH - the MSVC one must come first

echo.
echo === DONE ===
popd
endlocal
exit /b 0
