@echo off
setlocal
title CamperUI - VanPi Tailscale Gateway
color 0B

echo ====================================================================
echo        CamperUI -- VanPi Tailscale Gateway fuer Windows
echo ====================================================================
echo.

:: 1. Suche nach Python (python, py oder AppData)
set "PYTHON_CMD="

where python >nul 2>&1
if not errorlevel 1 set "PYTHON_CMD=python"

if "%PYTHON_CMD%"=="" (
    where py >nul 2>&1
    if not errorlevel 1 set "PYTHON_CMD=py"
)

if "%PYTHON_CMD%"=="" (
    if exist "%LOCALAPPDATA%\Programs\Python\Python311\python.exe" (
        set "PYTHON_CMD=%LOCALAPPDATA%\Programs\Python\Python311\python.exe"
    )
)

if "%PYTHON_CMD%"=="" (
    color 0C
    echo [FEHLER] Python wurde nicht gefunden!
    echo Bitte installiere Python 3 und aktiviere Add to PATH.
    echo Download: https://www.python.org/
    echo.
    pause
    exit /b 1
)

echo [OK] Python gefunden: %PYTHON_CMD%
echo.

:: 2. Pruefe Tailscale
where tailscale >nul 2>&1
if not errorlevel 1 (
    echo [TAILSCALE] Suche nach VanPi / pekaway...
    tailscale status 2>nul | findstr /i "pekaway vanpi" >nul 2>&1
    if not errorlevel 1 (
        echo [TAILSCALE] VanPi-Geraet in Tailscale gefunden!
    ) else (
        echo [TAILSCALE] Hinweis: VanPi-Geraet ist in Tailscale aktuell offline oder nicht gefunden.
    )
) else (
    echo [HINWEIS] Tailscale CLI nicht in PATH gefunden.
)

echo.
echo --------------------------------------------------------------------
echo [SCHRITT 1] Display mit demselben WLAN / PC-Hotspot verbinden.
echo [SCHRITT 2] Am Display: Einstellungen (Zahnrad) -^> Netzwerk.
echo [SCHRITT 3] Trage dort die im Gateway angezeigte PC-IP ein!
echo --------------------------------------------------------------------
echo.
echo Starte Web-Dashboard unter http://localhost:8088/ ...
start http://localhost:8088/

:: Starte Gateway Tool
"%PYTHON_CMD%" "%~dp0vanpi_gateway.py" %*

if errorlevel 1 (
    echo.
    echo [FEHLER] Das Gateway wurde mit einem Fehler beendet.
    pause
)
