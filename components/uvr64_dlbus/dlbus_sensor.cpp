// MIT License - see LICENSE file in the project root for full details.
#include "dlbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

void DLBusSensor::parse_frame() {
  if (bit_index_ < 80) {
    ESP_LOGW(TAG, "Received frame too short: %d bits", bit_index_);
    return;
  }

  uint32_t sum = 0;
  for (int i = 0; i < bit_index_; i++) sum += timings_[i];
  uint32_t avg_duration = sum / bit_index_;

  ESP_LOGD(TAG, "Bit count: %d, avg duration: %u µs", bit_index_, avg_duration);

  uint8_t raw_bytes[16] = {0};
  int byte_i = 0, bit_i = 0;

  for (int i = 0; i < bit_index_ - 1; i += 2) {
    uint32_t t1 = timings_[i];
    uint32_t t2 = timings_[i + 1];
    bool bit = false;

    if (abs((int)t1 - (int)t2) < avg_duration / 4) {
      bit = false; // Manchester: 01 = 0
    } else if (abs((int)t1 - (int)t2) > avg_duration / 2) {
      bit = true;  // Manchester: 10 = 1
    } else {
      ESP_LOGV(TAG, "Unclear bit timing at %d: t1=%u t2=%u", i, t1, t2);
      continue;
    }

    raw_bytes[byte_i] <<= 1;
    raw_bytes[byte_i] |= bit;
    bit_i++;
    if (bit_i == 8) {
      bit_i = 0;
      byte_i++;
    }
    if (byte_i >= 16) break;
  }

  ESP_LOGD(TAG, "Decoded raw bytes:");
  for (int i = 0; i < 16; i++) {
    ESP_LOGD(TAG, "[%02d] 0x%02X", i, raw_bytes[i]);
  }

  // Prüfsumme berechnen (XOR über alle Bytes außer letztem)
  uint8_t checksum = 0;
  for (int i = 0; i < 15; i++) checksum ^= raw_bytes[i];
  if (checksum != raw_bytes[15]) {
    ESP_LOGW(TAG, "Checksum mismatch: expected 0x%02X, got 0x%02X", raw_bytes[15], checksum);
  }

  int sync_offset = -1;
  for (int i = 0; i < 14; i++) {
    if (raw_bytes[i] == 0xFF && raw_bytes[i + 1] == 0xFF) {
      sync_offset = i + 2;
      ESP_LOGD(TAG, "SYNC found at offset %d", i);
      break;
    }
  }
  if (sync_offset == -1) {
    for (int i = 0; i < 14; i++) {
      if (raw_bytes[i] == 0x0B && raw_bytes[i + 1] == 0x88) {
        sync_offset = i;
        ESP_LOGW(TAG, "Alternative sync pattern 0x0B88 found at offset %d", i);
        break;
      }
    }
  }
  if (sync_offset == -1) {
    sync_offset = 0;
    ESP_LOGW(TAG, "No known sync found – falling back to offset 0");
  }

  if (sync_offset + 13 >= 16) {
    ESP_LOGW(TAG, "Not enough data after sync offset %d", sync_offset);
    return;
  }

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
