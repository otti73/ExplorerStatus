# ExplorerStatus

[🇬🇧 English Version](README_EN.md)

**ExplorerStatus** ist eine native Windows-Shell-Erweiterung (C++ COM-DLL), mit der sich der Bearbeitungsstatus einzelner **Dateien und Ordner** direkt im Windows-Explorer visualisieren und verwalten lässt – komplett eigenständig ohne externe Laufzeitumgebungen und ohne die Dateien selbst zu verändern oder auf NTFS-Streams angewiesen zu sein.

## Architektur & Funktionsweise

Das Projekt arbeitet vollständig nativ unter Windows:

* **Native C++ Shell Extension (`ExplorerStatus.dll`):** Integriert das Kontextmenü (für Dateien und Verzeichnisse) sowie die vier Icon-Overlay-Handler direkt in den Windows-Explorer und liest/schreibt Statusänderungen eigenständig in nativem C++.
* **JSON-Datenbank (`data/status.json`):** Speichert alle zugewiesenen Pfade und Statuswerte atomar (über temporäre `.tmp`-Dateien) in einem Unterordner neben der DLL. Dadurch funktionieren die Statusmarkierungen dateisystemunabhängig auch auf Netzlaufwerken, exFAT/FAT32-Datenträgern und in synchronisierten Cloud-Ordnern.
* **Dynamische Icons (`icons/`):** Lädt sowohl die 16×16-Kontextmenü-Icons (`menu_*.ico`) als auch die Explorer-Overlay-Icons (`*_overlay.ico`) direkt aus dem beigefügten `icons/`-Verzeichnis.
* **Optionales CLI-Tool (`ExplorerStatus.py`):** Für Automatisierungen und Batch-Skripte liegt zusätzlich ein eigenständiges Python-Skript bei, das direkt auf dieselbe `data/status.json` zugreift (für den normalen Explorer-Betrieb ist kein Python erforderlich).

### Verfügbare Status-Kategorien
* **In Bearbeitung (`working`)** – Datei oder Ordner wird aktuell bearbeitet
* **Erledigt (`done`)** – Bearbeitung ist abgeschlossen
* **Wartet (`waiting`)** – Wartet auf Rückmeldung oder Freigabe
* **Problem (`problem`)** – Klärungsbedarf oder Fehler vorhanden
* **Status entfernen (`remove`)** – Entfernt den Eintrag aus der `status.json` und blendet das Overlay aus

---

## Installation (für Endanwender)

### Voraussetzungen
* Windows 10 oder Windows 11 (64-Bit) – **keine zusätzlichen Laufzeitumgebungen erforderlich**

### Einrichtung
1. Lade das aktuelle Release-Archiv aus dem Bereich **[Releases](../../releases)** herunter und entpacke es vollständig in einen festen Programmordner (z. B. `C:\Tools\ExplorerStatus\`).
   > **Wichtig:** Der Unterordner **`icons/`** mit allen 8 `.ico`-Dateien muss im selben Verzeichnis wie die `ExplorerStatus.dll` liegen.
2. Starte die **`install.bat`** per Rechtsklick als **Administrator**, um die `ExplorerStatus.dll` via `regsvr32` zu registrieren.
3. Starte den Windows-Explorer einmal neu (oder melde dich kurz ab und wieder an), damit Windows die Icon-Overlays und das Kontextmenü lädt.

*Deinstallation:* Führe die **`uninstall.bat`** als Administrator aus und starte anschließend den Windows-Explorer neu.

---

## Selbst kompilieren (Build from Source)

### Voraussetzungen
* Windows 10 / 11 (64-Bit)
* Visual Studio oder **Visual Studio Build Tools** mit C++-Desktopentwicklung (`cl.exe`, `rc.exe`, `link.exe`)

### Build-Vorgang
1. Repository klonen:
   ```bash
   git clone [https://github.com/otti73/ExplorerStatus.git](https://github.com/otti73/ExplorerStatus.git)
   cd ExplorerStatus