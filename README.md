md**Language:** [Deutsch](README.md) | [English](README.en.md)

# ESPHome UVR64 DL-Bus Decoder

## 📦 Projektbeschreibung
Dies ist ein ESPHome-Komponentenprojekt zur direkten Dekodierung des DL-Bus-Protokolls von Technische Alternative (UVR64). Der Decoder liest Logging-Datenrahmen, wertet Temperaturen und Relaiszustände aus und integriert diese direkt in Home Assistant.

---

## 🔧 Technische Daten des DL-Bus (UVR64)

| Parameter                | Wert                          |
|--------------------------|-------------------------------|
| **Signalart**            | Manchester-codiert (EXOR mit Displaytakt) |
| **Displaytakt UVR64**    | 50 Hz                         |
| **Bitdauer**             | 20 ms                         |
| **Halbbits**             | 2 × 10 ms pro Bit             |
| **Flankenfrequenz**      | 100 Flanken pro Sekunde       |
| **Bitrate**              | 50 Bit/s                      |
| **Rahmenlänge**          | 3,12 Sekunden pro Datenrahmen |
| **Gerätekennung UVR64**  | `0x20`                        |

---

## 🔗 Rahmenstruktur

| Feld          | Inhalt                          | Dauer                      |
|----------------|---------------------------------|-----------------------------|
| **SYNC Start** | 16 × HIGH → 320 ms HIGH         | 16 Bits                     |
| **Datenbytes** | 1 Startbit (0) + 8 Datenbits (LSB first) + 1 Stopbit (1) | 10 Bits / 200 ms pro Byte |
| **SYNC Ende**  | 16 × HIGH                       | 16 Bits                     |

---

## 🗂️ Datenstruktur

| Byte-Nr. | Feld           | Beschreibung                                  |
|-----------|----------------|-----------------------------------------------|
| 0         | SYNC           | 16 HIGH Bits ohne Start/Stop                 |
| 1         | Gerätekennung  | 0x20                                          |
| 2–3       | Temp1          | Temperatur 1 (Low + High Byte)               |
| 4–5       | Temp2          | Temperatur 2                                 |
| 6–7       | Temp3          | Temperatur 3                                 |
| 8–9       | Temp4          | Temperatur 4                                 |
| 10–11     | Temp5          | Temperatur 5                                 |
| 12–13     | Temp6          | Temperatur 6                                 |
| 14        | Ausgangsbyte   | Relaisstatus (bitweise codiert)              |

## 🌡️ Temperaturdaten-Format

| Eigenschaft   | Wert                                 |
|----------------|--------------------------------------|
| Datenformat    | 16 Bit (Low-Byte + High-Byte)       |
| Auflösung      | 1/10 °C                             |
| Vorzeichen     | signed (Vorzeichenbehaftet)         |
| Byte-Reihenfolge| Little Endian (Low-Byte zuerst)    |

Berechnung der Temperatur:

```
Temp = (High << 8) | Low
Temp_Celsius = Temp / 10.0
```

## 🚦 Ausgangsbyte (Relais-Status)

| Bit | Bedeutung     |
|-----|----------------|
| 0   | Relais 1       |
| 1   | Relais 2       |
| 2   | Relais 3       |
| 3   | Relais 4       |
| 4–7 | Unbenutzt      |

Bit = 1 → Relais AN  
Bit = 0 → Relais AUS

---

## 🔥 Manchester-Codierung

- Jedes Bit besteht aus **zwei Halbbits à 10 ms**.
- Erste Halbwelle = **invertierter Bitwert**.
- Zweite Halbwelle = **tatsächlicher Bitwert**.

→ **Der Bitwert kann sicher in der zweiten Halbwelle gelesen werden.**

---

## 🏗️ Byte-Struktur

- **1 Startbit:** 0  
- **8 Datenbits:** LSB first  
- **1 Stopbit:** 1

---

## 🛠️ Hardware-Anforderungen

- **Pegel:** DL-Bus 24V → Pegelwandler notwendig (Spannungsteiler + Schmitt-Trigger empfohlen).
- **Genaue Flankenerkennung:** alle **10ms**.
- **Manchester-Dekodierung notwendig:** Kein UART direkt verwendbar.

---

## 🚦 Decoder-Logik

1. **SYNC-Erkennung:**  
→ 320 ms HIGH (16 HIGH-Bits ohne Flanken)

2. **Manchester-Dekodierung:**  
→ Jede Flanke → Halbbits → Bit zusammensetzen.

3. **Bit-zu-Byte-Konverter:**  
→ Startbit → 8 Datenbits → Stopbit.

4. **Rahmen-Pufferung:**  
→ Von SYNC bis SYNC als vollständiges Telegramm verarbeiten.

5. **Datenparser:**  
→ Adressiert den Sender **0x20** (UVR64).  
→ Extrahiert Temperaturen, Relaiszustände und weitere Werte.

---

## 🏡 Integration in ESPHome & Home Assistant

- Alle Sensoren sind direkt als `sensor:` und `binary_sensor:` in ESPHome verfügbar.
- Home Assistant kann über die ESPHome-Integration alle Werte direkt auslesen.

---

## 📜 Lizenz
MIT License

---

## ❤️ Credits
Basierend auf der Protokolldokumentation von Technische Alternative und der Community-Arbeit rund um den DL-Bus.

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
      ref: main
    components: [uvr64_dlbus]
    refresh: always
globals:
  - id: dlbus_ptr
    type: esphome::uvr64_dlbus::DLBusSensor*
    restore_value: no
    initial_value: 'nullptr'
    #includes: ["uvr64_dlbus/dlbus_sensor.h"]
# Sensoren deklarieren
sensor:
  - platform: template
    name: "UVR64 Solarthermie"
    id: temp0
  - platform: template
    name: "UVR64 Solarthermie Boiler"
    id: temp1
  - platform: template
    name: "UVR64 Solarthermie Puffer Unten"
    id: temp2
  - platform: template
    name: "UVR64 Solarthermie Pool Waermetauscher"
    id: temp3
  - platform: template
    name: "UVR64 Heizung Pool Waermetauscher"
    id: temp4
  - platform: template
    name: "UVR64 Solarthermie Puffer Oben"
    id: temp5

  - platform: wifi_signal
    name: UVR64 Solarthermie WiFi Signal Strength
    update_interval: 60s


binary_sensor:
  - platform: gpio # DEBUG for the signal. 
    pin: 
      number: GPIO20
      mode: INPUT
    name: "DL Bus Signal"
    on_press:
      - logger.log: "Signal HIGH"
    on_release:
          - logger.log: "Signal LOW"
  - platform: template
    name: "UVR64 Relay Boiler"
    id: relay0
  - platform: template
    name: "UVR64 Relay Puffer"
    id: relay1
  - platform: template
    name: "UVR64 Relay Solarthermie Pool Waermetauscher"
    id: relay2
  - platform: template
    name: "UVR64 Relay Heizung Pool Waermetauscher"
    id: relay3

# DL-Bus-Komponente
uvr64_dlbus:
  id: uvr
  pin: GPIO18
  temp_sensors:
    - temp0
    - temp1
    - temp2
    - temp3
    - temp4
    - temp5
  relay_sensors:
    - relay0
    - relay1
    - relay2
    - relay3

interval:
    - interval: 1s
      then:
      - logger.log: "DL-Bus läuft"
      - lambda: |-
          if (id(dlbus_ptr) == nullptr) {
            auto dl = new esphome::uvr64_dlbus::DLBusSensor(id(uvr)->get_pin());
            dl->set_temp_sensor(0, id(temp0));
            dl->set_temp_sensor(1, id(temp1));
            dl->set_temp_sensor(2, id(temp2));
            dl->set_temp_sensor(3, id(temp3));
            dl->set_temp_sensor(4, id(temp4));
            dl->set_temp_sensor(5, id(temp5));
            dl->set_relay_sensor(0, id(relay0));
            dl->set_relay_sensor(1, id(relay1));
            dl->set_relay_sensor(2, id(relay2));
            dl->set_relay_sensor(3, id(relay3));
            id(dlbus_ptr) = dl;
            App.register_component(dl);
          }


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
