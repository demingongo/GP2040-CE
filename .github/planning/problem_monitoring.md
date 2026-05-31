# ESP UART Bridge — Problem Log

**Session date:** 2026-05-31  
**Goal:** Send gamepad state from RP2350 (Pico2EspBridge) to HC-06 Bluetooth SPP → COM5 → ESP32  
**Status: PAUSED — latest firmware flashed, HC-06 showed NOTHING (not even "UART OK")**

## Where We Left Off

The `uart_set_baudrate` fix was compiled clean and flashed. HC-06 on COM5 showed **nothing at all** — not even the `UART OK` banner that previously worked in an earlier build. This is a regression.

Per the decision tree below: the most likely cause is something changed in the HC-06 pairing / COM5 state, OR the banner in `setup()` is now not reaching the HC-06 before RealTerm opened the port. Less likely but possible: a different build artifact was flashed (verify `.uf2` path below).

**First thing to do next session:**
1. Verify HC-06 is still paired and COM5 exists in Device Manager
2. Open RealTerm on COM5 at 115200 **before** plugging Pico in
3. Alternatively: connect a USB-UART dongle directly to GPIO 0 (no Bluetooth at all) to eliminate HC-06 as a variable
4. If still nothing with a wired UART dongle, the `setup()` banner itself regressed — suspect a build/flash issue

**UF2 file to flash:** `build/GP2040-CE_0.7.12_Pico2EspBridge.uf2`  
**Flash method:** hold BOOTSEL while plugging USB, drag UF2 to RPI-RP2 drive

---

## Hardware Setup

- Board: Pico2EspBridge (RP2350 / Pico 2W variant)
- HC-06 module: factory-programmed to 115200 via `AT+BAUD8`, paired to Windows COM5 (outgoing SPP)
- Diagnostic wiring: Pico **GPIO 0** (physical pin 1, top-left) → HC-06 RXD
- HC-06 RXD is the pin labelled RXD on the module (NOT TXD — this is the pin that receives data from Pico)
- Normal wiring (post-diagnosis target): GPIO 20 / UART1 TX → HC-06 RXD, GPIO 21 / UART1 RX → HC-06 TXD

**Alternative diagnostic without HC-06 (eliminates Bluetooth variability):**  
Plug a USB-UART dongle (e.g. CH340, CP2102) into a PC USB port, connect dongle RX → Pico GPIO 0. Open the dongle's COM port at 115200 in RealTerm. This bypasses HC-06 entirely and is the fastest way to confirm whether UART is actually transmitting.

---

## Root Cause Identified: `tud_init()` changes `clk_peri` after UART is initialised

### Dual-core execution order

```
Core 0: setup()  →  [waits for Core 1 ready]  →  run() → tud_init()  →  tud_task() loop
Core 1:             setup() → [signals ready]  →  process() loop
```

- Core 1 calls `uart_init(uartInst, baudRate)` during its `setup()`.  
  At that point `clk_peri` is at the boot value (~48 MHz on RP2350).
- Core 0 then calls `tud_init()` inside `GP2040::run()`.  
  On RP2350, `tud_init()` → `dcd_init()` reconfigures `clk_peri` (typically to 120 MHz or USB-PLL value).
- The UART baud-rate divisor (IBRD/FBRD registers) is **not recalculated** — it now corresponds to the wrong clock, so the effective baud rate is wrong.

### Evidence

| Test | Observation | Interpretation |
|---|---|---|
| `uart_write_blocking("UART OK\r\n")` in `setup()` | Visible on COM5 ✅ | UART correct before `tud_init` |
| `uart_write_blocking(frame, 18)` in `process()` (blocking) | Hangs forever | FIFO never drains — UART stalled/too slow |
| Non-blocking writes in `process()` | HC-06 receives `0x00` bytes | UART transmitting at wrong baud, HC-06 interprets framing error as break |

---

## Fix Implemented (built, not yet confirmed working)

**File:** `src/addons/esp_uart_bridge.cpp`

In `process()`, before doing anything else, poll `tud_inited()`. Once it returns true, call `uart_set_baudrate()` to recalculate IBRD/FBRD for the current (post-tud_init) `clk_peri`. This does **not** reset or re-init the UART peripheral — it only corrects the divisor.

```cpp
void EspUartBridgeAddon::process() {
    if (!uartReady) {
        if (!tud_inited()) return;
        uart_set_baudrate(uartInst, baudRate);  // recalc divisor for new clk_peri
        gpio_set_function(txPin, GPIO_FUNC_UART);
        gpio_set_function(rxPin, GPIO_FUNC_UART);
        uartReady = true;
        const char* banner2 = "READY\r\n";
        uart_write_blocking(uartInst, (const uint8_t*)banner2, 7);
        uart_tx_wait_blocking(uartInst);
        return;
    }
    // ... send frame ...
}
```

**`uartReady` flag** added to `headers/addons/esp_uart_bridge.h` private section.

**Build status:** compiled cleanly (0 errors) as of 2026-05-31.  
**Flash/test status:** not yet attempted after this fix.

---

## Expected Test Sequence (next session)

1. Open COM5 (or USB-UART dongle COM port) at 115200 baud in RealTerm **before** plugging Pico in.
2. Plug Pico in while holding BOOTSEL to enter bootloader, drag UF2 to RPI-RP2 drive, Pico reboots.  
   **OR** just unplug/replug without BOOTSEL if already flashed.
3. Expected output:
   - `UART OK` — from `setup()`, before `tud_init` (proves UART hardware works at boot clock)
   - `READY` — from first `process()` after `tud_inited()` = true (proves baud fix worked)
   - Continuous binary frames: `A5 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F <xor> 5A`

---

## Decision Tree If Still Broken

| Symptom | Likely cause | Next step |
|---|---|---|
| Nothing at all (no `UART OK`) via HC-06 | HC-06 Bluetooth connection not established before data sent, or COM5 closed/wrong port | Try wired USB-UART dongle on GPIO 0 instead |
| Nothing via wired USB-UART dongle either | Build artifact or flash issue; OR `setup()` regressed | Re-flash, verify UF2 timestamp matches build; add `sleep_ms(2000)` before banner in `setup()` to give time to open port |
| `UART OK` but no `READY` | `tud_inited()` never returns true on Core 1 (non-volatile global `_usbd_rhport` compiler-cached); OR `uart_set_baudrate` call is hanging | Replace `tud_inited()` poll with `sleep_ms(2000)` as blunt sync instead |
| `UART OK` + `READY` but no frames | `uart_write_blocking(frame, 18)` still hanging post-fix | Switch frames to non-blocking: write byte-by-byte with `uart_is_writable()` guard |
| `UART OK` + `READY` + frames | **Fix confirmed.** Proceed to restore section below. | |

---

## Current File States (exact, as of end of session)

### `src/addons/esp_uart_bridge.cpp`
```cpp
#include "addons/esp_uart_bridge.h"
#include "storagemanager.h"
#include "helper.h"
#include "config.pb.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "tusb.h"

#define FRAME_START   0xA5
#define FRAME_END     0x5A
#define FRAME_SIZE    18

bool EspUartBridgeAddon::available() {
    return true;  // DEBUG: force enable
}

void EspUartBridgeAddon::setup() {
    // DEBUG: hardcoded UART0 / GPIO 0+1
    txPin = 0; rxPin = 1; baudRate = 115200; uartInst = uart0;
    uart_init(uartInst, baudRate);
    gpio_set_function(txPin, GPIO_FUNC_UART);
    gpio_set_function(rxPin, GPIO_FUNC_UART);
    const char* banner = "UART OK\r\n";
    uart_write_blocking(uartInst, (const uint8_t*)banner, 9);
    uart_tx_wait_blocking(uartInst);
}

void EspUartBridgeAddon::process() {
    if (!uartReady) {
        if (!tud_inited()) return;
        uart_set_baudrate(uartInst, baudRate);  // recalc divisor for new clk_peri
        gpio_set_function(txPin, GPIO_FUNC_UART);
        gpio_set_function(rxPin, GPIO_FUNC_UART);
        uartReady = true;
        const char* banner2 = "READY\r\n";
        uart_write_blocking(uartInst, (const uint8_t*)banner2, 7);
        uart_tx_wait_blocking(uartInst);
        return;
    }
    uint8_t frame[FRAME_SIZE];
    frame[0] = FRAME_START;
    for (int i = 1; i <= 15; i++) frame[i] = (uint8_t)i;
    uint8_t checksum = 0;
    for (int i = 1; i <= 15; i++) checksum ^= frame[i];
    frame[16] = checksum;
    frame[17] = FRAME_END;
    uart_write_blocking(uartInst, frame, FRAME_SIZE);
    uart_tx_wait_blocking(uartInst);
}
```

### `headers/addons/esp_uart_bridge.h` (private section)
```cpp
private:
    uart_inst_t* uartInst;
    uint32_t baudRate;
    int txPin;
    int rxPin;
    bool uartReady = false;
```

### `configs/Pico2EspBridge/BoardConfig.h` (ESP lines only)
```cpp
#define ESP_UART_BRIDGE_ENABLED    1
#define ESP_UART_BRIDGE_UART_BLOCK 0    // UART0 (diagnostic; restore to 1)
#define ESP_UART_BRIDGE_TX_PIN     0    // GPIO 0 (diagnostic; restore to 20)
#define ESP_UART_BRIDGE_RX_PIN     1    // GPIO 1 (diagnostic; restore to 21)
//#define ESP_UART_BRIDGE_BAUD       1000000
#define ESP_UART_BRIDGE_BAUD  115200   // HC-06 at 115200; revert to 1000000 for production
```

### `src/gp2040aux.cpp` (addon section)
```cpp
// DEBUG: all other addons commented out to isolate EspUartBridge
//addons.LoadAddon(new DisplayAddon());
//addons.LoadAddon(new NeoPicoLEDAddon());
//addons.LoadAddon(new PlayerLEDAddon());
//addons.LoadAddon(new BoardLedAddon());
//addons.LoadAddon(new BuzzerSpeakerAddon());
//addons.LoadAddon(new DRV8833RumbleAddon());
//addons.LoadAddon(new ReactiveLEDAddon());
addons.LoadAddon(new EspUartBridgeAddon());
```

---



| Item | Diagnostic value | Production value |
|---|---|---|
| `available()` | hardcoded `return true` | storage: `options.enabled && isValidPin(txPin) && isValidPin(rxPin)` |
| `setup()` pins | hardcoded GPIO 0 / UART0 | storage: `options.txPin` / `options.rxPin`; UART block from `options.uartBlock` |
| `setup()` baud | hardcoded 115200 | storage: `options.baudRate` (production target: 1 000 000) |
| `gp2040aux.cpp` | all other addons commented out | all addons re-enabled |
| `BoardConfig.h` UART_BLOCK | 0 (UART0) | 1 (UART1) |
| `BoardConfig.h` TX/RX | GPIO 0 / 1 | GPIO 20 / 21 |
| `BoardConfig.h` baud | 115200 | 1 000 000 |
| `process()` payload | test pattern 0x01–0x0F | real `GamepadState` fields |

---

## Restore Checklist (after fix confirmed)

- [ ] `available()` → read from `Storage::getInstance().getAddonOptions().espUartBridgeOptions`
- [ ] `setup()` → read uartBlock, txPin, rxPin, baudRate from storage
- [ ] `process()` payload → pack real `GamepadState` (buttons, axes, hat)
- [ ] Keep `uartReady` + `uart_set_baudrate` pattern — this is the **permanent fix**
- [ ] Re-enable all addons in `src/gp2040aux.cpp`
- [ ] `BoardConfig.h`: `UART_BLOCK=1`, `TX_PIN=20`, `RX_PIN=21`
- [ ] Move HC-06 RXD wire from GPIO 0 (pin 1) to GPIO 20 (physical pin 26, right side, 7th from top)
- [ ] Factory-reset flash (S1+S2+Up at boot) or set via web UI to pick up new defaults
- [ ] Eventually: `ESP_UART_BRIDGE_BAUD 1000000` for production ESP32 link speed

---

## Other Issues Encountered During Session

- **GPIO 25 not usable as debug LED on Pico2W** — reserved for CYW43 wireless chip. Use GPIO 15 or similar if a blink-debug LED is needed.
- **`tud_inited()` reads a non-volatile global** (`_usbd_rhport`) — there is a theoretical risk the compiler caches the value and the poll never sees it update on Core 1. If the `tud_inited()` approach fails, replace with `sleep_ms(2000)` as a blunt sync.
- **useSystemStats.ts bug** (fixed) — GitHub API call was coupled into local stats fetch, causing web UI errors in environments without internet. Decoupled; local stats now fetched independently.
- **HC-06 AT command mode** — only active when no Bluetooth connection is present. AT commands must be sent without line endings at 9600 baud (default) or at the current baud if already changed.
