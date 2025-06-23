#include "dlbus_sensor.h"
#include "esphome/core/log.h"

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
  // TODO: Echtzeit-DL-Bus-Decoder hier implementieren.
  self->frame_buffer_ready_ = true;
}

void DLBusSensor::parse_frame_() {
  for (int i = 0; i < 6; i++) {
    if (this->temp_sensors_[i]) {
      this->temp_sensors_[i]->publish_state(20.0f + i);
    }
  }
  for (int i = 0; i < 4; i++) {
    if (this->relay_sensors_[i]) {
      this->relay_sensors_[i]->publish_state(i % 2 == 0);
    }
  }
  ESP_LOGD(TAG, "Parsed DLBus frame (dummy values).");
}

void DLBusSensor::compute_timing_stats_() {
  ESP_LOGI(TAG, "Bit length histogram:");
  for (int i = 0; i < 64; i++) {
    if (timing_histogram_[i] > 0) {
      ESP_LOGI(TAG, "  Length %d: %d samples", i, timing_histogram_[i]);
    }
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
