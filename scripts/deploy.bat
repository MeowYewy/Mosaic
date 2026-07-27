@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0.."
call "%~dp0env.bat"
if errorlevel 1 exit /b 1

if not exist "%BUILD_EXE%" (
    echo ERROR: %BUILD_EXE% not found. Run scripts\build-release.bat first.
    exit /b 1
)

echo.
echo === Deploy Mosaic v%APP_VERSION% ===
echo Source: %BUILD_EXE%
echo Staging: %DIST_DIR%
echo.

if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"

copy /Y "%BUILD_EXE%" "%DIST_DIR%\Mosaic.exe" >nul

set "WDEPLOY_FLAGS=--qmldir "%PROJECT_ROOT%\qml" --no-translations"
echo %BUILD_DIR% | findstr /I "Debug" >nul
if not errorlevel 1 set "WDEPLOY_FLAGS=--debug %WDEPLOY_FLAGS%"
echo %BUILD_DIR% | findstr /I "release" >nul
if not errorlevel 1 set "WDEPLOY_FLAGS=--release %WDEPLOY_FLAGS%"
if "%WDEPLOY_FLAGS%"=="--qmldir "%PROJECT_ROOT%\qml" --no-translations" set "WDEPLOY_FLAGS=--release %WDEPLOY_FLAGS%"

pushd "%DIST_DIR%"
windeployqt %WDEPLOY_FLAGS% Mosaic.exe
if errorlevel 1 (
    popd
    exit /b 1
)
popd

for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%MINGW_DIR%\bin\%%F" (
        copy /Y "%MINGW_DIR%\bin\%%F" "%DIST_DIR%\%%F" >nul
    ) else if exist "%QT_DIR%\bin\%%F" (
        copy /Y "%QT_DIR%\bin\%%F" "%DIST_DIR%\%%F" >nul
    )
)

if exist "%QT_DIR%\bin\opengl32sw.dll" (
    copy /Y "%QT_DIR%\bin\opengl32sw.dll" "%DIST_DIR%\opengl32sw.dll" >nul
)

if exist "%BUILD_DIR%\tools\tesseract\tesseract.exe" (
    xcopy /E /I /Y /Q "%BUILD_DIR%\tools\tesseract" "%DIST_DIR%\tools\tesseract" >nul
) else if exist "%PROJECT_ROOT%\tools\tesseract\tesseract.exe" (
    xcopy /E /I /Y /Q "%PROJECT_ROOT%\tools\tesseract" "%DIST_DIR%\tools\tesseract" >nul
) else (
    echo WARNING: tools\tesseract not found - OCR may fail on scans.
)

if exist "%BUILD_DIR%\tools\poppler\pdftoppm.exe" (
    xcopy /E /I /Y /Q "%BUILD_DIR%\tools\poppler" "%DIST_DIR%\tools\poppler" >nul
) else if exist "%PROJECT_ROOT%\tools\poppler\pdftoppm.exe" (
    xcopy /E /I /Y /Q "%PROJECT_ROOT%\tools\poppler" "%DIST_DIR%\tools\poppler" >nul
) else (
    echo WARNING: tools\poppler not found — run setup-poppler.bat then Rebuild, or PDF preview may stretch on rotated pages.
)

echo.
echo Deploy OK: %DIST_DIR%
echo Run: "%DIST_DIR%\Mosaic.exe"
endlocal
