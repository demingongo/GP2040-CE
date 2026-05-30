# Plan: `feat/esp32-uart-bridge` Branch

## Goal

Add a new GP2040-CE addon that streams processed gamepad state to an ESP32 over UART. The ESP32 (running esp-gamepad firmware) re-broadcasts this state as a BLE HID gamepad, enabling wireless connectivity without touching any existing USB driver or core gamepad logic.

The Pico2 continues to function as a normal USB gamepad simultaneously.

---

## Branch Name

```
feat/esp32-uart-bridge
```

---

## UART Frame Protocol

A fixed-size 16-byte binary frame sent at 1 MHz baud (can be tuned):

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
- `GPIO_PIN_26` and `GPIO_PIN_27` → `GpioAction::ASSIGNED_TO_ADDON` (UART1 TX/RX; these are free on Pico2 and map to UART1)
- Add:
  ```c
  #define ESP_UART_BRIDGE_ENABLED    1
  #define ESP_UART_BRIDGE_UART_BLOCK 1
  #define ESP_UART_BRIDGE_TX_PIN     26
  #define ESP_UART_BRIDGE_RX_PIN     27
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

## Build & Test Sequence

1. Create branch: `git checkout -b feat/esp32-uart-bridge`
2. Create all new files and apply all modifications above
3. Regenerate protobuf: `cmake` will call `compile_proto.cmake` automatically
4. Build with: `GP2040_BOARDCONFIG=Pico2EspBridge cmake .. && make`
5. Flash to Pico2, connect TX/RX to ESP32 UART RX/TX
6. Flash esp-gamepad firmware to ESP32
7. Validate UART frames with a logic analyzer or second Pico reading the bus
8. Validate BLE HID pairing and input response on PC
