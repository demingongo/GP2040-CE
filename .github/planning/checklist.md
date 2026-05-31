# Checklist: `feat/esp32-uart-bridge`

## Branch Setup

- [x] Create branch `feat/esp32-uart-bridge`

---

## Files to Create

### `proto/config.proto`
- [x] Add `EspUartBridgeOptions` message (enabled, uartBlock, txPin, rxPin, baudRate)
- [x] Add `espUartBridgeOptions` field (field 31) to `AddonOptions` message

### `headers/addons/esp_uart_bridge.h`
- [x] Create file with `#ifndef` guards for all defaults (ENABLED, UART_BLOCK, TX_PIN, RX_PIN, BAUD)
- [x] Declare `EspUartBridgeAddon` class inheriting `GPAddon`
- [x] Declare private members (`uart_inst_t*`, `baudRate`, `txPin`, `rxPin`)

### `src/addons/esp_uart_bridge.cpp`
- [x] Implement `available()` — check enabled + valid pins via `isValidPin()`
- [x] Implement `setup()` — read options from storage, call `uart_init()`, `gpio_set_function()`, set baud rate
- [x] Implement `process()` — read `GetProcessedGamepad()->state`, pack 18-byte frame, XOR checksum, `uart_write_blocking()`
- [x] Implement empty stubs: `preprocess()`, `postprocess()`, `reinit()`

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

---

## Build

- [x] Run `cmake` — verify protobuf regenerates cleanly (`config.pb.h` / `config.pb.c`)
- [x] Build with `GP2040_BOARDCONFIG=Pico2EspBridge` — no errors
- [ ] Build with default `Pico` config — no regressions (addon disabled by default)

---

## Hardware Validation

- [ ] Flash `Pico2EspBridge` firmware to Pico2
- [ ] Wire Pico2 GPIO 20 (UART1 TX, physical pin 26) → ESP32 RX, GPIO 21 (UART1 RX, physical pin 27) → ESP32 TX, GND → GND
- [ ] Flash esp-gamepad firmware to ESP32
- [ ] Confirm UART frames are transmitted (logic analyzer or Serial monitor on ESP32)
- [ ] Confirm XOR checksum passes on every frame
- [ ] Confirm no frame corruption under sustained button input

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
