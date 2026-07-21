@echo off
setlocal
cd /d "%~dp0"
call scripts\env.bat
if errorlevel 1 exit /b 1

if not exist build mkdir build
cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=%QT_DIR% ..
if errorlevel 1 exit /b 1
cmake --build . --parallel
if errorlevel 1 exit /b 1
cd ..
call scripts\deploy-dev.bat
if errorlevel 1 exit /b 1
echo.
echo Build OK: build\MaskStudio.exe
echo Run: run.bat   Diag: scripts\diag.bat
echo If using Qt Creator, rebuild there then run scripts\deploy-dev.bat
endlocal
