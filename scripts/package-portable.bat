@echo off
setlocal
cd /d "%~dp0.."
call "%~dp0env.bat"
if errorlevel 1 exit /b 1

if not exist "%DIST_DIR%\Mosaic.exe" (
    echo ERROR: %DIST_DIR% not found. Run scripts\deploy.bat first.
    exit /b 1
)

if not exist "%ARTIFACT_DIR%" mkdir "%ARTIFACT_DIR%"

set "ZIP_NAME=Mosaic_%APP_VERSION%_win64_portable.zip"
set "ZIP_PATH=%ARTIFACT_DIR%\%ZIP_NAME%"

if exist "%ZIP_PATH%" del /f /q "%ZIP_PATH%"

echo.
echo === Create portable ZIP ===
echo %ZIP_PATH%
echo.

powershell -NoProfile -Command "Compress-Archive -Path '%DIST_DIR%\*' -DestinationPath '%ZIP_PATH%' -Force"
if errorlevel 1 exit /b 1

echo.
echo Portable package OK: %ZIP_PATH%
endlocal
