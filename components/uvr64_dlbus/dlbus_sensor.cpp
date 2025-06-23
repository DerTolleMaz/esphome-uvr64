#include "dlbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

void DLBusSensor::setup() {
  pinMode(pin_, INPUT);
  attachInterruptArg(digitalPinToInterrupt(pin_), &DLBusSensor::isr, this, CHANGE);
  ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", pin_);
}

void DLBusSensor::loop() {
  // Check if we have a complete frame
  if (frame_buffer_ready_) {
    detachInterrupt(digitalPinToInterrupt(pin_));  // Temporarily disable interrupt
    compute_timing_stats_();                       // Optional: log or analyze bit timings
    parse_frame_();                                // Parse the frame and publish results
    frame_buffer_ready_ = false;
    attachInterruptArg(digitalPinToInterrupt(pin_), &DLBusSensor::isr, this, CHANGE);  // Re-enable
  }
}

void IRAM_ATTR DLBusSensor::isr(void *arg) {
  auto *self = static_cast<DLBusSensor *>(arg);
  uint32_t now = micros();
  bool state = digitalRead(self->pin_);
  uint32_t duration = now - self->last_edge_;
  self->last_edge_ = now;

  if (duration < self->debounce_us_) {
    return;  // ignore bouncing
  }

  if (self->bit_index_ < sizeof(self->frame_buffer_) * 8) {
    size_t byte_idx = self->bit_index_ / 8;
    size_t bit_pos = self->bit_index_ % 8;

    if (state) {
      self->frame_buffer_[byte_idx] |= (1 << bit_pos);
    } else {
      self->frame_buffer_[byte_idx] &= ~(1 << bit_pos);
    }

    self->timing_histogram_[std::min(duration / 10, 255u)]++;  // Bucket histogram (10 µs bins)
    self->bit_index_++;

    if (self->bit_index_ >= 64) {  // Example: fixed frame size
      self->frame_buffer_ready_ = true;
    }
  }
}

void DLBusSensor::parse_frame_() {
  // Dummy parser: fill values with dummy data
  for (int i = 0; i < 6; i++) {
    if (temp_sensors_[i]) {
      temp_sensors_[i]->publish_state(20.0 + i);
    }
  }

  for (int i = 0; i < 4; i++) {
    if (relay_sensors_[i]) {
      relay_sensors_[i]->publish_state(i % 2 == 0);
    }
  }

  ESP_LOGD(TAG, "Parsed DLBus frame (dummy values).");
}

void DLBusSensor::compute_timing_stats_() {
  ESP_LOGI(TAG, "Bit length histogram:");
  for (size_t i = 0; i < 256; i++) {
    if (timing_histogram_[i] > 0) {
      ESP_LOGI(TAG, "  %3zu0 µs: %u", i, timing_histogram_[i]);
    }
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
