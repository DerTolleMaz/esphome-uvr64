#include "dlbus_sensor.h"
#include "esphome/core/log.h"
#include <algorithm>
#ifdef UNIT_TEST
#include "arduino_stubs.h"
#endif

#ifndef ARDUINO
#include "arduino_stubs.h"
#endif

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

DLBusSensor::DLBusSensor() {
  this->bit_index_ = 0;
  this->timings_.fill(0);
}

DLBusSensor::DLBusSensor(uint8_t pin) : pin_num_(pin) {
  this->bit_index_ = 0;
  this->timings_.fill(0);
}

DLBusSensor::DLBusSensor(InternalGPIOPin *pin) : pin_(pin) {
  this->bit_index_ = 0;
  this->timings_.fill(0);
}

void DLBusSensor::setup() {
  if (this->pin_ != nullptr) {
    this->pin_->setup();
    this->pin_isr_ = this->pin_->to_isr();
    this->pin_->attach_interrupt(DLBusSensor::isr_typed, this, gpio::INTERRUPT_ANY_EDGE);
    ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", this->pin_->get_pin());
  } else {
    pinMode(pin_num_, INPUT);
    attachInterruptArg(digitalPinToInterrupt(pin_num_), &DLBusSensor::isr, this, CHANGE);
    ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", pin_num_);
  }
}

void DLBusSensor::loop() {
  if (frame_buffer_ready_) {
    if (this->pin_ != nullptr) {
      this->pin_->detach_interrupt();
    } else {
      detachInterrupt(digitalPinToInterrupt(pin_num_));
    }
    parse_frame_();
    compute_timing_stats_();
    if (this->pin_ != nullptr) {
      this->pin_->attach_interrupt(DLBusSensor::isr_typed, this, gpio::INTERRUPT_ANY_EDGE);
    } else {
      attachInterruptArg(digitalPinToInterrupt(pin_num_), &DLBusSensor::isr, this, CHANGE);
    }
    frame_buffer_ready_ = false;
    bit_index_ = 0;
    timings_.fill(0);
  }
}

void IRAM_ATTR DLBusSensor::isr(void *arg) {
  auto *self = static_cast<DLBusSensor *>(arg);
  // TODO: Echtzeit-DL-Bus-Decoder hier implementieren.
  self->frame_buffer_ready_ = true;
}

void IRAM_ATTR DLBusSensor::isr_typed(DLBusSensor *arg) { DLBusSensor::isr(static_cast<void *>(arg)); }

void DLBusSensor::parse_frame_() {
  std::array<uint8_t, 8> bytes{};
  size_t bit_pos = 0;
  for (size_t i = 0; i + 1 < bit_index_ && bit_pos < bytes.size() * 8; i += 2) {
    bool bit = timings_[i] < timings_[i + 1];
    bytes[bit_pos / 8] <<= 1;
    bytes[bit_pos / 8] |= bit ? 1 : 0;
    bit_pos++;
  }

  for (int i = 0; i < 4; i++) {
    uint8_t high = bytes[2 * i];
    uint8_t low = bytes[2 * i + 1];
    int16_t raw = static_cast<int16_t>((high << 8) | low);
    if (this->temp_sensors_[i]) {
      this->temp_sensors_[i]->publish_state(raw / 10.0f);
    }
  }

  for (int i = 0; i < 4; i++) {
    if (this->relay_sensors_[i]) {
      this->relay_sensors_[i]->publish_state(false);
    }
  }

  ESP_LOGD(TAG, "Parsed DLBus frame from timings");
}

void DLBusSensor::compute_timing_stats_() {
  std::fill(std::begin(timing_histogram_), std::end(timing_histogram_), 0);
  for (size_t i = 0; i < bit_index_; i++) {
    if (timings_[i] < 64) {
      timing_histogram_[timings_[i]]++;
    }
  }

  ESP_LOGI(TAG, "Bit length histogram:");
  for (int i = 0; i < 64; i++) {
    if (timing_histogram_[i] > 0) {
      ESP_LOGI(TAG, "  Length %d: %d samples", i, timing_histogram_[i]);
    }
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
