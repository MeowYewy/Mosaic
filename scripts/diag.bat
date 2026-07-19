@echo off
setlocal
cd /d "%~dp0.."
call scripts\env.bat
if errorlevel 1 exit /b 1
if not exist "%BUILD_EXE%" (
  echo Build first: build.bat
  exit /b 1
)
echo Running pipeline self-test on: %BUILD_EXE%
"%BUILD_EXE%" --diag
exit /b %ERRORLEVEL%
