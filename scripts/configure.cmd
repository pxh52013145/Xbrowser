@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0configure.ps1" %*
if errorlevel 1 (
  echo.
  echo [XBrowser] configure failed with exit code %errorlevel%.
  pause
)

endlocal

