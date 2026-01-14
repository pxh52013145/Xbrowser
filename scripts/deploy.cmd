@echo off
setlocal

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy.ps1" %*
if errorlevel 1 (
  echo.
  echo [XBrowser] deploy failed with exit code %errorlevel%.
  pause
)

endlocal
