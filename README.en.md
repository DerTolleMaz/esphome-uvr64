**Language:** [Deutsch](README.md)

# UVR64 DL-Bus for ESPHome

This component reads the DL-Bus of a UVR64 controller using a D1 Mini (ESP8266).

## Features

- 6 temperature channels
- 4 relay states
- Decoding via Manchester code

## Installation

Install the required Python dependencies:

```bash
pip install -r requirements.txt
```

## Usage

Add the component via `external_components` in your ESPHome configuration:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/DerTolleMaz/esphome-uvr64
    components: [uvr64_dlbus]
    refresh: always

sensor:
  - platform: uvr64_dlbus
    id: uvr64_bus
    pin: D1
    temp_sensors:
      - name: "Temp 1"
      - name: "Temp 2"
      - name: "Temp 3"
      - name: "Temp 4"
      - name: "Temp 5"
      - name: "Temp 6"
    relay_sensors:
      - name: "Relay 1"
      - name: "Relay 2"
      - name: "Relay 3"
  - name: "Relay 4"
```

## Hardware

The UVR64 DL bus is a 24 V open‑drain line. It needs a pull‑up resistor to 24 V
and must be level shifted for a 3.3 V microcontroller such as the ESP8266. One
simple circuit uses an NPN transistor with a pull‑up to 3.3 V on the collector
and a voltage divider on the bus side (e.g. 22 kΩ/4.7 kΩ). The UVR64
specification states a bit frequency of 50 Hz, so each bit lasts 20 ms and edge
timing is critical.

### Local test

Use `source: local` and `path: ./components` in your YAML file to test locally.

### Build

Install ESPHome and run `esphome compile uvr64_dlbus.yaml`.

## License

This project is licensed under the MIT License – see [LICENSE](LICENSE) for details.
