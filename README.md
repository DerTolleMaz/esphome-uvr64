# uvr64_dlbus for ESPHome

Custom ESPHome component to read data from a UVR64 controller via DL-Bus using UART.

## DL-Bus electrical interface

The DL-Bus used by the UVR64 controller operates around **15–24 V DC**. The levels are not compatible with a 3.3 V UART interface, so the bus must **not** be connected directly to the ESP32's RX pin. Use galvanic isolation (for example an optocoupler) and limit the bus current via a resistor.

Obenansicht (Kerbe oben, Pins nach unten)
```
   Obenansicht (Kerbe oben, Pins nach unten)

       _________
      |         |
   1 -|●        |- 4
   2 -|         |- 3
      |_________|

       PC817 DIP-4
```
| Pin | Name          | Funktion             | Anschluss in deiner Schaltung                 |
| --- | ------------- | -------------------- | --------------------------------------------- |
| 1   | **Anode**     | LED + (Eingang)      | DL-Bus-Signal über 1 kΩ Widerstand            |
| 2   | **Kathode**   | LED –                | DL-Bus GND                                    |
| 3   | **Collector** | Transistor-Ausgang + | ➝ D1 Mini RX (GPIO3) + Pull-Up 10 kΩ zu 3.3 V |
| 4   | **Emitter**   | Transistor-Ausgang – | ➝ GND vom D1 Mini                             |



A simple interface can look like this:

```
DL-Bus (+15–24 V)
     |
     | 
    [1kΩ Vorwiderstand]              
     |
     |        PC817 (Ansicht von oben, PIN-Nr.)
     |        +------------------+
     |        |      4 (Emitter) |------------> GND (D1 Mini)
     |        |                  |
     +------->| 1 (Anode)        |       [10kΩ Pull-Up]
              |                  |        +----[R]----+
DL-Bus GND -->| 2 (Kathode)      |       |           |
              |                  |      [5]         3.3 V
              |                  |       |           |
              |     3 (Kollektor)|-------+----> RX (GPIO3, D1 Mini)
              |                  |
              +------------------+
                     
                     
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
```
