// DL-Bus Dekodierungskomponente für ESPHome (mit Manchester-Decoding und Telegrammauswertung für UVR64)
#include "dlbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

void DLBusSensor::setup() {
  ESP_LOGI(TAG, "DLBusSensor setup complete");
}

void DLBusSensor::loop() {
  if (!frame_ready_) return;
  frame_ready_ = false;
  parse_frame_();
}

void DLBusSensor::set_temp_sensor(int index, sensor::Sensor *sensor) {
  if (index >= 0 && index < 6) {
    this->temp_sensors_[index] = sensor;
  }
}

void DLBusSensor::set_relay_sensor(int index, binary_sensor::BinarySensor *sensor) {
  if (index >= 0 && index < 4) {
    this->relay_sensors_[index] = sensor;
  }
}

void DLBusSensor::update() {
  if (!frame_ready_) return;
  frame_ready_ = false;
  ESP_LOGD(TAG, "DLBus frame received (update), decoding...");
  parse_frame_();
}

void DLBusSensor::parse_frame_() {
  constexpr size_t timing_len = 128;
  ESP_LOGD(TAG, "Timing sequence (%zu edges):", timing_len);
  for (size_t i = 0; i < std::min(timing_len, size_t(33)); i++) {
    ESP_LOGD(TAG, "  timings[%03zu] = %3u µs", i, this->timings_[i]);
  }

  size_t bit_count = 0;
  for (size_t i = 0; i + 1 < timing_len; i += 2) {
    uint32_t t1 = this->timings_[i];
    uint32_t t2 = this->timings_[i + 1];

    if (t1 < 10 || t2 < 10 || t1 > 130 || t2 > 130) {
      ESP_LOGD(TAG, "Skipping unplausible timings t1=%u, t2=%u", t1, t2);
      continue;
    }

    if (t1 < t2) {
      bits_[bit_count++] = 1;
    } else {
      bits_[bit_count++] = 0;
    }
    ESP_LOGD(TAG, "Bit Count %zu", bit_count);

    if (t1 > 1000 || t2 > 1000) {
      ESP_LOGD(TAG, "Aborting decode: framing break at i=%zu (t1=%u µs, t2=%u µs)", i, t1, t2);
      break;
    }
  }

  ESP_LOGD(TAG, "Decoded bit count: %zu", bit_count);
  if (bit_count < 32) {
    ESP_LOGD(TAG, "Decoded too few bits: %zu", bit_count);
    return;
  }

  uint8_t raw_bytes[32] = {0};
  for (int i = 0; i < 32 && i * 8 + 7 < bit_count; i++) {
    for (int b = 0; b < 8; b++) {
      raw_bytes[i] <<= 1;
      if (bits_[i * 8 + b]) raw_bytes[i] |= 0x01;
    }
  }

  ESP_LOGD(TAG, "First few bytes: %02X %02X %02X %02X", raw_bytes[0], raw_bytes[1], raw_bytes[2], raw_bytes[3]);

  if (raw_bytes[0] != 0xAA) {
    ESP_LOGW(TAG, "Ungültiger Startwert: 0x%02X", raw_bytes[0]);
    return;
  }

  // Gerätekennung prüfen (UVR64: 0x10)
  if (raw_bytes[1] != 0x10) {
    ESP_LOGW(TAG, "Nicht UVR64-Adresse: 0x%02X", raw_bytes[1]);
    return;
  }
  ESP_LOGD(TAG, "Gerätekennung: 0x%02X (UVR64, ID %d)", raw_bytes[1], raw_bytes[1] & 0x0F);

  // CRC-Prüfung (einfache Prüfsumme über n-1 Bytes)
  uint8_t crc = 0;
  for (int i = 0; i < 31; i++) crc += raw_bytes[i];
  if (crc != raw_bytes[31]) {
    ESP_LOGW(TAG, "CRC ungültig: berechnet 0x%02X, empfangen 0x%02X", crc, raw_bytes[31]);
    return;
  }

  // Temperaturen dekodieren
  for (int i = 0; i < 6; i++) {
    uint8_t lo = raw_bytes[2 + i * 2];
    uint8_t hi = raw_bytes[3 + i * 2];
    int16_t value = static_cast<int16_t>((hi << 8) | lo);

    float temperature;
    if (hi & 0x80) {
      temperature = 0.1f * (value - 0x10000);
    } else {
      temperature = 0.1f * value;
    }

    if (temperature < -50.0f || temperature > 150.0f) {
      ESP_LOGW(TAG, "Temperatur %d ungültig: %.1f °C — verworfen", i + 1, temperature);
      continue;
    }

    ESP_LOGD(TAG, "Temperatur %d: %.1f °C", i + 1, temperature);
    if (this->temp_sensors_[i] != nullptr) {
      this->temp_sensors_[i]->publish_state(temperature);
    }
  }

  // Relaisausgänge (UVR64): Byte 14
  uint8_t relay_byte = raw_bytes[14];
  for (int i = 0; i < 4; i++) {
    bool relay_state = relay_byte & (1 << (3 + i));
    ESP_LOGD(TAG, "Relais %d: %s", i + 1, relay_state ? "AN" : "AUS");
    if (this->relay_sensors_[i] != nullptr) {
      this->relay_sensors_[i]->publish_state(relay_state);
    }
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
