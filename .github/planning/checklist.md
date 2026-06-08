# Checklist: `feat/esp32-uart-bridge`

## Branch Setup

- [x] Create branch `feat/esp32-uart-bridge`

---

## Files to Create

### `proto/config.proto`
- [x] Add `EspUartBridgeOptions` message (enabled, uartBlock, txPin, rxPin, baudRate)
- [x] Add `espUartBridgeOptions` field (field 31) to `AddonOptions` message
- [x] Add `disableWhenUsbConnected` field (field 6, default true) to `EspUartBridgeOptions`

### `headers/addons/esp_uart_bridge.h`
- [x] Create file with `#ifndef` guards for all defaults (ENABLED, UART_BLOCK, TX_PIN, RX_PIN, BAUD)
- [x] Declare `EspUartBridgeAddon` class inheriting `GPAddon`
- [x] Declare private members (`uart_inst_t*`, `baudRate`, `txPin`, `rxPin`, `uartReady`)
- [x] Add `ESP_UART_BRIDGE_DISABLE_WHEN_USB` default macro (=1); add `disableWhenUsbConnected` private member

### `src/addons/esp_uart_bridge.cpp`
- [x] Implement `available()` — check enabled + valid pins via `isValidPin()`
- [x] Implement `setup()` — read options from storage, call `uart_init()`, `gpio_set_function()`, set baud rate
- [x] Implement `process()` — read `GetProcessedGamepad()->state`, pack 18-byte frame, XOR checksum, `uart_write_blocking()`
- [x] Implement empty stubs: `preprocess()`, `postprocess()`, `reinit()`
- [x] **`clk_peri` fix** — `tud_init()` on Core 0 reconfigures `clk_peri` after Core 1's `setup()` runs, invalidating the UART baud divisor. Fix: poll `tud_inited()` in `process()` and call `uart_set_baudrate()` once it returns true. `uartReady` flag gates all frame TX until correction is applied. **Permanent — do not remove.**
- [x] **VBUS suppression** — define `PICO_VBUS_PIN` fallback (24); init GPIO 24 as input in `setup()` when `disableWhenUsbConnected`; skip frame in `process()` when `gpio_get(PICO_VBUS_PIN)` is high

### `configs/Pico2EspBridge/BoardConfig.h`
- [x] Copy from `configs/Pico2/BoardConfig.h`
- [x] Set `GPIO_PIN_20` and `GPIO_PIN_21` to `GpioAction::ASSIGNED_TO_ADDON` (UART1 TX/RX — GPIO 26/27 are CTS/RTS only, not usable for data)
- [x] A1/A2 buttons sacrificed to free GPIO 20/21 for UART
- [x] Add `ESP_UART_BRIDGE_*` defines (ENABLED=1, UART_BLOCK=1, TX_PIN=20, RX_PIN=21, BAUD=1000000)

### `configs/Pico2EspBridge/Pico2EspBridge.cmake`
- [x] Create file: `set(PICO_BOARD pico2)` + `set(PICO_PLATFORM rp2350-arm-s)`

---

## Files to Modify

### `src/gp2040aux.cpp`
- [x] Add `#include "addons/esp_uart_bridge.h"`
- [x] Add `addons.LoadAddon(new EspUartBridgeAddon())` in `setup()`

### `CMakeLists.txt`
- [x] Add `src/addons/esp_uart_bridge.cpp` to `add_executable` source list
- [x] Verify `hardware_uart` is in `target_link_libraries` (add if missing)

### `src/webconfig.cpp` *(optional)*
- [x] Add `EspUartBridgeOptions` read handler in `/api/addons/get`
- [x] Add `EspUartBridgeOptions` write handler in `/api/addons/set`

### `src/config_utils.cpp`
- [x] Add `#include "addons/esp_uart_bridge.h"`
- [x] Add `INIT_UNSET_PROPERTY` block for `espUartBridgeOptions` (enabled, uartBlock, txPin, rxPin, baudRate) so BoardConfig.h defaults are seeded into storage on first boot

### `www/src/Addons/EspUartBridge.tsx` *(new file)*
- [x] Create React addon component with enable toggle and fields for UART block, TX pin, RX pin, baud rate
- [x] Export `espUartBridgeScheme`, `espUartBridgeState`, and default component
- [x] Add `espUartBridgeDisableWhenUsbConnected` to state (default 1) and toggle switch inside the options panel

### `www/src/Locales/en/AddonsConfig.jsx`
- [x] Add translation strings for ESP32 UART Bridge section header and all field labels
- [x] Add `esp-uart-bridge-disable-when-usb-label` translation string

### `www/src/Pages/AddonsConfigPage.tsx`
- [x] Import `EspUartBridge`, `espUartBridgeScheme`, `espUartBridgeState`
- [x] Spread `espUartBridgeScheme` into validation schema
- [x] Spread `espUartBridgeState` into `DEFAULT_VALUES`
- [x] Add `EspUartBridge` to `ADDONS` render list

### `src/config_utils.cpp`
- [x] Add `#include "addons/esp_uart_bridge.h"`
- [x] Add `INIT_UNSET_PROPERTY` block for `espUartBridgeOptions` (enabled, uartBlock, txPin, rxPin, baudRate) so BoardConfig.h defaults are seeded into storage on first boot
- [x] Add `INIT_UNSET_PROPERTY` for `disableWhenUsbConnected` (seeded from `ESP_UART_BRIDGE_DISABLE_WHEN_USB`)

### `src/webconfig.cpp`
- [x] Add `EspUartBridgeOptions` read handler in `/api/addons/get`
- [x] Add `EspUartBridgeOptions` write handler in `/api/addons/set`
- [x] Add `docToValue` / `writeDoc` for `espUartBridgeDisableWhenUsbConnected`

### `www/src/Store/useSystemStats.ts`
- [x] Fix: decouple GitHub releases API fetch from local stats — failure no longer blocks home page from rendering

---

## Build

- [x] Run `cmake` — verify protobuf regenerates cleanly (`config.pb.h` / `config.pb.c`)
- [x] Build with `GP2040_BOARDCONFIG=Pico2EspBridge` — no errors (`cmake --build build --parallel`, exit 0)
- [ ] Build with default `Pico` config — no regressions (addon disabled by default)

## Web Configurator

- [x] Home page system stats render even when GitHub API is unreachable
- [x] Add-Ons page shows "ESP32 UART Bridge Configuration" section
- [x] Toggle enables/disables addon; UART block, TX pin, RX pin, baud rate are editable
- [ ] "Disable UART when USB connected" toggle visible and saves correctly
- [ ] Save settings via web UI and confirm persistence across reboot

---

## Hardware Validation — Pico side (UART frames)

- [x] Flash `Pico2EspBridge` firmware to Pico2; factory-reset with S1+S2+Up to seed BoardConfig.h defaults into storage
- [x] Wire CH340 RX → GPIO 20 (physical pin 26, right side 6th from bottom), GND → GND
- [x] Open RealTerm at 1,000,000 baud on CH340 COM port — continuous 18-byte frames confirmed
- [x] Confirm frame structure: `A5` start, buttons (4B LE), dpad (1B), lx/ly/rx/ry (2B LE each), lt/rt (1B each), XOR checksum, `5A` end
- [x] Confirm XOR checksum correct on idle frames
- [x] Confirm buttons change frame bytes in real time (S1 → byte [2] = `01`, S2 → byte [2] = `02`, UP dpad → byte [5] = `01`)
- [x] Confirm joystick center values: `FF 7F` (0x7FFF) on all four axes
- [ ] Confirm no frame corruption under sustained button input (stress test)

## Hardware Validation — ESP32 integration

- [ ] Wire Pico2 GPIO 20 → ESP32 UART RX, GPIO 21 → ESP32 UART TX, GND → GND
- [ ] Flash esp-gamepad firmware to ESP32
- [ ] Confirm ESP32 receives and parses frames (Serial monitor at 115200 showing decoded state)
- [ ] Confirm XOR checksum validation passes on ESP32 side

## BLE Validation

- [ ] ESP32 advertises as `GP2040-CE Wireless`
- [ ] Pair with Windows 10/11 — recognized as game controller
- [ ] Test in `joy.cpl` — all 32 buttons respond correctly
- [ ] Test hat switch (dpad) — all 8 directions correct
- [ ] Test analog axes (LX, LY, RX, RY) — full range, no drift
- [ ] Test triggers (LT, RT)
- [ ] Test macros configured in GP2040-CE Web Config — fire correctly over BLE
- [ ] Test SOCD cleaning — correct behavior reflected wirelessly
- [ ] Pair with Android — recognized and inputs work
- [ ] Test in Steam via Steam Input
- [ ] Test on Linux

## Regression Validation

- [ ] USB wired output still works simultaneously with BLE active
- [ ] Web configurator still accessible during BLE operation
- [ ] No increased input latency on USB side when UART addon is running
