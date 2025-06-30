**Language:** [Deutsch](README.md) | [English](README.en.md)

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
