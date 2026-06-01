# ESP UART Bridge — Troubleshooting Reference

**Applies to:** `configs/Pico2EspBridge` — UART1 on GPIO 20 (TX) / GPIO 21 (RX) at 1,000,000 baud

---

## Known Root Cause: `tud_init()` reconfigures `clk_peri`

This is the most important thing to understand about this addon. **Do not remove the `uartReady` / `uart_set_baudrate` block in `process()` without reading this.**

### What happens

```
Core 0: setup()  →  [waits for Core 1 ready]  →  run() → tud_init()  →  tud_task() loop
Core 1:             setup() → [signals ready]  →  process() loop
```

- Core 1 calls `uart_init(uartInst, baudRate)` during `setup()`. At that point `clk_peri` is at the RP2350 boot value (~48 MHz).
- Core 0 then calls `tud_init()` inside `GP2040::run()`. On RP2350, `tud_init()` → `dcd_init()` reconfigures `clk_peri` to the USB-PLL value (~120 MHz).
- The UART baud-rate divisor (IBRD/FBRD registers) is **not recalculated** — it now corresponds to the wrong clock, making the effective baud rate completely wrong.

### Symptoms if the fix is absent or removed

| Symptom | Interpretation |
|---|---|
| `uart_write_blocking()` in `process()` hangs forever | FIFO never drains — baud rate so wrong UART stalls |
| Receiver gets `0x00` bytes only | Framing errors at wrong baud interpreted as break |
| Data visible in `setup()` but not `process()` | `setup()` runs before `tud_init()` changes the clock; `process()` runs after |

### The fix (permanent — in `process()`)

```cpp
if (!uartReady) {
    if (!tud_inited()) return;
    uart_set_baudrate(uartInst, baudRate);  // recalc IBRD/FBRD for new clk_peri
    gpio_set_function(txPin, GPIO_FUNC_UART);
    gpio_set_function(rxPin, GPIO_FUNC_UART);
    uartReady = true;
    return;
}
```

`uart_set_baudrate()` does not reset the UART peripheral — it only corrects the divisor registers. `uartReady` ensures this runs exactly once.

---

## Troubleshooting: No frames on receiver

### Step 1 — Verify with CH340 on GPIO 20 first

Before suspecting the ESP32, confirm the Pico is transmitting. Wire a CH340 RX → GPIO 20, open RealTerm at **1,000,000** baud. You should see continuous frames immediately.

See [monitor_serial.md](monitor_serial.md) for full wiring and frame decode instructions.

### Step 2 — Decision tree

| Symptom | Likely cause | Action |
|---|---|---|
| Nothing at all on CH340 | Firmware not transmitting | Check UF2 was flashed after last build; verify `ESP_UART_BRIDGE_ENABLED=1` in storage (factory reset with S1+S2+Up if needed) |
| Garbage on CH340 | Wrong baud rate | Confirm RealTerm is set to exactly 1000000 |
| Frames on CH340, nothing on ESP32 | Wiring or ESP32 baud mismatch | Check GPIO 20 → ESP32 RX wire; confirm `Serial2.begin(1000000, ...)` in ESP32 sketch |
| Frames on ESP32 but checksum fails | Frame sync lost | ESP32 parser must sync on `0xA5` start byte before reading — do not assume stream starts on a frame boundary |
| `tud_inited()` never returns true | Compiler cached `_usbd_rhport` global | Replace `tud_inited()` poll with `sleep_ms(3000)` as a blunt one-time sync |
| Frame rate unexpectedly low | `uart_write_blocking` stalling | `clk_peri` fix may have been removed — add it back |

---

## Hardware Gotchas

- **GPIO 25 cannot be used as a debug LED on Pico 2W** — it is reserved for the CYW43 wireless chip. Use GPIO 15 instead.
- **GPIO 26 / 27 are UART1 CTS/RTS (flow control), not TX/RX** — they cannot carry UART data. TX/RX are GPIO 20 / 21 (function F2).
- **Factory reset required after reflash if BoardConfig.h pin assignments change** — `INIT_UNSET_PROPERTY` only seeds values that are not already set in flash. Hold S1+S2+Up at boot to wipe and re-seed from BoardConfig.h defaults.
- **HC-06 is unsuitable for monitoring at 1 Mbaud** — SPP profile caps at 115200 baud. Use a CH340 dongle wired directly to GPIO 20. See [monitor_serial.md](monitor_serial.md#why-not-hc-06-for-monitoring).

