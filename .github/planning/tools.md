# Development Tools: GP2040-CE on Windows

This document lists every tool required to build, flash, and develop GP2040-CE locally on Windows, in the order you should install them.

---

## 1. Git

**Why:** The build system calls `git describe` to embed the firmware version string. Git submodule initialization is also triggered automatically by CMake (fetches pico-sdk dependencies, tinyusb, nanopb, etc.). Without Git in PATH the cmake configure step fails immediately.

**Install:**
Download the Windows installer from https://git-scm.com/download/win and run it. Accept all defaults. Ensure "Git from the command line and also from 3rd-party software" is selected so `git` is on PATH.

**Verify:**
```powershell
git --version
```

---

## 2. Raspberry Pi Pico VS Code Extension (recommended path)

**Why:** This is the official, officially supported Windows development path. The extension automatically installs and manages the correct versions of:
- **ARM GNU Toolchain** (`arm-none-eabi-gcc`) — cross-compiler that turns C/C++ into RP2040/RP2350 machine code
- **CMake** — the build system generator used by the project
- **Ninja** — the fast build backend driven by CMake
- **pico-sdk** — the Raspberry Pi Pico SDK (version 2.2.0 as required by this project)
- **picotool** — tool for flashing `.uf2` files and inspecting firmware

Using the extension avoids manually managing SDK versions and toolchain paths, which is the most common source of build failures on Windows.

**Install:**
1. Open VS Code.
2. Go to the Extensions panel (Ctrl+Shift+X).
3. Search for **"Raspberry Pi Pico"** and install the extension published by *Raspberry Pi*.
4. After installing, open the Command Palette (Ctrl+Shift+P) and run **"Raspberry Pi Pico: Import Project"** — this triggers the extension to download and install all toolchain components automatically into `~/.pico-sdk/`.
5. Alternatively, just open the GP2040-CE folder; the extension detects `pico_sdk_import.cmake` and prompts you to set up the environment.

The `CMakeLists.txt` already includes the line:
```cmake
include(${picoVscode})
```
which hooks into the extension's automatically managed SDK and toolchain paths.

**Verify (after extension setup):**
```powershell
arm-none-eabi-gcc --version
cmake --version
ninja --version
```
These are in `~\.pico-sdk\toolchain\14_2_Rel1\bin\` and `~\.pico-sdk\cmake\*\bin\` — the extension adds them to PATH for the integrated terminal automatically.

---

## 3. Python 3 (system-wide)

**Why:** The protobuf compilation step (`compile_proto.cmake`) uses Python to create a virtual environment and run `nanopb_generator.py`, which converts `proto/config.proto` and `proto/enums.proto` into `config.pb.h` / `config.pb.c`. Without Python 3 in PATH, CMake configure fails with `Python3 not found`.

**Install:**
Download **Python 3.12** from https://www.python.org/downloads/release/python-31210/ and run the installer. During installation, **check "Add Python to PATH"** on the first screen.

> **Important:** Do **not** use Python 3.13 or 3.14. The nanopb build step installs `grpcio-tools==1.71.0`, which has no pre-built wheel for Python 3.13+. Pip will attempt to compile it from source and fail unless Microsoft C++ Build Tools are installed. Python 3.12 has a pre-built wheel and works out of the box.

> **If you have multiple Python versions installed** (e.g. via `uv` or other tools), CMake may pick up the wrong one. Pass it explicitly at configure time — see the configure command below.

**Verify:**
```powershell
py -3.12 --version
```

> CMake will handle creating the venv and installing nanopb's requirements automatically — you do not need to install anything with pip manually.

---

## 4. Node.js (LTS, v20.x)

**Why:** The web configurator (`www/`) is a React/Vite app. CMake calls `npm ci` and `npm run build` during configure to compile the web UI into binary data that gets embedded in the firmware. Without Node.js/npm the build fails unless you set `SKIP_WEBBUILD=TRUE`.

**Install:**
Download the LTS installer (v20.x) from https://nodejs.org/. Accept defaults.

**Verify:**
```powershell
node --version
npm --version
```

> To skip the web build during iteration (faster configure): set the environment variable `SKIP_WEBBUILD=TRUE` before running cmake. The firmware will work but the web configurator UI will be missing.

---

## 5. picotool

**Why:** Used to flash the compiled `.uf2` firmware onto the Pico2 from the command line, and to inspect or rescue a bricked device. The VS Code Pico extension installs it automatically (see step 2), but it is useful to know it is there.

**Location after extension install:** `~\.pico-sdk\picotool\2.2.0\picotool\picotool.exe`

**Flash a firmware file:**
```powershell
picotool load -f GP2040-CE_0.0.0_Pico2EspBridge.uf2
picotool reboot
```
Alternatively, hold BOOTSEL on the Pico2 while plugging in USB — it mounts as a USB drive. Drag and drop the `.uf2` onto it.

---

## Building the Project

### One-time configure (from the repo root):

```powershell
$env:GP2040_BOARDCONFIG = "Pico2"
$env:PICO_SDK_PATH = "$env:USERPROFILE\.pico-sdk\sdk\2.2.0"
$python312 = (py -3.12 -c 'import sys; print(sys.executable)')
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DPython3_EXECUTABLE=$python312"
```

The `-DPython3_EXECUTABLE` argument ensures CMake uses Python 3.12 even if a newer version (e.g. 3.14 via `uv`) is first on PATH.

### Build:

```powershell
cmake --build build --parallel
```

Output is `build/GP2040-CE_<version>_Pico2.uf2`.

### Build a specific board config (e.g., the new Pico2EspBridge):

```powershell
$env:GP2040_BOARDCONFIG = "Pico2EspBridge"
$env:PICO_SDK_PATH = "$env:USERPROFILE\.pico-sdk\sdk\2.2.0"
$python312 = (py -3.12 -c 'import sys; print(sys.executable)')
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DPython3_EXECUTABLE=$python312"
cmake --build build --parallel
```

### Skip web build for faster iteration:

```powershell
$env:SKIP_WEBBUILD = "TRUE"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Clean rebuild (when switching board configs):

```powershell
Remove-Item -Recurse -Force build
# then re-run configure + build
```
A clean is required when switching `GP2040_BOARDCONFIG` because CMake caches the previous config's generated files.

---

## VS Code Integration

After installing the Pico extension and opening the GP2040-CE folder, VS Code will:
- Offer to configure CMake automatically via the status bar.
- Provide IntelliSense using the generated `build/compile_commands.json` (enabled by `CMAKE_EXPORT_COMPILE_COMMANDS=ON` in `CMakeLists.txt`).
- Show build errors inline in the editor.

Recommended additional VS Code extensions:
- **C/C++** (Microsoft) — IntelliSense, hover docs, go-to-definition
- **CMake Tools** (Microsoft) — GUI for configuring and building from the status bar
- **CMake** (twxs) — syntax highlighting for `.cmake` files

---

## Summary Table

| Tool | Version | Managed by | Purpose |
|---|---|---|---|
| Git | Any recent | Manual install | Version embedding, submodule init |
| ARM GNU Toolchain (`arm-none-eabi-gcc`) | 14.2 Rel1 | Pico VS Code Extension | Cross-compile C/C++ for RP2040/RP2350 |
| CMake | 3.10–4.0 | Pico VS Code Extension | Build system generator |
| Ninja | Latest | Pico VS Code Extension | Fast build backend |
| pico-sdk | 2.2.0 | Pico VS Code Extension | Hardware abstraction for Pico/Pico2 |
| picotool | 2.2.0 | Pico VS Code Extension | Flash `.uf2` to device |
| Python 3 | 3.12.x | Manual install | Protobuf code generation (nanopb) |
| Node.js | 20.x LTS | Manual install | Web configurator build (React/Vite) |
