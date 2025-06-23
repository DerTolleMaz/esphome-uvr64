#include "dlbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

DLBusSensor::DLBusSensor(uint8_t pin) : pin_(pin) {}

void DLBusSensor::setup() {
  pinMode(this->pin_, INPUT);
  attachInterruptArg(digitalPinToInterrupt(this->pin_), &DLBusSensor::isr, this, CHANGE);
  ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", this->pin_);
}

void DLBusSensor::loop() {
  // Hier ggf. implementieren: Dekodierung nach Timing-Analyse oder Trigger auf gültiges Frame
  // Beispiel: alle x ms parse_frame_() aufrufen
}

void DLBusSensor::set_temp_sensor(uint8_t index, sensor::Sensor *sensor) {
  if (index < 6) {
    this->temp_sensors_[index] = sensor;
  }
}

void DLBusSensor::set_relay_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) {
  if (index < 4) {
    this->relay_sensors_[index] = sensor;
  }
}

void IRAM_ATTR DLBusSensor::isr(void *arg) {
  auto *self = static_cast<DLBusSensor *>(arg);
  uint32_t now = micros();
  uint32_t duration = now - self->last_interrupt_time_;
  self->last_interrupt_time_ = now;

  if (duration > 10000) {  // Beispielschwelle für neues Frame
    self->bit_durations_.clear();
  }

  self->bit_durations_.push_back(duration);

  if (self->bit_durations_.size() > 256) {
    self->bit_durations_.erase(self->bit_durations_.begin());  // Buffer begrenzen
  }
}

void DLBusSensor::compute_timing_stats_() {
  if (bit_durations_.empty()) return;

  uint32_t min = UINT32_MAX, max = 0, sum = 0;
  for (auto &d : bit_durations_) {
    if (d < min) min = d;
    if (d > max) max = d;
    sum += d;
  }

  uint32_t mean = sum / bit_durations_.size();

  ESP_LOGI(TAG, "Bit Timing – Min: %u µs, Max: %u µs, Mean: %u µs", min, max, mean);
}

void DLBusSensor::parse_frame_() {
  // Beispielhafte Umsetzung: Auswertung Frame-Bytes → Relais + Temperaturen
  // Achtung: Diese Methode muss noch vollständig mit Dekodierlogik ergänzt werden

  // Dummydaten (nur zur Demonstration):
  if (this->temp_sensors_[0] != nullptr) {
    this->temp_sensors_[0]->publish_state(42.0);
  }
  if (this->relay_sensors_[0] != nullptr) {
    this->relay_sensors_[0]->publish_state(true);
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
