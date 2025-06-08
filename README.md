# uvr64_dlbus for ESPHome

Custom ESPHome component to read data from a UVR64 controller via DL-Bus using UART.

## DL-Bus electrical interface

# TA DL-Bus Protocol – Explanation

This file describes the **DL-Bus protocol** from *Technische Alternative (TA)*, as used for example in the **UVR64** controller. This protocol enables the **reading of temperature values and relay states** over a serial line.

---

## 📡 DL-Bus Characteristics

| Property            | Value                               |
|---------------------|-------------------------------------|
| Signal type         | Serial (UART-like)                  |
| Baud rate           | 2400 baud                           |
| Signal level        | ~5 V, not TTL-compatible            |
| Direction           | Unidirectional (UVR ➝ receiver)     |
| Frame length        | Fixed: 22 bytes (UVR64)             |
| Checksum            | 1-byte additive checksum            |
| Isolation recommended | Yes, galvanic (e.g., with PC817)  |

---

## 🧱 Frame Structure

A complete frame from the UVR64 typically looks like this:

| Byte | Content                    | Description                            |
|------|----------------------------|----------------------------------------|
| 0    | `0xAA`                     | Start byte 1                           |
| 1    | `0x10`                     | Start byte 2                           |
| 2–13 | Temperature data           | 6 × 16-bit (big endian, each in 0.1 °C)|
| 14–19| Status data                | mostly unused / reserved               |
| 20   | Relay status               | 1 byte, bit-encoded (8 relays)         |
| 21   | Checksum                   | Additive checksum over bytes 0–20      |

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
- Ensure your UART runs at 2400 baud.
- Start bytes and checksum must be respected during parsing.
- The DL-Bus transmits data continuously – no polling is needed.

---

## 🧪 Common Issues

- No galvanic isolation → damage to the microcontroller
- Wrong start bytes (e.g., for UVR16x)
- UART set to wrong baud rate
- Checksum errors due to incomplete frames

---

## 🔗 Additional References

- The protocol is **not officially documented** by TA but has been well analyzed by community projects like:
  - [DL-Bus protocol analysis on openTA](https://open-ta.org)
  - [TA-DL-Bus GitHub projects](https://github.com/search?q=ta+dlbus)

---

The DL-Bus used by the UVR64 controller operates around **15–24 V DC**. The levels are not compatible with a 3.3 V UART interface, so the bus must **not** be connected directly to the ESP32's RX pin. Use galvanic isolation (e.g., an optocoupler) and limit the bus current via a resistor.

Top view (notch up, pins downward):
```
       _________
      |         |
   1 -|●        |- 4
   2 -|         |- 3
      |_________|

       PC817 DIP-4
```

| Pin | Name          | Function             | In your circuit                        |
| --- | ------------- | -------------------- |----------------------------------------|
| 1   | **Anode**     | LED + (input)        | DL-Bus signal via 1 kΩ resistor        |
| 2   | **Cathode**   | LED –                | DL-Bus GND                             |
| 3   | **Collector** | Transistor output +  | ➝ D1 Mini RX (GPIO3) + 10 kΩ Pull-Up to 3.3 V |
| 4   | **Emitter**   | Transistor output –  | ➝ GND of the D1 Mini                   |

A simple interface can look like this:

```
DL-Bus (+15–24 V)
     |
     | 
    [1kΩ Resistor]              
     |
     |        PC817 (Top view, pin numbers)
     |        +------------------+
     |        |      4 (Emitter) |------------> GND (D1 Mini)
     |        |                  |
     +------->| 1 (Anode)        |       [10kΩ Pull-Up]
              |                  |        +----[R]----+
DL-Bus GND -->| 2 (Cathode)      |       |           |
              |                  |      [3]         3.3 V
              |                  |       |           |
              |     3 (Collector)|-------+----> RX (GPIO3, D1 Mini)
              |                  |
              +------------------+
```

* The input resistor limits the current through the LED of the optocoupler.
* The optocoupler's output transistor pulls the ESP32 RX line low when the bus is active.
* Add a pull-up resistor (e.g., 10 kΩ) from ESP32 RX to 3.3 V.

## Example ESPHome configuration

```yaml
uart:
  id: dl_uart
  rx_pin: GPIO16
  baud_rate: 9600

sensor:
  - platform: uvr64_dlbus
    uart_id: dl_uart
    temperatures:
      - name: "UVR64 Temp 1"
      - name: "UVR64 Temp 2"
      - name: "UVR64 Temp 3"
      - name: "UVR64 Temp 4"
      - name: "UVR64 Temp 5"
      - name: "UVR64 Temp 6"
    relays:
      - name: "UVR64 Relay 1"
      - name: "UVR64 Relay 2"
      - name: "UVR64 Relay 3"
      - name: "UVR64 Relay 4"
```
