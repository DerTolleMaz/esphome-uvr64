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
      type: local
      path: ./components
    components: [uvr64_dlbus]

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

## Lizenz

Dieses Projekt steht unter der MIT-Lizenz – siehe die Datei [LICENSE](LICENSE) für Details.
