@echo off
setlocal
cd /d "%~dp0"
call scripts\env.bat
if errorlevel 1 exit /b 1

set "RUNDIR=build"
if not exist "build\Mosaic.exe" (
  if exist "build\release\Mosaic.exe" (
    set "RUNDIR=build\release"
  ) else (
    echo Mosaic.exe not found. Run build.bat first.
    exit /b 1
  )
)

if not exist "%RUNDIR%\platforms\qwindows.dll" (
  echo Qt plugins missing — running deploy-dev.bat ...
  call scripts\deploy-dev.bat
  if errorlevel 1 exit /b 1
)

echo Starting: %RUNDIR%\Mosaic.exe
start "" "%RUNDIR%\Mosaic.exe"
endlocal
