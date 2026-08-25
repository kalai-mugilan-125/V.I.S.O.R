@echo off
setlocal enabledelayedexpansion

title V.I.S.O.R. Application Launcher

echo ==========================================================
echo          V.I.S.O.R. One-Click Automated Launcher
echo ==========================================================
echo.
echo Launching automated dependency checker and application runner...
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0start.ps1"

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Application exited with error code %ERRORLEVEL%.
    pause
)
