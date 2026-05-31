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

- **PuTTY**: Connection type = *Serial*, Speed = `115200`, then go to *Terminal* and enable *Implicit CR in every LF*.  
  Use a hex viewer plugin or RealTerm instead if you need raw hex display.
- **RealTerm**: Port = `COM7`, Baud = `115200`, Display = *Hex[space]*.

You should see `A5`-prefixed 18-byte frames streaming continuously.

### Step 5 — Revert baud rate

```c
#define ESP_UART_BRIDGE_BAUD  1000000
```

Rebuild and reflash before connecting to the real ESP32.
