@echo off
setlocal
cd /d "%~dp0.."

echo.
echo === Clean local build artifacts (keep tools/ and .git) ===
echo.

if exist "build" rmdir /s /q "build"
if exist "build-test" rmdir /s /q "build-test"
if exist "dist" rmdir /s /q "dist"
if exist "%DIST%" rmdir /s /q "%DIST%"

echo Done. Rebuild with: release.bat
endlocal
