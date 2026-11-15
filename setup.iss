[Setup]
AppName=ExplorerStatus
AppVersion=1.0.0
DefaultDirName={pf}\ExplorerStatus
DefaultGroupName=ExplorerStatus
PrivilegesRequired=admin
OutputDir=build
OutputBaseFilename=ExplorerStatus_Setup
Compression=lzma
SolidCompression=yes

[Files]
; 1. Holt die frisch kompilierte DLL aus deinem build-Ordner
Source: "build\ExplorerStatus.dll"; DestDir: "{app}"; Flags: ignoreversion

; 2. Kopiert den gesamten icons-Ordner samt aller 8 .ico-Dateien in den Zielordner
Source: "icons\*"; DestDir: "{app}\icons"; Flags: ignoreversion recursesubdirs createallsubdirs

[Run]
; Registriert die DLL nach der Installation automatisch im System
Filename: "regsvr32.exe"; Parameters: "/s ""{app}\ExplorerStatus.dll"""; StatusMsg: "Registriere ExplorerStatus Shell-Erweiterung..."; Flags: runhidden

[UninstallRun]
; Trägt die DLL bei der Deinstallation automatisch aus der Registry aus
Filename: "regsvr32.exe"; Parameters: "/u /s ""{app}\ExplorerStatus.dll"""; StatusMsg: "Deregistiere ExplorerStatus Shell-Erweiterung..."; Flags: runhidden