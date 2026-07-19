@echo off
setlocal
echo Mosaic - Tesseract OCR setup
echo.
echo OCR is required for scanned images (PNG/JPEG) and improves bbox accuracy on PDF pages.
echo Language pack: chi_sim + eng (Chinese + English mixed documents)
echo.

set "DEST=%~dp0tools\tesseract"
set "TESSDATA=%DEST%\tessdata"

if exist "%DEST%\tesseract.exe" if exist "%TESSDATA%\chi_sim.traineddata" if exist "%TESSDATA%\eng.traineddata" (
  echo OK: bundled OCR already ready.
  echo   %DEST%
  "%DEST%\tesseract.exe" --version
  exit /b 0
)

if exist "%DEST%\tesseract.exe" (
  echo Found tesseract.exe but tessdata may be incomplete.
  goto :verify
)

echo Option A - Copy from old machine (fastest if you migrated the project folder):
echo   Copy the whole folder: tools\tesseract\  (exe + dll + tessdata\chi_sim + eng)
echo.
echo Option B - Install system-wide, then re-run this script:
echo   winget install UB-Mannheim.TesseractOCR
echo   Select "Additional language: Chinese Simplified" during install
echo.
echo Option C - Manual download:
echo   1. Installer: https://github.com/UB-Mannheim/tesseract/wiki
echo   2. chi_sim.traineddata: https://github.com/tesseract-ocr/tessdata/raw/main/chi_sim.traineddata
echo      Save to: %TESSDATA%\
echo.

set "SYS=C:\Program Files\Tesseract-OCR"
if not exist "%SYS%\tesseract.exe" (
  echo System Tesseract not found at "%SYS%".
  echo Install Tesseract first, then run this script again.
  exit /b 1
)

if not exist "%DEST%" mkdir "%DEST%"
if not exist "%TESSDATA%" mkdir "%TESSDATA%"

copy /Y "%SYS%\tesseract.exe" "%DEST%\" >nul
for %%f in (leptonica-*.dll libtesseract-*.dll libgcc_s_seh-*.dll libstdc++-*.dll libwinpthread-*.dll) do (
  if exist "%SYS%\%%f" copy /Y "%SYS%\%%f" "%DEST%\" >nul
)
xcopy /Y /E /I "%SYS%\tessdata\chi_sim.traineddata" "%TESSDATA%\" >nul 2>&1
xcopy /Y /E /I "%SYS%\tessdata\eng.traineddata" "%TESSDATA%\" >nul 2>&1
copy /Y "%SYS%\tessdata\chi_sim.traineddata" "%TESSDATA%\" >nul 2>&1
copy /Y "%SYS%\tessdata\eng.traineddata" "%TESSDATA%\" >nul 2>&1

:verify
if exist "%TESSDATA%\chi_sim.traineddata" if exist "%TESSDATA%\eng.traineddata" (
  echo OK: chi_sim + eng tessdata ready.
  echo Path: %DEST%
  if exist "%DEST%\tesseract.exe" "%DEST%\tesseract.exe" --version
  exit /b 0
)

echo ERROR: Missing tessdata. Ensure chi_sim.traineddata and eng.traineddata exist in:
echo   %TESSDATA%
echo.
echo Quick fix - download chi_sim only (if you have eng from winget):
echo   curl -L -o "%TESSDATA%\chi_sim.traineddata" ^
echo     https://github.com/tesseract-ocr/tessdata/raw/main/chi_sim.traineddata
exit /b 1
