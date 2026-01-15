@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package.ps1" %*
if errorlevel 1 (
  echo.
  echo [XBrowser] package failed with exit code %errorlevel%.
  pause
)

endlocal
