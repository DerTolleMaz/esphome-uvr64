# UVR64 DL-Bus für ESPHome

Diese Komponente liest den DL-Bus eines UVR64-Reglers mit einem D1 Mini (ESP8266) aus.

## Features

- 6 Temperaturkanäle
- 4 Relaiszustände
- Dekodierung über Manchester-Code

## Verwendung

```yaml
external_components:
  - source:
      type: local
      path: ./custom_components
    components: [uvr64_dlbus]
```

## Lokaler Test

Nutze `source: local` und `path: ./custom_components` in deiner YAML-Datei.

## Lizenz

Dieses Projekt steht unter der MIT-Lizenz – siehe die Datei [LICENSE](LICENSE) für Details.
