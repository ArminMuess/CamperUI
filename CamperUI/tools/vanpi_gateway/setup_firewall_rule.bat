@echo off
setlocal
title CamperUI Gateway - Windows Firewall Freigabe
color 0E

echo ====================================================================
echo   CamperUI Gateway: Windows Firewall Port 1880 Freigabe
echo ====================================================================
echo.
echo Dieser Vorgang schaltet Port 1880 (TCP) in der Windows Defender Firewall
echo frei, damit das ESP32-Display ueber WLAN auf das Gateway zugreifen kann.
echo.
echo Benoetigt Administrator-Rechte!
echo.
pause

net session >nul 2>&1
if errorlevel 1 (
    color 0C
    echo.
    echo [FEHLER] Bitte fuehre diese Batch-Datei als ADMINISTRATOR aus!
    echo (Rechtsklick auf setup_firewall_rule.bat -^> 'Als Administrator ausfuehren')
    echo.
    pause
    exit /b 1
)

echo Freigabe wird eingerichtet...
netsh advfirewall firewall delete rule name="CamperUI Gateway (Port 1880)" >nul 2>&1
netsh advfirewall firewall add rule name="CamperUI Gateway (Port 1880)" dir=in action=allow protocol=TCP localport=1880 profile=private,public

if not errorlevel 1 (
    color 0A
    echo.
    echo [ERFOLG] Port 1880 wurde erfolgreich in der Windows Firewall freigegeben!
    echo Das CamperUI-Display kann sich nun problemlos verbinden.
) else (
    color 0C
    echo [FEHLER] Die Firewall-Regel konnte nicht erstellt werden.
)

echo.
pause
