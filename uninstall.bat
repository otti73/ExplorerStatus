@echo off
setlocal EnableExtensions

title ExplorerStatus - Deinstallation

echo ============================================
echo ExplorerStatus - Deinstallation
echo ============================================
echo.

net session >nul 2>&1
if not "%errorlevel%"=="0" (
    echo Administratorrechte werden benoetigt.
    echo Starte die Deinstallation erneut als Administrator...
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%ComSpec%' -ArgumentList '/c ""%~f0""' -Verb RunAs"
    exit /b 0
)

set "INSTALL=C:\Programme\ExplorerStatus"
set "DLL=%INSTALL%\ExplorerStatus.dll"

if exist "%DLL%" (
    echo Deregistriere ExplorerStatus...
    "%SystemRoot%\System32\regsvr32.exe" /u /s "%DLL%"
)

if exist "%DLL%" del /Q "%DLL%" >nul 2>&1

REM Statusdaten bewusst erhalten. Bei Bedarf data\status.json manuell loeschen.

echo.
echo Starte Windows Explorer neu...
taskkill /F /IM explorer.exe >nul 2>&1
timeout /t 2 /nobreak >nul
start "" explorer.exe

echo.
echo ============================================
echo DEINSTALLATION ABGESCHLOSSEN
echo ============================================
echo.
echo Die Statusdaten wurden nicht automatisch geloescht.
echo Sie liegen unter:
echo %INSTALL%\data\status.json
echo.
pause
endlocal
