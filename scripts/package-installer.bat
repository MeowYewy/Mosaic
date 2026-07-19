@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0.."
call "%~dp0env.bat"
if errorlevel 1 exit /b 1

if not exist "%DIST_DIR%\Mosaic.exe" (
    echo ERROR: %DIST_DIR% not found. Run scripts\deploy.bat first.
    exit /b 1
)

set "ISCC="
if defined INNO_SETUP_DIR (
    if exist "%INNO_SETUP_DIR%\ISCC.exe" set "ISCC=%INNO_SETUP_DIR%\ISCC.exe"
)
if not defined ISCC if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" (
    set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
)
if not defined ISCC if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
)

if not defined ISCC (
    echo ERROR: Inno Setup 6 ISCC.exe not found.
    echo Expected: %LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe
    exit /b 1
)

if not exist "%ARTIFACT_DIR%" mkdir "%ARTIFACT_DIR%"

echo.
echo === Build installer with Inno Setup ===
echo ISCC: %ISCC%
echo.

"%ISCC%" /DAppVersion=%APP_VERSION% /DSourceDir="%DIST_DIR%" /DOutputDir="%ARTIFACT_DIR%" "%PROJECT_ROOT%\packaging\windows\Mosaic.iss"
if errorlevel 1 exit /b 1

echo.
echo Installer OK: %ARTIFACT_DIR%\Mosaic_%APP_VERSION%_win64_Setup.exe
endlocal
