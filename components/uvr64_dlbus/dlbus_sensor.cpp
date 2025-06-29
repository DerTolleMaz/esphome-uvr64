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

uint8_t DLBusSensor::get_pin() const {
  if (this->pin_ != nullptr) {
    return this->pin_->get_pin();
  }
  return this->pin_num_;
}

void DLBusSensor::setup() {
  if (this->pin_ != nullptr) {
    this->pin_->setup();
    this->pin_isr_ = this->pin_->to_isr();
    this->pin_->attach_interrupt(&DLBusSensor::isr, this, gpio::INTERRUPT_ANY_EDGE);
    ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", this->pin_->get_pin());
  } else {
    pinMode(pin_num_, INPUT);
    attachInterruptArg(digitalPinToInterrupt(pin_num_),
                      reinterpret_cast<void (*)(void *)>(&DLBusSensor::isr), this, CHANGE);
    ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", pin_num_);
  }
}

void DLBusSensor::loop() {
  if (frame_buffer_ready_) {
    ESP_LOGD(TAG, "Processing frame with %d bits", bit_index_);
    if (this->pin_ != nullptr) {
      this->pin_->detach_interrupt();
    } else {
      detachInterrupt(digitalPinToInterrupt(pin_num_));
    }
    parse_frame_();
    compute_timing_stats_();
    if (this->pin_ != nullptr) {
      this->pin_->attach_interrupt(&DLBusSensor::isr, this, gpio::INTERRUPT_ANY_EDGE);
    } else {
      attachInterruptArg(digitalPinToInterrupt(pin_num_),
                        reinterpret_cast<void (*)(void *)>(&DLBusSensor::isr), this, CHANGE);
    }
    frame_buffer_ready_ = false;
    bit_index_ = 0;
    timings_.fill(0);
  }
}

void IRAM_ATTR DLBusSensor::isr(DLBusSensor *arg) {
  auto *self = arg;
  uint32_t now = micros();

  if (self->bit_index_ == 0) {
    self->last_change_ = now;
    return;
  }

  uint32_t delta = now - self->last_change_;
  self->last_change_ = now;
  if (self->bit_index_ < DLBusSensor::MAX_BITS) {
    self->timings_[self->bit_index_++] = static_cast<uint8_t>(std::min(delta, 255u));
    if (self->bit_index_ >= DLBusSensor::MAX_BITS) {
      self->frame_buffer_ready_ = true;
    }
  } else {
    self->frame_buffer_ready_ = true;
  }
}

void DLBusSensor::parse_frame_() {
  ESP_LOGD(TAG, "Start parsing frame with %d bits", bit_index_);
  std::array<uint8_t, 8> bytes{};
  size_t bit_pos = 0;
  for (size_t i = 0; i + 1 < bit_index_ && bit_pos < bytes.size() * 8; i += 2) {
    bool bit = timings_[i] < timings_[i + 1];
    bytes[bit_pos / 8] <<= 1;
    bytes[bit_pos / 8] |= bit ? 1 : 0;
    bit_pos++;
  }

  ESP_LOGD(TAG, "Raw bytes: %02X %02X %02X %02X %02X %02X %02X %02X", bytes[0],
           bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);

  for (int i = 0; i < 4; i++) {
    uint8_t high = bytes[2 * i];
    uint8_t low = bytes[2 * i + 1];
    int16_t raw = static_cast<int16_t>((high << 8) | low);
    if (this->temp_sensors_[i]) {
      this->temp_sensors_[i]->publish_state(raw / 10.0f);
      ESP_LOGD(TAG, "Temp %d: %.1f", i, raw / 10.0f);
    }
  }

  for (int i = 0; i < 4; i++) {
    if (this->relay_sensors_[i]) {
      this->relay_sensors_[i]->publish_state(false);
      ESP_LOGD(TAG, "Relay %d: %s", i, "off");
    }
  }

  ESP_LOGD(TAG, "Parsed DLBus frame from timings");
}

void DLBusSensor::compute_timing_stats_() {
  ESP_LOGD(TAG, "Computing timing stats for %d bits", bit_index_);
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
