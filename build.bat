@echo off
setlocal EnableExtensions

echo ============================================
echo ExplorerStatus - Visual Studio 2026 Build
echo ============================================
echo.

set "VSROOT=D:\Programme\Microsoft Visual Studio\18\Insiders"
set "VSCMD=%VSROOT%\Common7\Tools\VsDevCmd.bat"

if not exist "%VSCMD%" (
  echo FEHLER: Visual Studio 2026 Insiders wurde nicht gefunden.
  echo %VSCMD%
  exit /b 1
)

echo Visual Studio gefunden:
echo %VSCMD%
call "%VSCMD%" -arch=x64
if errorlevel 1 exit /b 1

if not exist build mkdir build
if not exist build\data mkdir build\data

echo.
echo [1/2] Kompiliere Ressourcen...
rc.exe /nologo /fo build\ExplorerStatus.res src\ExplorerStatus.rc
if errorlevel 1 (
  echo FEHLER: Ressourcen-Kompilierung fehlgeschlagen.
  exit /b 1
)

echo.
echo [2/2] Kompiliere ExplorerStatus.dll...
cl.exe /nologo /std:c++17 /EHsc /W4 /LD /DUNICODE /D_UNICODE ^
 src\ExplorerStatus.cpp build\ExplorerStatus.res ^
 /link /DEF:src\ExplorerStatus.def /OUT:build\ExplorerStatus.dll ^
 /IMPLIB:build\ExplorerStatus.lib shell32.lib shlwapi.lib ole32.lib advapi32.lib user32.lib

if errorlevel 1 (
echo.
echo ============================================
echo BUILD FEHLGESCHLAGEN
echo ============================================
 exit /b 1
)

echo.
echo ============================================
echo BUILD ERFOLGREICH
echo ============================================
echo DLL: %CD%\build\ExplorerStatus.dll
endlocal
