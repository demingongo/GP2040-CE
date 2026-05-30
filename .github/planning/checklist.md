# Checklist: `feat/esp32-uart-bridge`

## Branch Setup

- [x] Create branch `feat/esp32-uart-bridge`

---

## Files to Create

### `proto/config.proto`
- [ ] Add `EspUartBridgeOptions` message (enabled, uartBlock, txPin, rxPin, baudRate)
- [ ] Add `espUartBridgeOptions` field (field 31) to `AddonOptions` message

### `headers/addons/esp_uart_bridge.h`
- [ ] Create file with `#ifndef` guards for all defaults (ENABLED, UART_BLOCK, TX_PIN, RX_PIN, BAUD)
- [ ] Declare `EspUartBridgeAddon` class inheriting `GPAddon`
- [ ] Declare private members (`uart_inst_t*`, `baudRate`, `txPin`, `rxPin`)

### `src/addons/esp_uart_bridge.cpp`
- [ ] Implement `available()` — check enabled + valid pins via `isValidPin()`
- [ ] Implement `setup()` — read options from storage, call `uart_init()`, `gpio_set_function()`, set baud rate
- [ ] Implement `process()` — read `GetProcessedGamepad()->state`, pack 18-byte frame, XOR checksum, `uart_write_blocking()`
- [ ] Implement empty stubs: `preprocess()`, `postprocess()`, `reinit()`

### `configs/Pico2EspBridge/BoardConfig.h`
- [ ] Copy from `configs/Pico2/BoardConfig.h`
- [ ] Set `GPIO_PIN_26` and `GPIO_PIN_27` to `GpioAction::ASSIGNED_TO_ADDON`
- [ ] Add `ESP_UART_BRIDGE_*` defines (ENABLED=1, UART_BLOCK=1, TX_PIN=26, RX_PIN=27, BAUD=1000000)

### `configs/Pico2EspBridge/Pico2EspBridge.cmake`
- [ ] Create file: `set(PICO_BOARD pico2)` + `set(PICO_PLATFORM rp2350-arm-s)`

---

## Files to Modify

### `src/gp2040aux.cpp`
- [ ] Add `#include "addons/esp_uart_bridge.h"`
- [ ] Add `addons.LoadAddon(new EspUartBridgeAddon())` in `setup()`

### `CMakeLists.txt`
- [ ] Add `src/addons/esp_uart_bridge.cpp` to `add_executable` source list
- [ ] Verify `hardware_uart` is in `target_link_libraries` (add if missing)

### `src/webconfig.cpp` *(optional)*
- [ ] Add `EspUartBridgeOptions` read handler in `/api/addons/get`
- [ ] Add `EspUartBridgeOptions` write handler in `/api/addons/set`

---

## Build

- [ ] Run `cmake` — verify protobuf regenerates cleanly (`config.pb.h` / `config.pb.c`)
- [ ] Build with `GP2040_BOARDCONFIG=Pico2EspBridge` — no errors
- [ ] Build with default `Pico` config — no regressions (addon disabled by default)

---

## Hardware Validation

- [ ] Flash `Pico2EspBridge` firmware to Pico2
- [ ] Wire Pico2 GPIO 26 (TX) → ESP32 RX, GPIO 27 (RX) → ESP32 TX, GND → GND
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
