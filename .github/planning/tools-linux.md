# Development Tools: GP2040-CE on Linux

This document lists every tool required to build, flash, and develop GP2040-CE locally on Linux (tested on Arch Linux), in the order you should install them.

---

## 1. Git

**Why:** The build system calls `git describe` to embed the firmware version string. Git submodule initialization is also triggered automatically by CMake (fetches pico-sdk dependencies, tinyusb, nanopb, etc.). Without Git in PATH the cmake configure step fails immediately.

**Install:**
Install Git using your package manager, for example on Arch Linux: `sudo pacman -S git`. Ensure `git` is on PATH.

**Verify:**
```zsh
git --version
```

**Clone the repo:**
```zsh
git clone --recursive https://github.com/demingongo/GP2040-CE.git
git remote add upstream https://github.com/OpenStickCommunity/GP2040-CE.git
git fetch upstream --tags
```

Fetching tags from upstream is required to build the firmware version string correctly. The `--recursive` flag initializes submodules automatically, but if you forget it, you can run `git submodule update --init --recursive` after cloning.

---

## 2. Raspberry Pi Pico VS Code Extension (recommended path)

**Why:** This is the official, officially supported Linux development path. The extension automatically installs and manages the correct versions of:
- **ARM GNU Toolchain** (`arm-none-eabi-gcc`) — cross-compiler that turns C/C++ into RP2040/RP2350 machine code
- **CMake** — the build system generator used by the project
- **Ninja** — the fast build backend driven by CMake
- **pico-sdk** — the Raspberry Pi Pico SDK (version 2.2.0 as required by this project)

Using the extension avoids manually managing SDK versions and toolchain paths.

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
```zsh
arm-none-eabi-gcc --version
cmake --version
ninja --version
```
These are in `~/.pico-sdk/toolchain/14_2_Rel1/bin/` and `~/.pico-sdk/cmake/*/bin/` — the extension adds them to PATH for the integrated terminal automatically.

---

## 3. Python 3 (system-wide)

**Why:** The protobuf compilation step (`compile_proto.cmake`) uses Python to create a virtual environment and run `nanopb_generator.py`, which converts `proto/config.proto` and `proto/enums.proto` into `config.pb.h` / `config.pb.c`. Without Python 3 in PATH, CMake configure fails with `Python3 not found`.

**Install:**
Install Python using your package manager, for example on Arch Linux: `sudo pacman -S python`. Ensure `python3` is on PATH.

> CMake will handle creating the venv and installing nanopb's requirements automatically — you do not need to install anything with pip manually.

---

## 4. Node.js (LTS)

**Why:** The web configurator (`www/`) is a React/Vite app. CMake calls `npm ci` and `npm run build` during configure to compile the web UI into binary data that gets embedded in the firmware. Without Node.js/npm the build fails unless you set `SKIP_WEBBUILD=TRUE`.

**Install:**
Install NVM using your package manager, for example on Arch Linux: `sudo pacman -S nvm`. Then install Node.js LTS using NVM:
```zsh
nvm install --lts
nvm use --lts
```

Ensure `node` and `npm` are on PATH. You can also install Node.js directly from your package manager, but NVM is recommended for managing multiple versions.

**Verify:**
```zsh
node --version
npm --version
```

> To skip the web build during iteration (faster configure): set the environment variable `SKIP_WEBBUILD=TRUE` before running cmake. The firmware will work but the web configurator UI will be missing.

---

## Building the Project

### One-time configure (from the repo root):

```zsh
export GP2040_BOARDCONFIG="Pico2"
export PICO_SDK_PATH="$HOME/.pico-sdk/sdk/2.2.0"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

### Build:

```zsh
cmake --build build --parallel
```

Output is `build/GP2040-CE_<version>_Pico2.uf2`.

### Build a specific board config (e.g., the new Pico2EspBridge):

```zsh
export GP2040_BOARDCONFIG="Pico2EspBridge"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

or

```zsh
GP2040_BOARDCONFIG="Pico2EspBridge" cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Skip web build for faster iteration:

```zsh
export SKIP_WEBBUILD=TRUE
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Clean rebuild (when switching board configs):

A clean rebuild (when switching board configs):

```zsh
rm -rf build
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
- **CMake Tools** (Microsoft) — GUI for configuring and building from the status bar

