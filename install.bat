@echo off
setlocal EnableExtensions

title ExplorerStatus - Installation

echo ============================================
echo ExplorerStatus - Installation
echo ============================================
echo.

REM ------------------------------------------------
REM Administratorrechte sicherstellen
REM ------------------------------------------------
net session >nul 2>&1
if not "%errorlevel%"=="0" (
    echo Administratorrechte werden benoetigt.
    echo Starte die Installation erneut als Administrator...
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%ComSpec%' -ArgumentList '/c ""%~f0""' -Verb RunAs"
    exit /b 0
)

REM ------------------------------------------------
REM Projekt- und Buildpfad
REM ------------------------------------------------
set "SOURCE=%~dp0"
set "DLL=%SOURCE%build\ExplorerStatus.dll"

if not exist "%DLL%" (
    echo.
    echo FEHLER: ExplorerStatus.dll wurde nicht gefunden.
    echo.
    echo Erwarteter Pfad:
    echo %DLL%
    echo.
    echo Bitte zuerst build.bat ausfuehren.
    echo.
    pause
    exit /b 1
)

echo DLL gefunden:
echo %DLL%
echo.

REM ------------------------------------------------
REM Installationsverzeichnis
REM ------------------------------------------------
set "INSTALL=C:\Programme\ExplorerStatus"

echo Installationsverzeichnis:
echo %INSTALL%
echo.

REM ------------------------------------------------
REM Vorhandenen Installationsordner verwenden
REM ------------------------------------------------
if exist "%INSTALL%\" (
    echo Vorhandener Installationsordner wird verwendet.
) else (
    echo Installationsordner existiert nicht.
    echo Erstelle Installationsordner...
    mkdir "%INSTALL%"
    if errorlevel 1 (
        echo.
        echo FEHLER: Installationsverzeichnis konnte nicht erstellt werden.
        echo.
        pause
        exit /b 1
    )
)

REM ------------------------------------------------
REM Unterverzeichnisse
REM ------------------------------------------------
if not exist "%INSTALL%\data\" (
    mkdir "%INSTALL%\data"
    if errorlevel 1 (
        echo.
        echo FEHLER: Datenverzeichnis konnte nicht erstellt werden.
        echo.
        pause
        exit /b 1
    )
)

if not exist "%INSTALL%\icons\" (
    mkdir "%INSTALL%\icons"
    if errorlevel 1 (
        echo.
        echo FEHLER: Icon-Verzeichnis konnte nicht erstellt werden.
        echo.
        pause
        exit /b 1
    )
)

echo.
echo Verzeichnisstruktur OK.

REM ------------------------------------------------
REM Schreibtest
REM ------------------------------------------------
echo Pruefe Schreibrechte...

set "TESTFILE=%INSTALL%\data\.__explorerstatus_write_test.tmp"

echo test> "%TESTFILE%" 2>nul

if not exist "%TESTFILE%" (
    echo.
    echo FEHLER: Installationsverzeichnis ist nicht beschreibbar.
    echo.
    echo Schreibtest fehlgeschlagen:
    echo %INSTALL%\data
    echo.
    pause
    exit /b 1
)

del /f /q "%TESTFILE%" >nul 2>&1

echo Schreibrechte OK.

REM ------------------------------------------------
REM Explorer beenden, damit die Shell-DLL
REM nicht gesperrt ist
REM ------------------------------------------------
echo.
echo Beende Windows Explorer...
taskkill /F /IM explorer.exe >nul 2>&1
timeout /t 2 /nobreak >nul

REM ------------------------------------------------
REM Alte DLL deregistrieren
REM ------------------------------------------------
if exist "%INSTALL%\ExplorerStatus.dll" (
    echo Entferne alte Registrierung...
    "%SystemRoot%\System32\regsvr32.exe" /u /s "%INSTALL%\ExplorerStatus.dll" >nul 2>&1
)

REM ------------------------------------------------
REM DLL installieren
REM ------------------------------------------------
echo.
echo Kopiere ExplorerStatus.dll...

copy /Y "%DLL%" "%INSTALL%\ExplorerStatus.dll" >nul

if errorlevel 1 (
    echo.
    echo FEHLER: ExplorerStatus.dll konnte nicht kopiert werden.
    echo.
    start "" explorer.exe
    pause
    exit /b 1
)

echo DLL erfolgreich kopiert.

REM ------------------------------------------------
REM Alle Status-Icons installieren
REM ------------------------------------------------
echo.
echo Kopiere Status-Icons...

REM Kontextmenue-Icons
if exist "%SOURCE%icons\menu_working.ico" (
    copy /Y "%SOURCE%icons\menu_working.ico" "%INSTALL%\icons\menu_working.ico" >nul
)

if exist "%SOURCE%icons\menu_done.ico" (
    copy /Y "%SOURCE%icons\menu_done.ico" "%INSTALL%\icons\menu_done.ico" >nul
)

if exist "%SOURCE%icons\menu_waiting.ico" (
    copy /Y "%SOURCE%icons\menu_waiting.ico" "%INSTALL%\icons\menu_waiting.ico" >nul
)

if exist "%SOURCE%icons\menu_problem.ico" (
    copy /Y "%SOURCE%icons\menu_problem.ico" "%INSTALL%\icons\menu_problem.ico" >nul
)

REM Kleine Overlay-Icons
if exist "%SOURCE%icons\working_overlay.ico" (
    copy /Y "%SOURCE%icons\working_overlay.ico" "%INSTALL%\icons\working_overlay.ico" >nul
)

if exist "%SOURCE%icons\done_overlay.ico" (
    copy /Y "%SOURCE%icons\done_overlay.ico" "%INSTALL%\icons\done_overlay.ico" >nul
)

if exist "%SOURCE%icons\waiting_overlay.ico" (
    copy /Y "%SOURCE%icons\waiting_overlay.ico" "%INSTALL%\icons\waiting_overlay.ico" >nul
)

if exist "%SOURCE%icons\problem_overlay.ico" (
    copy /Y "%SOURCE%icons\problem_overlay.ico" "%INSTALL%\icons\problem_overlay.ico" >nul
)

echo Status-Icons installiert.

REM ------------------------------------------------
REM Schreibrechte fuer Statusdaten
REM ------------------------------------------------
echo.
echo Setze Schreibrechte fuer Datenverzeichnis...

icacls "%INSTALL%\data" /grant "%USERNAME%":(OI)(CI)M /T >nul

if errorlevel 1 (
    echo WARNUNG: Schreibrechte konnten nicht automatisch gesetzt werden.
) else (
    echo Schreibrechte fuer Statusdaten gesetzt.
)

REM ------------------------------------------------
REM DLL registrieren
REM ------------------------------------------------
echo.
echo Registriere ExplorerStatus...

"%SystemRoot%\System32\regsvr32.exe" /s "%INSTALL%\ExplorerStatus.dll"

if errorlevel 1 (
    echo.
    echo FEHLER: DLL-Registrierung fehlgeschlagen.
    echo.
    start "" explorer.exe
    pause
    exit /b 1
)

echo DLL erfolgreich registriert.

REM ------------------------------------------------
REM Explorer aktualisieren
REM ------------------------------------------------
echo.
echo Starte Windows Explorer neu...

start "" explorer.exe

timeout /t 2 /nobreak >nul

echo.
echo ============================================
echo INSTALLATION ERFOLGREICH
echo ============================================
echo.
echo Installiert nach:
echo %INSTALL%
echo.
echo Statusdatei:
echo %INSTALL%\data\status.json
echo.
echo Im Explorer:
echo Rechtsklick auf Datei oder Ordner
echo.
echo   ExplorerStatus
echo      In Bearbeitung
echo      Erledigt
echo      Wartet
echo      Problem
echo      Status entfernen
echo.
echo Hinweis: Unter Windows 11 kann die klassische
echo Shell-Erweiterung unter "Weitere Optionen anzeigen"
echo erscheinen.
echo.
pause

endlocal