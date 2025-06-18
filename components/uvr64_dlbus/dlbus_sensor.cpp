// MIT License - see LICENSE file in the project root for full details.
#include "dlbus_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/api/custom_api_device.h"

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

class DLBusSensorWithAPI : public DLBusSensor, public api::CustomAPIDevice {
 public:
  void set_byte_order_little(bool little) { this->byte_order_little_ = little; }
  void parse_frame_() override;

 protected:
  bool byte_order_little_ = true;  // default to little endian
};

void DLBusSensorWithAPI::parse_frame_() {
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
    int16_t raw;
    if (byte_order_little_) {
      raw = (raw_bytes[sync_offset + 2 * i + 1] << 8) | raw_bytes[sync_offset + 2 * i];
    } else {
      raw = (raw_bytes[sync_offset + 2 * i] << 8) | raw_bytes[sync_offset + 2 * i + 1];
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
