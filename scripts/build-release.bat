@echo off
setlocal
cd /d "%~dp0.."
call "%~dp0env.bat"
if errorlevel 1 exit /b 1

echo.
echo === Mosaic Release Build (v%APP_VERSION%) ===
echo Qt: %QT_DIR%
echo Output: %BUILD_EXE%
echo.

python "%~dp0generate-app-icon.py"
if errorlevel 1 (
    echo WARNING: app-icon generation failed; using existing resources\app-icon.* if present.
)

if not exist "%PROJECT_ROOT%\build\release" mkdir "%PROJECT_ROOT%\build\release"

cmake -S "%PROJECT_ROOT%" -B "%PROJECT_ROOT%\build\release" -G "Ninja" ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
    -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build "%PROJECT_ROOT%\build\release" --parallel
if errorlevel 1 exit /b 1

if not exist "%PROJECT_ROOT%\build\release\Mosaic.exe" (
    echo ERROR: Build finished but Mosaic.exe was not created.
    exit /b 1
)

echo.
echo Build OK: %PROJECT_ROOT%\build\release\Mosaic.exe
endlocal
