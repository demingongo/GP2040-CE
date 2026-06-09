# Plan: `feat/esp32-uart-bridge` Branch

## Goal

Add a new GP2040-CE addon that streams processed gamepad state to an ESP32 over UART. The ESP32 (running esp-gamepad firmware) re-broadcasts this state as a BLE HID gamepad, enabling wireless connectivity without touching any existing USB driver or core gamepad logic.

The Pico2 continues to function as a normal USB gamepad simultaneously.

---

## Status

| Item | Status |
|---|---|
| UART frame protocol defined | ✅ done |
| `esp_uart_bridge.h` / `.cpp` | ✅ done |
| `configs/Pico2EspBridge/BoardConfig.h` | ✅ done |
| `configs/Pico2EspBridge/Pico2EspBridge.cmake` | ✅ done |
| `proto/config.proto` — `EspUartBridgeOptions` | ✅ done |
| `src/gp2040aux.cpp` — addon registered | ✅ done |
| `src/config_utils.cpp` — `INIT_UNSET_PROPERTY` | ✅ done |
| `src/webconfig.cpp` — `/api/addons` get/set | ✅ done |
| `CMakeLists.txt` — source + `hardware_uart` | ✅ done |
| Web UI `EspUartBridge.tsx` | ✅ done |
| `clk_peri` fix (`uart_set_baudrate` after `tud_inited()`) | ✅ done |
| `disableWhenUsbConnected` option (`tud_mounted()` USB suppression) | ✅ done |
| Pico-side UART frames confirmed at 1 Mbaud (CH340) | ✅ done |
| ESP32 receiver firmware (`esp-gamepad`) | 🔲 todo |
| End-to-end BLE HID validation | 🔲 todo |

---

## Branch Name

```
feat/esp32-uart-bridge
```

---

## UART Frame Protocol

A fixed-size 18-byte binary frame sent at 1 MHz baud (can be tuned):

| Byte(s) | Field      | Type     | Notes                        |
|---------|------------|----------|------------------------------|
| 0       | Start      | `0xA5`   | Magic byte                   |
| 1–4     | buttons    | uint32   | Full button bitmask          |
| 5       | dpad       | uint8    | Processed dpad value         |
| 6–7     | lx         | uint16   | Left stick X (0–0xFFFF)      |
| 8–9     | ly         | uint16   | Left stick Y (0–0xFFFF)      |
| 10–11   | rx         | uint16   | Right stick X (0–0xFFFF)     |
| 12–13   | ry         | uint16   | Right stick Y (0–0xFFFF)     |
| 14      | lt         | uint8    | Left trigger (0–0xFF)        |
| 15      | rt         | uint8    | Right trigger (0–0xFF)       |
| 16      | checksum   | uint8    | XOR of bytes 1–15            |
| 17      | End        | `0x5A`   | Magic byte                   |

Total: **18 bytes per frame**

---

## Files to Create

### 1. `headers/addons/esp_uart_bridge.h`

New addon header following the same pattern as existing addons (e.g. `drv8833_rumble.h`).

**Purpose:** Declare the `EspUartBridgeAddon` class inheriting `GPAddon`. Defines compile-time defaults for UART block, TX pin, RX pin, and baud rate as `#ifndef` guards so board configs can override them.

```
#ifndef ESP_UART_BRIDGE_ENABLED    → 0
#ifndef ESP_UART_BRIDGE_UART_BLOCK → 1   (UART1, avoids conflict with debug UART0)
#ifndef ESP_UART_BRIDGE_TX_PIN     → -1
#ifndef ESP_UART_BRIDGE_RX_PIN     → -1
#ifndef ESP_UART_BRIDGE_BAUD       → 1000000
```

Declares private members: `uart_inst_t* uartInst`, `uint32_t baudRate`, `int txPin`, `int rxPin`.

---

### 2. `src/addons/esp_uart_bridge.cpp`

Implementation of `EspUartBridgeAddon`.

**Purpose:** Initialize UART hardware and send a frame every `process()` call (called on Core 1 at ~1 ms interval).

- `available()`: Returns `true` only if `enabled`, `txPin` and `rxPin` are both valid (using existing `isValidPin()`), and the options are loaded from storage.
- `setup()`: Reads `EspUartBridgeOptions` from `Storage::getInstance().getAddonOptions().espUartBridgeOptions`. Calls `uart_init()`, `gpio_set_function()` for TX and RX with `GPIO_FUNC_UART`, sets baud rate.
- `process()`: Reads `Storage::getInstance().GetProcessedGamepad()->state`, packs the 18-byte frame, computes XOR checksum, calls `uart_write_blocking()`.
- `preprocess()`, `postprocess()`, `reinit()`: empty stubs.

---

### 3. `configs/Pico2EspBridge/BoardConfig.h`

A new board config derived from the standard Pico2 config.

**Purpose:** Provide a ready-to-use build target for a Pico2 wired to an ESP32. Frees two GPIO pins for UART and enables the addon.

Key differences from `configs/Pico2/BoardConfig.h`:
- `GPIO_PIN_20` and `GPIO_PIN_21` → `GpioAction::ASSIGNED_TO_ADDON` — these are the **actual UART1 TX/RX pins** on RP2350 (function F2). GPIO 26/27 are UART1 **CTS/RTS** (flow control only) and cannot carry data.
- A1 (Guide/Home/PS) and A2 (Capture) buttons are sacrificed to free these pins.
- GPIO 26 and 27 are left undefined (same as stock Pico2 — they are CTS/RTS and unused).
- Add:
  ```c
  #define ESP_UART_BRIDGE_ENABLED    1
  #define ESP_UART_BRIDGE_UART_BLOCK 1
  #define ESP_UART_BRIDGE_TX_PIN     20   // UART1 TX (RP2350 F2) — physical pin 26 on Pico2
  #define ESP_UART_BRIDGE_RX_PIN     21   // UART1 RX (RP2350 F2) — physical pin 27 on Pico2
  #define ESP_UART_BRIDGE_BAUD       1000000
  ```

Everything else (buttons, display, LEDs) remains identical to the stock Pico2 config.

---

### 4. `configs/Pico2EspBridge/Pico2EspBridge.cmake`

**Purpose:** CMake board config file for the new build target, identical in structure to `configs/Pico2/Pico2.cmake`.

```cmake
set(PICO_BOARD pico2)
set(PICO_PLATFORM rp2350-arm-s)
```

---

## Files to Modify

### 5. `proto/config.proto`

**Purpose:** Add persistent configuration for the addon so its settings survive reboots and are accessible from the web configurator.

Add a new message before `AddonOptions`:

```protobuf
message EspUartBridgeOptions {
    optional bool enabled = 1 [default = false];
    optional int32 uartBlock = 2 [default = 1];
    optional int32 txPin = 3 [default = -1];
    optional int32 rxPin = 4 [default = -1];
    optional uint32 baudRate = 5 [default = 1000000];
}
```

Add to `AddonOptions` message at the next available field number (currently 31):

```protobuf
optional EspUartBridgeOptions espUartBridgeOptions = 31;
```

**Impact:** Triggers protobuf regeneration (`compile_proto.cmake`). All generated `config.pb.h` / `config.pb.c` files are auto-generated and must be rebuilt.

---

### 6. `src/gp2040aux.cpp`

**Purpose:** Register the new addon so it runs on Core 1.

Add include at the top:
```cpp
#include "addons/esp_uart_bridge.h"
```

Add in `GP2040Aux::setup()`, alongside the existing addon registrations:
```cpp
addons.LoadAddon(new EspUartBridgeAddon());
```

---

### 7. `CMakeLists.txt`

**Purpose:** Include the new source file in the build.

Add to the `add_executable(${PROJECT_NAME} ...)` source list:
```cmake
src/addons/esp_uart_bridge.cpp
```

Also add `hardware_uart` to `target_link_libraries` if not already present (check existing list — `hardware_i2c` and `hardware_spi` are there; `hardware_uart` may need adding).

---

### 8. `src/webconfig.cpp` *(optional, recommended)*

**Purpose:** Expose `EspUartBridgeOptions` in the web configurator's add-ons API endpoint (`/api/addons/get` and `/api/addons/set`), allowing TX pin, RX pin, baud rate, and enabled flag to be configured without reflashing.

This follows the exact same pattern used for all other addons in this file (read options from JSON, write back to storage).

---

### 9. `src/config_utils.cpp`

**Purpose:** Seed `espUartBridgeOptions` defaults from BoardConfig.h macros into persistent storage on first boot. Without this block every other addon has `INIT_UNSET_PROPERTY` entries, but the ESP32 UART Bridge was missing them, causing `options.enabled` to always be `false` at runtime regardless of `ESP_UART_BRIDGE_ENABLED` in BoardConfig.h — the addon would never start and the web configurator would not show it.

Add near the end of the init function (after `tg16Options`):
```cpp
// addonOptions.espUartBridgeOptions
INIT_UNSET_PROPERTY(config.addonOptions.espUartBridgeOptions, enabled, !!ESP_UART_BRIDGE_ENABLED);
INIT_UNSET_PROPERTY(config.addonOptions.espUartBridgeOptions, uartBlock, ESP_UART_BRIDGE_UART_BLOCK);
INIT_UNSET_PROPERTY(config.addonOptions.espUartBridgeOptions, txPin, (Pin_t)ESP_UART_BRIDGE_TX_PIN);
INIT_UNSET_PROPERTY(config.addonOptions.espUartBridgeOptions, rxPin, (Pin_t)ESP_UART_BRIDGE_RX_PIN);
INIT_UNSET_PROPERTY(config.addonOptions.espUartBridgeOptions, baudRate, ESP_UART_BRIDGE_BAUD);
INIT_UNSET_PROPERTY(config.addonOptions.espUartBridgeOptions, disableWhenUsbConnected, !!ESP_UART_BRIDGE_DISABLE_WHEN_USB);
```

Also add `#include "addons/esp_uart_bridge.h"` alongside the other addon includes at the top of the file.

---

### 11. `disableWhenUsbConnected` — UART suppression when USB power is present

**Problem:** With the original implementation the UART bridge transmitted continuously once `tud_inited()` returned true, even while the Pico2 was powered and enumerated over USB. In wired (USB) mode the BLE path is unnecessary and the constant UART traffic wastes power and may confuse an attached ESP32.

**Solution:** Add a boolean option `disableWhenUsbConnected` (proto field 6, default `true`). When enabled the addon calls `tud_mounted()` each `process()` call. If the USB host has the device mounted (enumerated as a HID gamepad), the frame is skipped silently.

`tud_mounted()` is used rather than reading GPIO 24 (PICO_VBUS_PIN) directly because on RP2350 the GPIO 24 input buffer is not reliably active in this execution context (Core 1, after USB init on Core 0). `tud_mounted()` is a single-byte flag set atomically by Core 0 and is safe to read from Core 1. It is also semantically more precise: UART is suppressed exactly when the Pico2 is actively enumerated as a USB HID device, not merely when VBUS power is present.

Note: during the brief window between `tud_inited()` and full USB enumeration (~100–500 ms), a handful of frames may leak through before `tud_mounted()` becomes true. This is acceptable.

**This option can be disabled via the web configurator** (Add-Ons page → ESP32 UART Bridge → "Disable UART when USB is connected" toggle off). When off, `disableWhenUsbConnected` is stored as `false` in persistent storage, and the `tud_mounted()` check in `process()` is entirely bypassed — UART frames are sent regardless of USB state. The full save/load path is:

> web UI toggle → `setFieldValue(name, 0/1)` → `setAddonsOptions` POST → `docToValue(espUartBridgeOptions.disableWhenUsbConnected, ...)` → flash storage → loaded in `setup()` → `disableWhenUsbConnected` member → `if (disableWhenUsbConnected && tud_mounted())` guard in `process()`

**Behaviour summary:**

| Option state | USB state | `tud_mounted()` | UART frames sent? |
|---|---|---|---|
| Enabled (default) | Enumerated by USB host | true | No (suppressed) |
| Enabled (default) | Not connected / VSYS only | false | Yes |
| Disabled (via web UI) | Any | any | Yes (always) |

**Default:** `ESP_UART_BRIDGE_DISABLE_WHEN_USB 1` in `esp_uart_bridge.h` — enabled by default on all board configs. Override in BoardConfig.h to change the default for a specific board.

**Files changed:**
- `proto/config.proto` — added field `disableWhenUsbConnected = 6`
- `headers/addons/esp_uart_bridge.h` — added `ESP_UART_BRIDGE_DISABLE_WHEN_USB` default macro; added `disableWhenUsbConnected` private member
- `src/addons/esp_uart_bridge.cpp` — reads option in `setup()`; calls `tud_mounted()` in `process()` to suppress frames when USB host is active (replaced unreliable `gpio_get(PICO_VBUS_PIN)` on RP2350)
- `src/config_utils.cpp` — `INIT_UNSET_PROPERTY` for new field
- `src/webconfig.cpp` — `docToValue` / `writeDoc` for `espUartBridgeDisableWhenUsbConnected`
- `www/src/Addons/EspUartBridge.tsx` — toggle switch added inside the options panel
- `www/src/Locales/en/AddonsConfig.jsx` — `esp-uart-bridge-disable-when-usb-label`

---

### 10. Web configurator UI

**New file: `www/src/Addons/EspUartBridge.tsx`**

React component following the same pattern as `TG16.tsx`. Exports:
- `espUartBridgeScheme` — Yup validation (currently empty, fields are plain numbers)
- `espUartBridgeState` — default form values matching the protobuf field names used by `webconfig.cpp`
- `EspUartBridge` — section component with enable toggle and four numeric inputs

**Modified: `www/src/Locales/en/AddonsConfig.jsx`**

Add translation strings:
- `esp-uart-bridge-header-text`
- `esp-uart-bridge-uart-block-label`
- `esp-uart-bridge-tx-pin-label`
- `esp-uart-bridge-rx-pin-label`
- `esp-uart-bridge-baud-rate-label`

**Modified: `www/src/Pages/AddonsConfigPage.tsx`**

Import component and spread its scheme/state into the page's validation schema, `DEFAULT_VALUES`, and `ADDONS` render list.

---

### 11. `www/src/Store/useSystemStats.ts` — bug fix

**Problem:** All three fetches (local firmware API, local memory API, GitHub releases API) were in a single `Promise.all`. Any GitHub API failure (rate limit, no internet) caused the entire home page to show nothing.

**Fix:** Split the fetches — local stats are fetched together (blocking), then the GitHub releases call runs independently with `.catch(() => null)`. Home page always renders local stats; latest-version fields are empty only if GitHub is unreachable.

---

## Build & Test Sequence

1. ✅ Branch created: `feat/esp32-uart-bridge`
2. ✅ All Pico-side files created and modified
3. ✅ Protobuf regenerated automatically by cmake
4. ✅ Build: `cmake --build build --parallel` (exit 0)
5. ✅ Flash to Pico2 (BOOTSEL drag-drop), factory-reset with S1+S2+Up
6. ✅ UART frames confirmed at 1 Mbaud via CH340 on GPIO 20 — buttons, dpad, axes all correct
7. 🔲 Write ESP32 receiver in `esp-gamepad` (read UART, parse frames, send BLE HID reports)
8. 🔲 Wire Pico GPIO 20 → ESP32 UART RX; validate BLE HID pairing and input on PC

### Key implementation note — `clk_peri` fix (permanent, do not remove)

On RP2350, `tud_init()` (called on Core 0 after Core 1 is already running) reconfigures `clk_peri`, which invalidates the UART baud-rate divisor set during `setup()`. The fix is in `process()`: poll `tud_inited()` and call `uart_set_baudrate()` once it returns true. The `uartReady` flag gates all frame transmission until this correction is applied.
