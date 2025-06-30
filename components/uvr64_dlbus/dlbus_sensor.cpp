#include "dlbus_sensor.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <vector>
#include <string>
#include <cstdio>

#if __has_include(<Arduino.h>)
#include <Arduino.h>
#else
#include "arduino_stubs.h"
#endif

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

DLBusSensor::DLBusSensor() {
  this->bit_index_ = 0;
  this->timings_.fill(0);
  this->levels_.fill(0);
}

DLBusSensor::DLBusSensor(uint8_t pin) : pin_num_(pin) {
  this->bit_index_ = 0;
  this->timings_.fill(0);
  this->levels_.fill(0);
}

DLBusSensor::DLBusSensor(InternalGPIOPin *pin) : pin_(pin) {
  this->bit_index_ = 0;
  this->timings_.fill(0);
  this->levels_.fill(0);
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
  uint32_t now = micros();

  if (!frame_buffer_ready_ && bit_index_ > 0 && (now - last_change_) > FRAME_TIMEOUT_US) {
    frame_buffer_ready_ = true;
  }

  if (frame_buffer_ready_) {
    ESP_LOGD(TAG, "Processing frame with %d bits", bit_index_);

    if (bit_index_ < 100) {
      ESP_LOGW(TAG, "Frame ignored – too short to be valid.");
      log_bits_();
      frame_buffer_ready_ = false;
      bit_index_ = 0;
      return;
    }

    if (this->pin_ != nullptr) {
      this->pin_->detach_interrupt();
    } else {
      detachInterrupt(digitalPinToInterrupt(pin_num_));
    }

    parse_frame_();

    if (this->pin_ != nullptr) {
      this->pin_->attach_interrupt(&DLBusSensor::isr, this, gpio::INTERRUPT_ANY_EDGE);
    } else {
      attachInterruptArg(digitalPinToInterrupt(pin_num_),
                         reinterpret_cast<void (*)(void *)>(&DLBusSensor::isr), this, CHANGE);
    }

    frame_buffer_ready_ = false;
    bit_index_ = 0;
  }
}

void IRAM_ATTR DLBusSensor::isr(DLBusSensor *arg) {
  uint32_t now = micros();
  uint8_t level = arg->pin_isr_.digital_read();
  uint32_t duration = now - arg->last_change_;
  arg->last_change_ = now;

  if (arg->bit_index_ < MAX_BITS) {
    arg->levels_[arg->bit_index_] = level;
    arg->timings_[arg->bit_index_] = duration > 255 ? 255 : duration;
    arg->bit_index_++;
  }
}

void DLBusSensor::log_bits_() {
  std::string out;
  for (size_t i = 0; i < bit_index_; i++) {
    out += levels_[i] ? '1' : '0';
    if ((i + 1) % 8 == 0)
      out += ' ';
  }
  ESP_LOGI(TAG, "Bits: %s", out.c_str());
}

void DLBusSensor::log_frame_(const std::vector<uint8_t> &frame) {
  std::string out;
  for (size_t i = 0; i < frame.size(); i++) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02X ", frame[i]);
    out += buf;
    if ((i + 1) % 8 == 0)
      out += '\n';
  }
  ESP_LOGI(TAG, "Frame Bytes:\n%s", out.c_str());
}

void DLBusSensor::parse_frame_() {
  std::vector<uint8_t> bytes;
  size_t bit_pos = 0;

  // Manchester-Dekodierung
  for (size_t i = 0; i + 1 < bit_index_; i += 2) {
    bool bit = timings_[i] < timings_[i + 1];
    if (bit_pos % 8 == 0) {
      bytes.push_back(0);
    }
    bytes.back() = (bytes.back() << 1) | (bit ? 1 : 0);
    bit_pos++;
  }

  log_frame_(bytes);

  if (bytes.size() < 16) {
    ESP_LOGW(TAG, "Frame too short");
    return;
  }

  // Temperatur-Daten (Little Endian: Low + High)
  for (int t = 0; t < 6; t++) {
    uint8_t low = bytes[t * 2 + 1];
    uint8_t high = bytes[t * 2];
    int16_t raw = (static_cast<int16_t>(high) << 8) | low;
    float temp = raw / 10.0f;
    if (this->temp_sensors_[t]) {
      ESP_LOGI(TAG, "Temperature Sensor %d: %.1f°C (Raw: %d)", t, temp, raw);
      this->temp_sensors_[t]->publish_state(temp);
    }
  }

  // Relais-Daten
  for (int r = 0; r < 4; r++) {
    uint8_t val = bytes[12 + r];
    bool state = val != 0;
    ESP_LOGI(TAG, "Relay %d: %s (Raw: %d)", r, state ? "ON" : "OFF", val);
    if (this->relay_sensors_[r]) {
      this->relay_sensors_[r]->publish_state(state);
    }
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
