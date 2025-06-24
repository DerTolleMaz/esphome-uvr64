**Language:** [Deutsch](README.md) | [English](README.en.md)

# UVR64 DL-Bus für ESPHome

Diese Komponente liest den DL-Bus eines UVR64-Reglers mit einem D1 Mini (ESP8266) aus.

## Features

- 6 Temperaturkanäle
- 4 Relaiszustände
- Dekodierung über Manchester-Code

## Installation

Installiere die notwendigen Python-Abhängigkeiten mit

```bash
pip install -r requirements.txt
```

## Verwendung

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
      - name: "Relais 1"
      - name: "Relais 2"
      - name: "Relais 3"
      - name: "Relais 4"
```

## Lokaler Test

Nutze `source: local` und `path: ./components` in deiner YAML-Datei.

## Build

Installiere ESPHome und führe `esphome compile uvr64_dlbus.yaml` aus.
PlatformIO alleine löst Abhängigkeiten wie `binary_sensor.h` nicht auf.

## Tests

Zum Kompilieren der Beispieltests wird ein C++-Compiler (z.B. `g++`) und
`make` benötigt. Der Test `test_parse_frame` im Verzeichnis `tests` kann mit

```bash
make -C tests
```

gebaut und mit

```bash
make -C tests run
```

ausgeführt werden.

## Lizenz

Dieses Projekt steht unter der MIT-Lizenz – siehe die Datei [LICENSE](LICENSE) für Details.
