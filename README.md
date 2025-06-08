# TA DL-Bus Protocol – Explanation

This file describes the **DL-Bus protocol** used by *Technische Alternative (TA)*, for example in the **UVR64** controller. This protocol allows the **reading of temperatures and relay states** via a serial line.

---

## 📡 Characteristics of the DL-Bus

| Property              | Value                               |
|-----------------------|-------------------------------------|
| Signal type           | Serial (UART-like)                  |
| Baud rate             | 2400 baud                           |
| Signal level          | ~5 V, not TTL compatible            |
| Direction             | Unidirectional (UVR ➝ receiver)     |
| Frame length          | Fixed: 22 bytes (UVR64)             |
| Checksum              | 1-byte additive checksum            |
| Isolation recommended | Yes, galvanic (e.g., with PC817)    |

---

## 🧱 Structure of a Frame

A complete frame on the UVR64 looks like this:

| Byte | Content                    | Description                          |
|------|----------------------------|--------------------------------------|
| 0    | `0xAA`                     | Start byte 1                         |
| 1    | `0x10`                     | Start byte 2                         |
| 2–13 | Temperature data           | 6 × 16-bit (big endian, in 0.1 °C)   |
| 14–19| Status data                | Mostly unused / reserved             |
| 20   | Relay status               | 1 byte, bit-encoded (8 relays)       |
| 21   | Checksum                   | Additive checksum of bytes 0–20      |

### Example: Temperature values

A temperature value consists of two bytes:
```cpp
uint16_t raw = (data[5] << 8) | data[6];
float temp = raw * 0.1;  // e.g., 253 → 25.3 °C
```

### Example: Relay status

The relay status is bit-encoded:
```cpp
bool relay1 = (data[20] >> 0) & 0x01;
bool relay2 = (data[20] >> 1) & 0x01;
// ...
```

---

## ✅ Implementation Notes

- Use an **optocoupler** (e.g., PC817) to isolate the ESP or D1 Mini from the DL-Bus.
- Make sure your UART runs at 2400 baud.
- Start bytes and checksum must be respected during parsing.
- The DL-Bus sends data regularly – no polling is required.

---

## 🧪 Common Issues

- No galvanic isolation → risk of damage to the microcontroller
- Wrong start bytes (e.g., for UVR16x)
- Wrong UART baud rate
- Checksum errors from truncated frames

---

## 🔗 Further Resources

- The protocol is **not officially documented** by TA, but has been well analyzed by the community:
    - [DL-Bus protocol analysis on openTA](https://open-ta.org)
    - [TA-DL-Bus GitHub projects](https://github.com/search?q=ta+dlbus)
