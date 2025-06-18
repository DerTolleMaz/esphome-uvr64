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
}

void DLBusSensor::update() {
  if (frame_ready_) {
    parse_frame_();
    bit_index_ = 0;
    last_edge_ = micros();
    attachInterruptArg(digitalPinToInterrupt(pin_), isr, this, CHANGE);
    frame_ready_ = false;
  }
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
  if (self->bit_index_ < MAX_BITS) {
    self->timings_[self->bit_index_++] = duration;
  } else {
    self->frame_ready_ = true;
    detachInterrupt(digitalPinToInterrupt(self->pin_));
  }
}

void DLBusSensor::parse_frame_() {
  if (bit_index_ < 80) return;

  // Timingkalibrierung: Mittelwert ermitteln
  uint32_t sum = 0;
  for (int i = 0; i < bit_index_; i++) sum += timings_[i];
  uint32_t avg_duration = sum / bit_index_;

  uint8_t raw_bytes[16] = {0};
  int byte_i = 0, bit_i = 0;

  ESP_LOGD(TAG, "Bit count: %d, avg duration: %u µs", bit_index_, avg_duration);

  for (int i = 0; i < bit_index_ - 1; i += 2) {
    uint32_t t1 = timings_[i];
    uint32_t t2 = timings_[i + 1];
    bool bit = false;

    if (abs((int)t1 - (int)t2) < avg_duration / 4) {
      bit = false; // Manchester: 01 = 0
    } else if (abs((int)t1 - (int)t2) > avg_duration / 2) {
      bit = true;  // Manchester: 10 = 1
    } else {
      continue; // unklarer Pegel
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

  ESP_LOGD(TAG, "Raw bytes:");
  for (int i = 0; i < 16; i++) {
    ESP_LOGD(TAG, "[%02d] 0x%02X", i, raw_bytes[i]);
  }

  // SYNC-Suche: Finde 0xFF 0xFF Startkennung
  int sync_offset = -1;
  for (int i = 0; i < 14; i++) {
    if (raw_bytes[i] == 0xFF && raw_bytes[i + 1] == 0xFF) {
      sync_offset = i + 2;
      break;
    }
  }
  if (sync_offset == -1 || sync_offset + 11 >= 16) return;

  for (int i = 0; i < 6; i++) {
    int16_t raw = (raw_bytes[sync_offset + 2 * i] << 8) | raw_bytes[sync_offset + 2 * i + 1];
    float temp = raw / 10.0f;
    if (temp_sensors_[i] != nullptr)
      temp_sensors_[i]->publish_state(temp);
  }

  uint8_t relays = raw_bytes[sync_offset + 12];
  for (int i = 0; i < 4; i++) {
    if (relay_sensors_[i] != nullptr)
      relay_sensors_[i]->publish_state((relays >> i) & 0x01);
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
