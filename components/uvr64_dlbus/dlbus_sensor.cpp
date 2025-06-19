// MIT License - see LICENSE file in the project root for full details.
#include "dlbus_sensor.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

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
  if (frame_ready_) {
    ESP_LOGD(TAG, "DLBus frame received, decoding...");
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

  // Adaptive Software-Entprellung
  if (duration < (self->min_valid_timing_ / 2)) return;

  self->last_edge_ = now;

  if (self->bit_index_ < MAX_BITS) {
    self->timings_[self->bit_index_++] = duration;
  }
