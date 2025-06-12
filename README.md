# UVR64 DL-Bus für ESPHome (External Component)

Diese Komponente liest den DL-Bus eines UVR64-Reglers mit einem D1 Mini (ESP8266) aus.

## Features

- 6 Temperaturkanäle
- 4 Relaiszustände
- Dekodierung über Manchester-Code

## Verwendung

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/DEIN-BENUTZERNAME/uvr64-dlbus-esphome
    components: [uvr64_dlbus]
```

## Lokaler Test

Nutze `source: local` und `path: ./custom_components` in deiner YAML-Datei.
