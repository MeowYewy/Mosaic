@echo off
setlocal
cd /d "%~dp0"

set DEST=tools\poppler
set ZIP=tools\poppler-temp.zip
set TEMP=tools\poppler-temp
set URL=https://github.com/oschwartz10612/poppler-windows/releases/download/v24.08.0-0/Release-24.08.0-0.zip

if exist "%DEST%\pdftoppm.exe" (
    echo Poppler already installed: %DEST%\pdftoppm.exe
    exit /b 0
)

if not exist "%DEST%" mkdir "%DEST%"

echo Downloading Poppler...
curl -L --connect-timeout 30 --max-time 600 -o "%ZIP%" "%URL%"
if errorlevel 1 (
    echo.
    echo Download failed. Try one of these:
    echo   1. Run this script again with VPN / better network
    echo   2. Manually download: %URL%
    echo      Extract Library\bin\* to %DEST%\
    echo   3. Install "Qt Pdf" via Qt Maintenance Tool, then Rebuild
    exit /b 1
)

echo Extracting...
powershell -NoProfile -Command "Expand-Archive -Path '%ZIP%' -DestinationPath '%TEMP%' -Force"
if errorlevel 1 exit /b 1

set BIN_DIR=
for /d %%D in ("%TEMP%\poppler-*") do set BIN_DIR=%%D\Library\bin
if not defined BIN_DIR if exist "%TEMP%\Library\bin\pdftoppm.exe" set BIN_DIR=%TEMP%\Library\bin

if not exist "%BIN_DIR%\pdftoppm.exe" (
    echo Cannot find pdftoppm.exe in extracted archive.
    exit /b 1
)

xcopy /Y /E /I "%BIN_DIR%\*" "%DEST%\"
del /Q "%ZIP%"
rmdir /S /Q "%TEMP%"

if exist "%DEST%\pdftoppm.exe" (
    echo OK: %DEST%\pdftoppm.exe
    echo Now Rebuild All in Qt Creator.
) else (
    echo Failed to install pdftoppm
    exit /b 1
)
