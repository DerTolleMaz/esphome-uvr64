#include "dlbus_sensor.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

void DLBusSensor::setup() {
  pinMode(this->pin_, INPUT);
  attachInterruptArg(digitalPinToInterrupt(this->pin_), &DLBusSensor::isr, this, CHANGE);
  ESP_LOGI(TAG, "DLBusSensor setup complete (GPIO%d)", this->pin_);
}

void DLBusSensor::loop() {
  if (!this->frame_ready_)
    return;

  detachInterrupt(digitalPinToInterrupt(this->pin_));
  this->compute_timing_stats_();
  this->parse_frame_();
  this->frame_ready_ = false;
  this->bit_index_ = 0;
  attachInterruptArg(digitalPinToInterrupt(this->pin_), &DLBusSensor::isr, this, CHANGE);
}

void IRAM_ATTR DLBusSensor::isr(void *arg) {
  auto *self = static_cast<DLBusSensor *>(arg);
  unsigned long now = micros();
  if (self->bit_index_ >= MAX_BITS)
    return;
  self->timings_[self->bit_index_++] = now - self->last_edge_;
  self->last_edge_ = now;

  if (self->bit_index_ >= MAX_BITS) {
    self->frame_ready_ = true;
  }
}

void DLBusSensor::compute_timing_stats_() {
  if (bit_index_ == 0) return;

  uint32_t min = timings_[0];
  uint32_t max = timings_[0];
  uint64_t sum = 0;
  std::vector<uint32_t> sorted_timings;

  for (int i = 0; i < bit_index_; i++) {
    uint32_t val = timings_[i];
    min = std::min(min, val);
    max = std::max(max, val);
    sum += val;
    sorted_timings.push_back(val);
  }

  std::sort(sorted_timings.begin(), sorted_timings.end());
  uint32_t median = sorted_timings[bit_index_ / 2];
  float mean = sum / static_cast<float>(bit_index_);

  float variance = 0;
  for (auto val : sorted_timings) {
    variance += (val - mean) * (val - mean);
  }
  float stddev = sqrtf(variance / bit_index_);

  ESP_LOGI(TAG, "Timing – Min: %u µs, Max: %u µs, Median: %u µs, Mean: %.1f µs, StdDev: %.1f µs", min, max, median, mean, stddev);
}

void DLBusSensor::parse_frame_() {
  ESP_LOGD(TAG, "Parsing DLBus frame:");
  for (int i = 0; i < bit_index_; i++) {
    ESP_LOGV(TAG, "  Bit %d duration: %u µs", i, timings_[i]);
  }

  // Beispiel: Dummy-Wert setzen
  if (temp_sensors_[0])
    temp_sensors_[0]->publish_state(42.0);
  if (relay_sensors_[0])
    relay_sensors_[0]->publish_state(true);
}

void DLBusSensor::set_temp_sensor(int index, sensor::Sensor *sensor) {
  if (index >= 0 && index < 6)
    this->temp_sensors_[index] = sensor;
}

void DLBusSensor::set_relay_sensor(int index, binary_sensor::BinarySensor *sensor) {
  if (index >= 0 && index < 4)
    this->relay_sensors_[index] = sensor;
}

}  // namespace uvr64_dlbus
}  // namespace esphome
