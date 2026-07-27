@echo off
setlocal EnableDelayedExpansion

set "PROJECT_ROOT=%~dp0.."
for %%I in ("%PROJECT_ROOT%") do set "PROJECT_ROOT=%%~fI"

set "APP_VERSION=0.1.0"
if exist "%PROJECT_ROOT%\APP_VERSION.txt" (
    for /f "usebackq delims=" %%V in ("%PROJECT_ROOT%\APP_VERSION.txt") do set "APP_VERSION=%%V"
)

if not defined QT_DIR (
    if exist "D:\Qt\6.11.1\mingw_64\bin\qmake.exe" (
        set "QT_DIR=D:\Qt\6.11.1\mingw_64"
    ) else if exist "C:\Qt\6.11.1\mingw_64\bin\qmake.exe" (
        set "QT_DIR=C:\Qt\6.11.1\mingw_64"
    )
)

if not defined MINGW_DIR (
    if exist "D:\Qt\Tools\mingw1310_64\bin\g++.exe" (
        set "MINGW_DIR=D:\Qt\Tools\mingw1310_64"
    ) else if exist "C:\Qt\Tools\mingw1310_64\bin\g++.exe" (
        set "MINGW_DIR=C:\Qt\Tools\mingw1310_64"
    )
)

if not defined CMAKE_DIR (
    if exist "D:\Qt\Tools\CMake_64\bin\cmake.exe" (
        set "CMAKE_DIR=D:\Qt\Tools\CMake_64\bin"
    ) else if exist "C:\Qt\Tools\CMake_64\bin\cmake.exe" (
        set "CMAKE_DIR=C:\Qt\Tools\CMake_64\bin"
    )
)

if not defined NINJA_DIR (
    if exist "D:\Qt\Tools\Ninja\ninja.exe" (
        set "NINJA_DIR=D:\Qt\Tools\Ninja"
    ) else if exist "C:\Qt\Tools\Ninja\ninja.exe" (
        set "NINJA_DIR=C:\Qt\Tools\Ninja"
    )
)

if not defined INNO_SETUP_DIR (
    if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" (
        set "INNO_SETUP_DIR=%LOCALAPPDATA%\Programs\Inno Setup 6"
    )
)
if not defined INNO_SETUP_DIR (
    if exist "D:\Inno Setup 6\ISCC.exe" (
        set "INNO_SETUP_DIR=D:\Inno Setup 6"
    )
)

if not defined QT_DIR (
    echo [env.bat] ERROR: Qt not found. Set QT_DIR.
    exit /b 1
)

if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo [env.bat] ERROR: windeployqt not found in %QT_DIR%\bin
    exit /b 1
)

set "PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%CMAKE_DIR%;%NINJA_DIR%;%PATH%"

rem Prefer Release output for packaging; fall back to flat build, then Qt Creator Debug.
set "BUILD_DIR=%PROJECT_ROOT%\build\release"
set "BUILD_EXE=%BUILD_DIR%\Mosaic.exe"
if not exist "%BUILD_EXE%" (
    set "BUILD_DIR=%PROJECT_ROOT%\build"
    set "BUILD_EXE=%BUILD_DIR%\Mosaic.exe"
)
if not exist "%BUILD_EXE%" if exist "%PROJECT_ROOT%\build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug\Mosaic.exe" (
    set "BUILD_DIR=%PROJECT_ROOT%\build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug"
    set "BUILD_EXE=%BUILD_DIR%\Mosaic.exe"
)
if not exist "%BUILD_EXE%" if exist "%BUILD_DIR%\MaskStudio.exe" (
    set "BUILD_EXE=%BUILD_DIR%\MaskStudio.exe"
)
set "DIST_ROOT=%PROJECT_ROOT%\dist"
set "DIST_DIR=%DIST_ROOT%\Mosaic_%APP_VERSION%_win64"
set "ARTIFACT_DIR=%DIST_ROOT%\artifacts"

endlocal & (
    set "PROJECT_ROOT=%PROJECT_ROOT%"
    set "APP_VERSION=%APP_VERSION%"
    set "QT_DIR=%QT_DIR%"
    set "MINGW_DIR=%MINGW_DIR%"
    set "CMAKE_DIR=%CMAKE_DIR%"
    set "NINJA_DIR=%NINJA_DIR%"
    set "INNO_SETUP_DIR=%INNO_SETUP_DIR%"
    set "BUILD_DIR=%BUILD_DIR%"
    set "BUILD_EXE=%BUILD_EXE%"
    set "DIST_ROOT=%DIST_ROOT%"
    set "DIST_DIR=%DIST_DIR%"
    set "ARTIFACT_DIR=%ARTIFACT_DIR%"
    set "PATH=%PATH%"
)
