# uvr64_dlbus for ESPHome

Custom ESPHome component to read data from a UVR64 controller via DL-Bus using UART.

## DL-Bus electrical interface

The DL-Bus used by the UVR64 controller operates around **15–24 V DC**. The levels are not compatible with a 3.3 V UART interface, so the bus must **not** be connected directly to the ESP32's RX pin. Use galvanic isolation (for example an optocoupler) and limit the bus current via a resistor.

A simple interface can look like this:

```
DL-Bus + -----[2.2kΩ]---->|----+------- ESP32 RX (pulled up to 3.3 V)
                optocoupler   |
DL-Bus - ---------------------+---- ESP32 GND
```

* The input resistor limits the LED current of the optocoupler.
* The optocoupler output transistor pulls the ESP32 RX line low when the bus is active.
* Add a pull‑up resistor (e.g. 10 kΩ) from ESP32 RX to 3.3 V.

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
      - name: "UVR64 Relay 5"
      - name: "UVR64 Relay 6"
      - name: "UVR64 Relay 7"
      - name: "UVR64 Relay 8"
```
