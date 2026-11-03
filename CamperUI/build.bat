@echo off
setlocal enabledelayedexpansion
echo ===========================================
echo  CamperUI - Kompilieren ^& Exportieren (.bin)
echo ===========================================

:: Version aus ui_main.h extrahieren
for /f "tokens=3" %%v in ('findstr /R /C:"#define CAMPERUI_VERSION" ui_main.h') do set VERSION=%%~v

if "%VERSION%"=="" (
    set VERSION=v1.0.0
)

echo Aktuelle Version: %VERSION%
echo.

set "ARDUINO_CLI=C:\Users\njest\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"

if not exist "%ARDUINO_CLI%" (
    echo Arduino CLI nicht gefunden unter: %ARDUINO_CLI%
    pause
    exit /b 1
)

echo Kompiliere CamperUI (%VERSION%)...
"%ARDUINO_CLI%" compile --output-dir ./build -b esp32:esp32:esp32s3:CPUFreq=240,CDCOnBoot=cdc,PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB CamperUI.ino

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo FEHLER beim Kompilieren!
    pause
    exit /b %ERRORLEVEL%
)

:: Kopiere versionierte Dateien
copy /Y ".\build\CamperUI.ino.bin" ".\build\CamperUI-%VERSION%.bin" >nul
copy /Y ".\build\CamperUI.ino.bin" ".\build\CamperUI.bin" >nul
copy /Y ".\build\CamperUI.ino.bootloader.bin" ".\build\bootloader-%VERSION%.bin" >nul
copy /Y ".\build\CamperUI.ino.partitions.bin" ".\build\partitions-%VERSION%.bin" >nul

if exist ".\build\CamperUI.ino.merged.bin" (
    copy /Y ".\build\CamperUI.ino.merged.bin" ".\build\CamperUI-%VERSION%-merged.bin" >nul
    copy /Y ".\build\CamperUI.ino.merged.bin" ".\build\CamperUI-merged.bin" >nul
    if not exist ".\docs\firmware" mkdir ".\docs\firmware"
    copy /Y ".\build\CamperUI.ino.merged.bin" ".\docs\firmware\CamperUI-%VERSION%-merged.bin" >nul
    copy /Y ".\build\CamperUI.ino.merged.bin" ".\docs\firmware\CamperUI-merged.bin" >nul
)

echo.
echo ===========================================
echo  Erfolgreich exportiert nach .\build\
echo  - .\build\CamperUI-%VERSION%.bin (fuer GitHub OTA Release)
echo  - .\build\CamperUI-%VERSION%-merged.bin (fuer Web-Flasher)
echo ===========================================
pause

