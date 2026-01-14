@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dev.ps1" %*
if errorlevel 1 (
  echo.
  echo [XBrowser] dev failed with exit code %errorlevel%.
  pause
)

endlocal
