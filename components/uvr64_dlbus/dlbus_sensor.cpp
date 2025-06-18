// MIT License - see LICENSE file in the project root for full details.
#include "dlbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

void DLBusSensor::setup() {
  pinMode(pin_, INPUT);
  last_edge_ = micros();
  attachInterruptArg(digitalPinToInterrupt(pin_), isr, this, CHANGE);
  ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", pin_);
}

void DLBusSensor::update() {
  if (!frame_ready_)
    return;

  ESP_LOGD(TAG, "DLBus frame received (update), decoding...");
  parse_frame_();
  bit_index_ = 0;
  last_edge_ = micros();
  attachInterruptArg(digitalPinToInterrupt(pin_), isr, this, CHANGE);
  frame_ready_ = false;
}

void DLBusSensor::loop() {
 // if (frame_ready_) {
 //   ESP_LOGD("uvr64_dlbus", "DLBus frame received, decoding...");
 //   parse_frame_();
 //   bit_index_ = 0;
 //   last_edge_ = micros();
 //   attachInterruptArg(digitalPinToInterrupt(pin_), isr, this, CHANGE);
 //   frame_ready_ = false;
 // }
}

void DLBusSensor::set_temp_sensor(int index, sensor::Sensor *sensor) {
  if (index >= 0 && index < 6)
    temp_sensors_[index] = sensor;
}

void DLBusSensor::set_relay_sensor(int index, binary_sensor::BinarySensor *sensor) {
  if (index >= 0 && index < 4)
    relay_sensors_[index] = sensor;
}

void IRAM_ATTR DLBusSensor::isr(void *arg) {
  auto *self = static_cast<DLBusSensor *>(arg);
  unsigned long now = micros();
  uint32_t duration = now - self->last_edge_;
  self->last_edge_ = now;

  // NEU: Nur gültige Zeitdifferenzen erfassen
  if (duration < 30 || duration > 300) return;

  if (self->bit_index_ < MAX_BITS) {
    self->timings_[self->bit_index_++] = duration;
  } else {
    self->frame_ready_ = true;
    detachInterrupt(digitalPinToInterrupt(self->pin_));
  }
}

void DLBusSensor::parse_frame_() {
  ESP_LOGI(TAG, "Timing sequence (%d edges):", bit_index_);
  for (int i = 0; i < bit_index_; i++) {
    ESP_LOGI(TAG, "  timings[%03d] = %3u µs", i, timings_[i]);
  }

  // Ultra-toleranter Manchester-Debug-Decoder
  bool bits[64];  // 64 Bits bei 128 Flanken
  int bit_count = 0;

  for (int i = 0; i + 1 < bit_index_ && bit_count < 64; i += 2) {
    uint32_t t1 = timings_[i];
    uint32_t t2 = timings_[i + 1];

    // KEINE Filterung → alles verwerten
    bool bit = (t1 > t2);  // lang-kurz = 1, kurz-lang = 0
    bits[bit_count++] = bit;
  }

  if (bit_count < 64) {
    ESP_LOGW(TAG, "Decoded only %d bits", bit_count);
    return;
  }

  // Bitfolge als String loggen
  std::string bitstring;
  for (int i = 0; i < bit_count; i++) {
    bitstring += bits[i] ? '1' : '0';
  }
  ESP_LOGI(TAG, "Bitstream: %s", bitstring.c_str());

  // Bits zu Bytes umwandeln
  uint8_t raw_bytes[16] = {0};
  for (int i = 0; i < 16; i++) {
    for (int b = 0; b < 8; b++) {
      raw_bytes[i] <<= 1;
      raw_bytes[i] |= bits[i * 8 + b] ? 1 : 0;
    }
  }

  // Rohbytes loggen – unabhängig von Gültigkeit
  ESP_LOGI(TAG, "Decoded raw bytes:");
  for (int i = 0; i < 16; i++) {
    ESP_LOGI(TAG, "[%02d] 0x%02X", i, raw_bytes[i]);
  }

  // OPTIONAL: Sync-Pattern anzeigen
  for (int i = 0; i < 14; i++) {
    if (raw_bytes[i] == 0xFF && raw_bytes[i + 1] == 0xFF) {
      ESP_LOGI(TAG, "Found SYNC pattern 0xFF 0xFF at byte offset %d", i);
    }
    if (raw_bytes[i] == 0x0B && raw_bytes[i + 1] == 0x88) {
      ESP_LOGI(TAG, "Found alternative SYNC 0x0B 0x88 at byte offset %d", i);
    }
  }

  // Noch keine Sensorzuweisung – Debug only!
}

void DLBusSensor::parse_frame_2() {
  if (bit_index_ < 80) {
    ESP_LOGW(TAG, "Received frame too short: %d bits", bit_index_);
    return;
  }

  // Logging aller Timings zur Analyse
  ESP_LOGI(TAG, "Timing sequence (%d edges):", bit_index_);
  //for (int i = 0; i < bit_index_; i++) {
  //  ESP_LOGI(TAG, "  timings[%03d] = %3u µs", i, timings_[i]);
 // }

  // Manchester-Dekodierung: feste Toleranz, keine ratio-Prüfung
  bool bits[128];
  int bit_count = 0;

  for (int i = 0; i + 1 < bit_index_; i += 2) {
    uint32_t t1 = timings_[i];
    uint32_t t2 = timings_[i + 1];

    if (t1 < 30 || t2 < 30 || (t1 + t2) > 300) {
      ESP_LOGV(TAG, "Skipping implausible timings at %d: t1=%u t2=%u", i, t1, t2);
      continue;
    }

    // Annahme: lang-kurz = 1, kurz-lang = 0
    bool bit = (t1 > t2);
    bits[bit_count++] = bit;

    if (bit_count >= 128) break;
  }

  if (bit_count < 64) {
    ESP_LOGW(TAG, "Decoded too few bits: %d", bit_count);
    return;
  }

  // Bits zu Bytes umwandeln
  uint8_t raw_bytes[16] = {0};
  for (int i = 0; i < 16 && (i * 8 + 7) < bit_count; i++) {
    for (int b = 0; b < 8; b++) {
      raw_bytes[i] <<= 1;
      raw_bytes[i] |= bits[i * 8 + b] ? 1 : 0;
    }
  }

  ESP_LOGD(TAG, "Decoded raw bytes:");
  for (int i = 0; i < 16; i++) {
    ESP_LOGD(TAG, "[%02d] 0x%02X", i, raw_bytes[i]);
  }

  // Prüfsumme (XOR) prüfen
  uint8_t checksum = 0;
  for (int i = 0; i < 15; i++) checksum ^= raw_bytes[i];
  if (checksum != raw_bytes[15]) {
    ESP_LOGW(TAG, "Checksum mismatch: expected 0x%02X, got 0x%02X", raw_bytes[15], checksum);
    return;
  }

  // Synchronisation
  int sync_offset = -1;
  for (int i = 0; i < 14; i++) {
    if (raw_bytes[i] == 0xFF && raw_bytes[i + 1] == 0xFF) {
      sync_offset = i + 2;
      ESP_LOGD(TAG, "SYNC pattern 0xFFFF found at offset %d", i);
      break;
    } else if (raw_bytes[i] == 0x0B && raw_bytes[i + 1] == 0x88) {
      sync_offset = i;
      ESP_LOGW(TAG, "Alternative sync pattern 0x0B88 found at offset %d", i);
      break;
    }
  }

  if (sync_offset == -1) {
    sync_offset = 0;
    ESP_LOGW(TAG, "No sync pattern found – defaulting to offset 0");
  }

  if (sync_offset + 13 >= 16) {
    ESP_LOGW(TAG, "Not enough data after sync offset %d", sync_offset);
    return;
  }

  // Temperaturen dekodieren
  for (int i = 0; i < 6; i++) {
    uint8_t low = raw_bytes[sync_offset + 2 * i];
    uint8_t high = raw_bytes[sync_offset + 2 * i + 1];
    int16_t raw = (high << 8) | low;
    if (raw == 0x7FFF || raw == 0x8000 || raw < -1000 || raw > 2000) {
      ESP_LOGW(TAG, "Temp[%d] raw value 0x%04X out of range or invalid", i, raw & 0xFFFF);
      continue;
    }
    float temp = raw / 10.0f;
    ESP_LOGI(TAG, "Temp[%d] = %.1f °C (raw: 0x%04X)", i, temp, raw & 0xFFFF);
    if (temp_sensors_[i] != nullptr)
      temp_sensors_[i]->publish_state(temp);
  }

  // Relaisstatus dekodieren
  uint8_t relays = raw_bytes[sync_offset + 12];
  for (int i = 0; i < 4; i++) {
    bool state = (relays >> i) & 0x01;
    ESP_LOGI(TAG, "Relais[%d] = %s", i, state ? "ON" : "OFF");
    if (relay_sensors_[i] != nullptr)
      relay_sensors_[i]->publish_state(state);
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
