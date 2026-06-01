# Monitoring UART Serial — Pico2EspBridge

The Pico2EspBridge firmware transmits 18-byte frames on **GPIO 20 (UART1 TX)** at **1,000,000 baud** (1 Mbaud), 8N1.

> **Monitoring recommendation:** Use a USB-UART dongle (CH340, CP2102, FT232) wired directly to GPIO 20. Do **not** use HC-06 for monitoring — see [Why not HC-06?](#why-not-hc-06-for-monitoring) below.

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

## Option B — CH340 / USB-UART dongle (recommended for development)

Any common USB-UART adapter works: CH340, CP2102, FT232RL. The CH340 in particular supports up to 2 Mbaud, well above the 1 Mbaud frame rate.

### Step 1 — Wire

| Pico2 | CH340 dongle |
|---|---|
| GPIO 20 (TX, physical pin 26) | RX |
| GND (any) | GND |

> TX (dongle → Pico) does not need to be connected for one-way monitoring.

**How to find physical pin 26:** on the right side of the Pico2, count up from the bottom — the order is GPIO 16, 17, GND, 18, 19, **20** (6th position). Connect to the 6th pin on that side.

### Step 2 — Open serial monitor

1. Plug the CH340 into a PC USB port — it appears as a COM port in Device Manager immediately, no pairing needed.
2. Open **RealTerm**: Port = the CH340 COM port, Baud = **1000000** (type it manually — it won't be in the dropdown), Display = *Hex[space]*.
3. Unplug and replug the Pico2 (no BOOTSEL needed if already flashed).

> **Important:** At 1 Mbaud the frame rate is extremely high. Do **not** try to read the data from the RealTerm display window — it will freeze or crash the app. Use file capture instead:
>
> 1. Go to the **Capture** tab
> 2. Set a file path (e.g. `c:\tmp\capture.txt`)
> 3. Check **Capture as Hex**
> 4. Click **Start: Overwrite**
> 5. Press the buttons you want to test on the controller
> 6. Click **Stop Capture** after 1–2 seconds
> 7. Open the file in a text editor — you will see the hex stream with your button presses embedded

You should see continuous 18-byte frames streaming at high speed:

```
A5 xx xx xx xx xx FF 7F FF 7F FF 7F FF 7F 00 00 xx 5A  ← idle (no buttons)
```

Press a button and watch the bytes change in real time.

### Step 3 — Interpret a frame

```
Offset  Bytes  Field
  0      1     0xA5 — frame start
  1–4    4     buttons (uint32 LE) — bit 0=UP, bit 8=S1, bit 9=S2, etc.
  5      1     dpad — bit 0=UP, bit 1=DOWN, bit 2=LEFT, bit 3=RIGHT
  6–7    2     lx (uint16 LE) — 0x7FFF = center
  8–9    2     ly (uint16 LE) — 0x7FFF = center
  10–11  2     rx (uint16 LE) — 0x7FFF = center
  12–13  2     ry (uint16 LE) — 0x7FFF = center
  14     1     lt — left trigger (0–0xFF)
  15     1     rt — right trigger (0–0xFF)
  16     1     XOR checksum of bytes 1–15
  17     1     0x5A — frame end
```

**Idle (no buttons, joysticks centered):**
```
A5 00 00 00 00 00 FF 7F FF 7F FF 7F FF 7F 00 00 00 5A
```

**UP pressed** — dpad byte (offset 5), bit 0:
```
A5 00 00 00 00 01 FF 7F FF 7F FF 7F FF 7F 00 00 01 5A
                ^^                                 ^^ checksum
```

**S1 pressed** — buttons bit 8 = 0x0100, appears in offset 2:
```
A5 00 01 00 00 00 FF 7F FF 7F FF 7F FF 7F 00 00 01 5A
      ^^                                           ^^ checksum
```

**S2 pressed** — buttons bit 9 = 0x0200, appears in offset 2:
```
A5 00 02 00 00 00 FF 7F FF 7F FF 7F FF 7F 00 00 02 5A
      ^^                                           ^^ checksum
```

> **Tip:** RealTerm file capture is continuous — if you capture while no buttons are held you will only see idle frames. Hold a button for a second while capturing and you will see the relevant byte change mid-stream.

### Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| No COM port appears | CH340 driver not installed | Install CH340 driver from manufacturer site |
| Garbage / no frames | Wrong baud rate | Confirm 1000000 exactly — RealTerm accepts manual entry |
| Frames visible but no button response | Factory reset not done | Hold S1+S2+Up at boot to re-seed storage from BoardConfig.h defaults |
| Steady 3.3 V on GPIO 20 | Firmware not transmitting | Verify UF2 was flashed after latest build |

---

## Why not HC-06 for monitoring?

The HC-06 is useful as a wireless transport for the production link to a PC but is a poor monitoring tool for this UART stream. Here is why:

| Problem | Detail |
|---|---|
| **Baud rate ceiling** | HC-06 SPP profile maxes out at 115200 baud via AT commands. The production firmware runs at **1,000,000 baud** — 8.7× faster. To use HC-06 you must rebuild the firmware at 115200, monitor, then rebuild again at 1 Mbaud. This is error-prone and tests a different configuration than production. |
| **SPP buffering and latency** | Bluetooth Serial Port Profile (SPP) does not forward bytes one-by-one. The HC-06 buffers incoming UART data and sends it in Bluetooth packets when a threshold is reached or a timer fires. You see bursts, not a real-time stream — making it hard to correlate button presses with frame changes. |
| **Connection must be established before data arrives** | HC-06 only forwards data when a host application holds the COM port open and the Bluetooth link is active. The Pico starts streaming immediately on boot, so any frames sent in the first ~2 seconds (before Windows establishes the SPP link) are silently dropped. |
| **Two extra failure modes** | HC-06 adds Bluetooth pairing state and Windows COM port assignment as additional points of failure, neither of which exists in the final Pico-to-ESP32 wired path. Debugging "no data" becomes ambiguous: is it the firmware, the Bluetooth link, or the COM port? |
| **Not the production topology** | The real link is wired UART between Pico GPIO 20 and ESP32. Monitoring via HC-06 inserts a component that is not in the final design and masks any wiring or baud-rate issues that would appear with the ESP32. |

**Summary:** use HC-06 only when you specifically need to validate the Bluetooth path to a Windows host. For all UART frame debugging, use a CH340 dongle wired directly to GPIO 20.
