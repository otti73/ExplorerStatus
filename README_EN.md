# ExplorerStatus

[🇩🇪 Deutsche Version](README.md)

**ExplorerStatus** is a native Windows Shell Extension (C++ COM DLL) that visualizes and manages the processing status of individual **files and folders** directly inside Windows Explorer—completely standalone with zero external runtime dependencies, without modifying file contents or relying on NTFS Alternate Data Streams.

## Architecture & How It Works

The project runs 100% natively on Windows:

* **Native C++ Shell Extension (`ExplorerStatus.dll`):** Integrates the context menu (for both files and directories) and four icon overlay handlers directly into Windows Explorer, reading and writing status updates natively in C++.
* **JSON Database (`data/status.json`):** Stores all tracked paths and status values atomically (via temporary `.tmp` files) inside a subfolder next to the DLL. This keeps status markers intact across network shares, exFAT/FAT32 drives, and synced cloud folders.
* **Dynamic Icons (`icons/`):** Loads both the 16×16 context menu icons (`menu_*.ico`) and the Explorer overlay icons (`*_overlay.ico`) directly from the bundled `icons/` directory.
* **Optional CLI Utility (`ExplorerStatus.py`):** Includes a standalone Python script operating on the same `data/status.json` for automation and batch workflows (Python is **not** required for regular Windows Explorer usage).

### Available Status Categories
* **In Bearbeitung (`working`)** – File or folder is currently being worked on
* **Erledigt (`done`)** – Processing is completed
* **Wartet (`waiting`)** – Waiting for review or feedback
* **Problem (`problem`)** – Issue or clarification required
* **Status entfernen (`remove`)** – Removes the entry from `status.json` and clears the overlay

---

## Installation (End Users)

### Prerequisites
* Windows 10 or Windows 11 (64-bit) — **no additional runtimes required**

### Setup
1. Download the latest release archive from **[Releases](../../releases)** and extract all contents into a permanent folder (e.g., `C:\Tools\ExplorerStatus\`).
   > **Important:** The **`icons/`** subfolder containing all 8 `.ico` files must remain in the same directory as `ExplorerStatus.dll`.
2. Right-click **`install.bat`** and select **Run as administrator** to register `ExplorerStatus.dll` via `regsvr32`.
3. Restart Windows Explorer (or sign out and back in) so Windows loads the icon overlays and context menu.

*Uninstallation:* Run **`uninstall.bat`** as administrator and restart Windows Explorer.

---

## Building from Source

### Prerequisites
* Windows 10 / 11 (64-bit)
* Visual Studio or **Visual Studio Build Tools** with C++ desktop development (`cl.exe`, `rc.exe`, `link.exe`)

### Build Steps
1. Clone the repository:
   ```bash
   git clone [https://github.com/otti73/ExplorerStatus.git](https://github.com/otti73/ExplorerStatus.git)
   cd ExplorerStatus