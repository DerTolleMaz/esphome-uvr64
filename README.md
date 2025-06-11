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

# TA DL-Bus Protocol – Technical Signal Description

This document summarizes how the **DL-Bus signal transmission** works as implemented by *Technische Alternative* in controllers like the **UVR64**.

---

## 🔁 Continuous Frame Transmission

- The controller continuously transmits **data frames in a loop** on the data line.
- To **detect the start** of each frame, a **SYNC sequence of 16 High bits** is sent before the first data byte.

---

## 🔀 XOR with Display Clock

- Before data is output to the bus, it is **XOR-modulated** with a square wave signal called the **display clock**:
  - Either **50 Hz** or **488 Hz**, depending on the controller type.
- This is used to **power bus receivers** from the signal itself.
- The signal on the line is **inverted** by the output transistor.

### 📌 Synchronization:
> If the receiver is synchronized to the display clock, the correct bit value can be read **during the second half-period** of each data bit.

---

## ⏱️ Timing by Controller Type

| Controller Type        | Display Clock | Bit Duration | Frame Duration |
|------------------------|----------------|--------------|----------------|
| UVR31, UVR42, UVR64, HZR65, EEG30, TFM66 | 50 Hz         | 20 ms         | 1.9–3.1 sec     |
| UVR1611, UVR61-3       | 488 Hz          | 2.048 ms     | ~0.75–1.35 sec |

> The total duration is determined by: SYNC + number of data bytes × bit duration

---

## 🔄 Bitstream and Logic

- The DL-Bus uses **XOR modulation** with a square wave to encode the data.
- **Inversion** is applied via an **output transistor**.
- Receivers should **sample in the second half** of each data bit to correctly interpret the signal.

---

## 🧠 Implications for ESP / UART reading

To successfully read DL-Bus data with a microcontroller (e.g. ESP32):

- UART must be set to **2400 baud**
- A **buffer** must store an entire frame (~22 bytes for UVR64)
- A **SYNC pattern (e.g., 0xFF x 2)** must be used to identify frame starts
- Signal must be **filtered or decoded** if XOR-modulated raw signal is read directly
- A proper **optocoupler circuit with pull-up** is essential for safe and clean signal reading

---

## ✅ Summary

| Property                     | Value / Note                                |
|------------------------------|---------------------------------------------|
| UART Baud Rate               | 2400 baud                                   |
| Signal Encoding              | XOR with Display Clock                      |
| Display Clock Frequency      | 50 Hz or 488 Hz                             |
| Bit Polarity                 | Inverted (via transistor)                   |
| Frame Sync                   | 16 High bits                                |
| Frame Size (UVR64)           | 22 bytes                                    |

---

For reliable decoding, either:
- Use a **hardware-decoded signal** (e.g. output from a TA module), or
- Decode the **XOR timing pattern** in firmware


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
  rx_pin: RX
  baud_rate: 2400

external_components:
  - source:
      type: git
      url: https://github.com/DerTolleMaz/esphome-uvr64
    components: [uvr64_dlbus]


sensor:
  - platform: uvr64_dlbus
    uart_id: dl_uart
    decode_xor: true
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
