#include "dlbus_sensor.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

DLBusSensor::DLBusSensor(uint8_t pin) : pin_(pin) {}

void DLBusSensor::setup() {
  pinMode(pin_, INPUT);
  attachInterruptArg(digitalPinToInterrupt(pin_), &DLBusSensor::isr, this, CHANGE);
  ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", pin_);
}

void DLBusSensor::loop() {
  if (frame_buffer_ready_) {
    detachInterrupt(digitalPinToInterrupt(pin_));
    parse_frame_();
    compute_timing_stats_();
    attachInterruptArg(digitalPinToInterrupt(pin_), &DLBusSensor::isr, this, CHANGE);
    frame_buffer_ready_ = false;
  }
}

void IRAM_ATTR DLBusSensor::isr(void *arg) {
  auto *self = static_cast<DLBusSensor *>(arg);
  static uint32_t last = micros();
  uint32_t now = micros();
  uint16_t diff = now - last;
  last = now;

  if (self->bit_index_ < DLBusSensor::MAX_TIMINGS)
    self->timings_[self->bit_index_++] = diff;

  if (diff > 1000 && self->bit_index_ >= 16) {
    self->frame_buffer_ready_ = true;
  }
}

void DLBusSensor::parse_frame_() {
  const uint16_t bits = this->bit_index_ / 2;
  uint8_t bytes[DLBusSensor::MAX_TIMINGS / 8]{};
  uint8_t cur = 0;
  uint16_t byte_index = 0;

  for (uint16_t i = 0; i < bits; i++) {
    bool bit = timings_[2 * i] < timings_[2 * i + 1];
    cur = (cur << 1) | (bit ? 1 : 0);
    if ((i % 8) == 7) {
      bytes[byte_index++] = cur;
      cur = 0;
    }
  }

  uint16_t temp_count = byte_index / 2;
  if (temp_count > 6)
    temp_count = 6;
  for (uint16_t i = 0; i < temp_count; i++) {
    int16_t raw = (int16_t)((bytes[i * 2] << 8) | bytes[i * 2 + 1]);
    float value = raw / 10.0f;
    if (this->temp_sensors_[i])
      this->temp_sensors_[i]->publish_state(value);
  }

  if (byte_index > 8) {
    uint8_t relay = bytes[8];
    for (int i = 0; i < 4; i++) {
      if (this->relay_sensors_[i])
        this->relay_sensors_[i]->publish_state(relay & (1 << i));
    }
  } else {
    for (int i = 0; i < 4; i++) {
      if (this->relay_sensors_[i]) this->relay_sensors_[i]->publish_state(false);
    }
  }
  ESP_LOGD(TAG, "Parsed DLBus frame with %d bytes", byte_index);
  bit_index_ = 0;
}

void DLBusSensor::compute_timing_stats_() {
  memset(timing_histogram_, 0, sizeof(timing_histogram_));
  for (uint16_t i = 0; i < bit_index_; i++) {
    uint16_t val = timings_[i];
    if (val >= 64) val = 63;
    timing_histogram_[val]++;
  }
  ESP_LOGD(TAG, "Bit length histogram:");
  for (int i = 0; i < 64; i++) {
    if (timing_histogram_[i] > 0) {
      ESP_LOGD(TAG, "  Length %d: %d samples", i, timing_histogram_[i]);
    }
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
