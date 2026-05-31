# Monitoring UART Serial — Pico2EspBridge

The Pico2EspBridge firmware transmits 18-byte frames on **GPIO 20 (UART1 TX)** at **1,000,000 baud** (1 Mbaud), 8N1.

---

## Option A — ESP32 (target hardware)

1. Wire:
   | Pico2 | ESP32 |
   |---|---|
   | GPIO 20 (TX, physical pin 26) | any GPIO configured as UART RX (e.g. GPIO 16) |
   | GND | GND |

2. In your ESP32 sketch, initialise the UART at 1 Mbaud:
   ```cpp
   Serial2.begin(1000000, SERIAL_8N1, /*RX=*/16, /*TX=*/17);
   ```

3. In `loop()`, read and print raw bytes:
   ```cpp
   while (Serial2.available()) {
       Serial.printf("%02X ", Serial2.read());
   }
   ```

4. Open the Arduino Serial Monitor at **115200** (the `Serial` debug port rate) and watch the hex stream.  
   Each valid frame starts with `0xA5` and ends with its XOR checksum byte.

---

## Option B — HC-06 Bluetooth module (monitoring only, not for production)

> **Limitation:** HC-06 hardware tops out at **115200 baud** by default. You must temporarily lower the firmware baud rate for this test and revert it before final use.

### Step 1 — Temporarily reduce baud rate

In [configs/Pico2EspBridge/BoardConfig.h](../../configs/Pico2EspBridge/BoardConfig.h), change:

```c
#define ESP_UART_BRIDGE_BAUD  115200   // temporary — revert to 1000000 before production
```

Rebuild and reflash the Pico2.

### Step 2 — Wire HC-06

| Pico2 | HC-06 |
|---|---|
| GPIO 20 (TX, physical pin 26) | RXD |
| 3.3 V or 5 V (check module) | VCC |
| GND | GND |

> HC-06 TX does not need to be connected for one-way monitoring.

### Step 3 — Pair HC-06 with Windows

1. Power the HC-06 — LED blinks rapidly (unpaired).
2. Open **Settings → Bluetooth** and pair `HC-06` (default PIN: `1234`).
3. Note the assigned COM port (e.g. `COM7`) under **Device Manager → Ports**.

### Step 4 — Open serial monitor

Use any hex-capable terminal at **115200 8N1**:

- **Arduino IDE 2.x** (easiest if already installed): Tools → Port → select the HC-06 COM port → Tools → Serial Monitor. Set baud to `115200` and use the **HEX** toggle in the monitor toolbar. The link stays alive as long as the monitor is open.
- **PuTTY**: Connection type = *Serial*, Speed = `115200`, then go to *Terminal* and enable *Implicit CR in every LF*.  
  Use a hex viewer plugin or RealTerm instead if you need raw hex display.
- **RealTerm**: Port = `COM7`, Baud = `115200`, Display = *Hex[space]*.

> **Tip:** Open the serial monitor *before* powering the Pico — this ensures the COM port is held open and the Bluetooth link stays connected.

You should see `A5`-prefixed 18-byte frames streaming continuously.

### Step 5 — Revert baud rate

```c
#define ESP_UART_BRIDGE_BAUD  1000000
```

Rebuild and reflash before connecting to the real ESP32.

---

## Troubleshooting — HC-06 LED goes dark / cannot connect

### LED turns off after a few seconds

| Symptom | Likely cause | Fix |
|---|---|---|
| LED blinks, then goes completely off | Power brown-out | Use **VBUS (5 V, physical pin 40)** instead of 3.3 V for HC-06 VCC. Many HC-06 modules have an onboard 3.3 V regulator and require 5 V input. |
| LED blinks briefly, then off | Baud rate mismatch locking up HC-06 | Confirm Step 1 was done — the firmware **must** be rebuilt at 115200 before connecting HC-06 to the TX line. |
| LED never blinks at all | No power reaching module | Check wiring; the Pico 3.3 V rail can only source ~300 mA — if other peripherals are attached, use VBUS. |

### "Paired" but not connected — shows only as paired after brief connection

This is normal Windows SPP (Serial Port Profile) behavior.  
"Paired" = credentials stored. "Connected" = an application is actively holding the COM port open.  
The HC-06 drops to standby within a few seconds once the port is closed or was never opened.

**The fix: keep RealTerm (or PuTTY) open with the COM port selected *before* you expect data.**  
Do not close the terminal between tests — re-opening it re-initiates the Bluetooth connection.

If the connection drops *while* the port is open:
- The Pico may still be transmitting at 1 Mbaud (check Step 1 was done and firmware rebuilt).
- Windows may have auto-suspended the Bluetooth adapter — disable **power management** for the Bluetooth adapter: Device Manager → Bluetooth adapter → Properties → Power Management → uncheck *Allow the computer to turn off this device to save power*.
- Try a shorter USB cable to the Pico to rule out USB power sag affecting the 3.3 V / VBUS rails.

### Paired but cannot open COM port

After pairing, the HC-06 LED will **blink slowly** (not solid) until a host application opens the COM port.

1. Open **Device Manager → Ports (COM & LPT)** and find the `HC-06` or `Standard Serial over Bluetooth link` entry — note the port number (e.g. `COM7`).  
   > If no port appears, right-click the HC-06 in Bluetooth settings → **More Bluetooth options → COM Ports tab** and add an outgoing port manually.
2. Open RealTerm (or PuTTY) at **115200 8N1** on that COM port.  
   The HC-06 LED should go **solid** when the port is successfully opened.
3. If connection immediately drops, the HC-06 may be paired to a stale entry — remove it from Bluetooth settings, power-cycle the module, and re-pair.

### Connected but no output in serial monitor

Check in this order:

1. **Firmware baud rate** — confirm Step 1 was done, the firmware was **rebuilt** (`ninja -C build`) and **reflashed** after changing `ESP_UART_BRIDGE_BAUD` to `115200`. If the Pico is still transmitting at 1 Mbaud, the HC-06 forwards nothing (framing errors).

2. **Wiring** — GPIO 20 (physical pin 26) must be connected to HC-06 **RXD**. A loose or missing wire here gives a connected-but-silent result.

3. **USB gamepad not attached** — the bridge only emits frames when a USB HID gamepad is connected upstream to the Pico. Plug in a controller and press buttons; you should see data appear.

4. **Wrong COM port** — Windows creates two SPP ports for HC-06. You need the **Outgoing** one:  
   Bluetooth settings → *More Bluetooth options* → **COM Ports** tab.  
   Use the port listed as *Direction: Outgoing* (e.g. `COM5 Outgoing`). The Incoming port will connect but receive nothing.

5. **Multimeter check** — with a meter on GPIO 20 vs GND, you should see voltage toggling rapidly (~1–2 V average) while the Pico sends. A steady 3.3 V means no transmission.
