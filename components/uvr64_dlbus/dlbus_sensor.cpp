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
    detachInterrupt(digitalPinToInterrupt(pin_));  // Temporär deaktivieren
    parse_frame_();
    compute_timing_stats_();
    frame_buffer_ready_ = false;
    attachInterruptArg(digitalPinToInterrupt(pin_), &DLBusSensor::isr, this, CHANGE);
  }
}

void IRAM_ATTR DLBusSensor::isr(void *arg) {
  auto *self = static_cast<DLBusSensor *>(arg);
  // Hier DL-Bus-Rohdaten erfassen. Derzeit als Dummy.
  self->frame_buffer_ready_ = true;
}

void DLBusSensor::parse_frame_() {
  for (int i = 0; i < 6; ++i) {
    if (temp_sensors_[i]) {
      temp_sensors_[i]->publish_state(20.0 + i);  // Dummywerte
    }
  }
  for (int i = 0; i < 4; ++i) {
    if (relay_sensors_[i]) {
      relay_sensors_[i]->publish_state(i % 2 == 0);  // Dummywerte
    }
  }
  ESP_LOGD(TAG, "Parsed DLBus frame (dummy values).");
}

void DLBusSensor::compute_timing_stats_() {
  ESP_LOGI(TAG, "Bit length histogram:");
  for (int i = 0; i < 64; ++i) {
    if (timing_histogram_[i] > 0) {
      ESP_LOGI(TAG, "Length %d us: %d", i, timing_histogram_[i]);
    }
  }
}

void DLBusSensor::set_temp_sensor(uint8_t index, sensor::Sensor *sensor) {
  if (index < 6) this->temp_sensors_[index] = sensor;
}

void DLBusSensor::set_relay_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) {
  if (index < 4) this->relay_sensors_[index] = sensor;
}

}  // namespace uvr64_dlbus
}  // namespace esphome
