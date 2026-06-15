# Button Bits Mapping for Pico2EspBridge

The UART frame format transmits gamepad state in a 18-byte binary frame. This document maps each button to its bit position within the frame.

---

## Frame Structure Reference

| Byte(s) | Field      | Type     | Notes                        |
|---------|------------|----------|------------------------------|
| 0       | Start      | `0xA5`   | Magic byte                   |
| 1–4     | buttons    | uint32   | Button bitmask (LE)          |
| 5       | dpad       | uint8    | D-pad bitmask                |
| 6–7     | lx         | uint16   | Left stick X (LE)            |
| 8–9     | ly         | uint16   | Left stick Y (LE)            |
| 10–11   | rx         | uint16   | Right stick X (LE)           |
| 12–13   | ry         | uint16   | Right stick Y (LE)           |
| 14      | lt         | uint8    | Left trigger                 |
| 15      | rt         | uint8    | Right trigger                |
| 16      | checksum   | uint8    | XOR of bytes 1–15            |
| 17      | End        | `0x5A`   | Magic byte                   |

---

## D-Pad Buttons (Byte 5)

D-pad inputs are stored in a separate byte at offset **5** in the frame. These buttons use bits 0–3:

| Button | Bit | Hex Value | Byte 5 Bit Position |
|--------|-----|-----------|---------------------|
| UP     | 0   | 0x01      | Bit 0               |
| DOWN   | 1   | 0x02      | Bit 1               |
| LEFT   | 2   | 0x04      | Bit 2               |
| RIGHT  | 3   | 0x08      | Bit 3               |

**Example:** If UP is pressed and all other d-pad buttons are released:
- Byte 5 = `0x01` (binary: 0000 0001)

**Example:** If UP and RIGHT are pressed:
- Byte 5 = `0x09` (binary: 0000 1001)

---

## Face & Shoulder Buttons (Bytes 1–4)

Face and shoulder buttons are stored in a 32-bit little-endian (LE) value across bytes 1–4. Bit positions are counted from LSB (bit 0) within the combined 32-bit value.

| Button | Bit | Hex Value | Byte Offset in Frame |
|--------|-----|-----------|----------------------|
| B1     | 0   | 0x0001    | Byte 1 (LSB)         |
| B2     | 1   | 0x0002    | Byte 1               |
| B3     | 2   | 0x0004    | Byte 1               |
| B4     | 3   | 0x0008    | Byte 1               |
| L1     | 4   | 0x0010    | Byte 2               |
| R1     | 5   | 0x0020    | Byte 2               |
| L2     | 6   | 0x0040    | Byte 2               |
| R2     | 7   | 0x0080    | Byte 2               |
| S1     | 8   | 0x0100    | Byte 3               |
| S2     | 9   | 0x0200    | Byte 3               |
| L3     | 10  | 0x0400    | Byte 3               |
| R3     | 11  | 0x0800    | Byte 3               |

**Example:** If S1 is pressed (bit 8):
- Bytes 1–4 in little-endian: `0x00 0x01 0x00 0x00`
  - Byte 1 = 0x00
  - Byte 2 = 0x01 (bit 8 occupies the upper byte)
  - Byte 3 = 0x00
  - Byte 4 = 0x00

**Example:** If S2 is pressed (bit 9):
- Bytes 1–4: `0x00 0x02 0x00 0x00`

**Example:** If B1 and S1 are pressed (bits 0 and 8):
- Bytes 1–4: `0x01 0x01 0x00 0x00`

---

## Complete Example Frame

**Scenario:** UP and S1 pressed, all sticks centered, triggers released

```
Byte  Hex   Description
  0   A5    Frame start
  1   01    B1..B4 & L1/R1 (byte 1) → B1 pressed (bit 0) = 0x01
  2   01    L2/R2 & S1/S2 (byte 2) → S1 pressed (bit 8, appears here) = 0x01
  3   00    L3/R3 (byte 3)
  4   00    Additional bits (byte 4)
  5   01    D-pad → UP pressed (bit 0) = 0x01
  6   FF    lx LSB (0x7FFF center, LSB)
  7   7F    lx MSB
  8   FF    ly LSB
  9   7F    ly MSB
  10  FF    rx LSB
  11  7F    rx MSB
  12  FF    ry LSB
  13  7F    ry MSB
  14  00    lt (0x00 released)
  15  00    rt (0x00 released)
  16  FB    checksum (XOR of bytes 1–15)
  17  5A    Frame end
```

---

## Quick Reference Table

| Button | Storage Location | Bit | Hex Value |
|--------|------------------|-----|-----------|
| UP     | Byte 5 (dpad)    | 0   | 0x01      |
| DOWN   | Byte 5 (dpad)    | 1   | 0x02      |
| LEFT   | Byte 5 (dpad)    | 2   | 0x04      |
| RIGHT  | Byte 5 (dpad)    | 3   | 0x08      |
| B1     | Bytes 1–4 (buttons) | 0   | 0x0001    |
| B2     | Bytes 1–4 (buttons) | 1   | 0x0002    |
| B3     | Bytes 1–4 (buttons) | 2   | 0x0004    |
| B4     | Bytes 1–4 (buttons) | 3   | 0x0008    |
| L1     | Bytes 1–4 (buttons) | 4   | 0x0010    |
| R1     | Bytes 1–4 (buttons) | 5   | 0x0020    |
| L2     | Bytes 1–4 (buttons) | 6   | 0x0040    |
| R2     | Bytes 1–4 (buttons) | 7   | 0x0080    |
| S1     | Bytes 1–4 (buttons) | 8   | 0x0100    |
| S2     | Bytes 1–4 (buttons) | 9   | 0x0200    |
| L3     | Bytes 1–4 (buttons) | 10  | 0x0400    |
| R3     | Bytes 1–4 (buttons) | 11  | 0x0800    |