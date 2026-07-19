@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0.."
call "%~dp0env.bat"
if errorlevel 1 exit /b 1

set "DEPLOYED=0"

for %%D in (
  "%PROJECT_ROOT%\build"
  "%PROJECT_ROOT%\build\release"
  "%PROJECT_ROOT%\build\Desktop_Qt_6_11_1_MinGW_64_bit_Debug"
) do (
  if exist "%%~D\Mosaic.exe" (
    echo.
    echo === Deploy Qt runtime ===
    echo Target: %%~D
    pushd "%%~D"
    windeployqt --qmldir "%PROJECT_ROOT%\qml" --no-translations Mosaic.exe
    if errorlevel 1 (
      popd
      exit /b 1
    )
    for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
      if exist "%MINGW_DIR%\bin\%%F" (
        copy /Y "%MINGW_DIR%\bin\%%F" "%%~D\%%F" >nul
      )
    )
    popd
    set /a DEPLOYED+=1
    echo Deploy OK: %%~D
  )
)

if "%DEPLOYED%"=="0" (
  echo ERROR: Mosaic.exe not found. Build first.
  exit /b 1
)

echo.
echo Deployed %DEPLOYED% build folder(s).
endlocal
