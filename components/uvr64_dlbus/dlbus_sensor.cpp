#include "dlbus_sensor.h"
#include "esphome/core/log.h"
#ifdef ARDUINO
#include <Arduino.h>
#else
#include "arduino_stubs.h"
#endif

#include <vector>
#include <cstdio>
#include <string>

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

DLBusSensor::DLBusSensor() {}
DLBusSensor::DLBusSensor(uint8_t pin) : pin_num_(pin) {}
DLBusSensor::DLBusSensor(InternalGPIOPin *pin) : pin_(pin) {}

uint8_t DLBusSensor::get_pin() const {
  if (this->pin_ != nullptr)
    return this->pin_->get_pin();
  return this->pin_num_;
}

void DLBusSensor::setup() {
  if (this->pin_ != nullptr) {
    this->pin_->setup();
    this->pin_isr_ = this->pin_->to_isr();
    this->pin_->attach_interrupt(&DLBusSensor::isr, this, gpio::INTERRUPT_ANY_EDGE);
    ESP_LOGI(TAG, "DLBusSensor setup complete, pin %d", this->pin_->get_pin());
  } else {
    pinMode(pin_num_, INPUT);
    attachInterruptArg(digitalPinToInterrupt(pin_num_), reinterpret_cast<void (*)(void *)>(&DLBusSensor::isr), this, CHANGE);
    ESP_LOGI(TAG, "DLBusSensor setup complete, pin %d", pin_num_);
  }
}

void DLBusSensor::loop() {
  uint32_t now = micros();

  if (!frame_buffer_ready_ && bit_index_ > 0 && (now - last_change_) > FRAME_TIMEOUT_US) {
    frame_buffer_ready_ = true;
  }

  if (frame_buffer_ready_) {
    ESP_LOGD(TAG, "Processing frame with %d bits", bit_index_);
    log_bits_();

    if (bit_index_ < 100) {
      ESP_LOGW(TAG, "Frame ignored – too short to be valid.");
      bit_index_ = 0;
      frame_buffer_ready_ = false;
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
      attachInterruptArg(digitalPinToInterrupt(pin_num_), reinterpret_cast<void (*)(void *)>(&DLBusSensor::isr), this, CHANGE);
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
    uint8_t units = (duration + HALF_BIT_US / 2) / HALF_BIT_US;
    if (units == 0)
      units = 1;
    arg->timings_[arg->bit_index_] = units;
    arg->bit_index_++;
  }
}

void DLBusSensor::parse_frame_() {
  std::vector<uint8_t> bytes;
  uint8_t current_byte = 0;
  uint8_t bit_count = 0;

  for (size_t i = 0; i + 1 < bit_index_; i += 2) {
    uint8_t first = timings_[i];
    uint8_t second = timings_[i + 1];
    if (first == second)
      return;  // Invalid Manchester sequence

    bool bit = first < second;
    current_byte = (current_byte << 1) | (bit ? 1 : 0);
    bit_count++;

    if (bit_count == 8) {
      bytes.push_back(current_byte);
      current_byte = 0;
      bit_count = 0;
    }
  }

  log_frame_(bytes);

  if (bytes.size() < 16) {
    ESP_LOGW(TAG, "Frame too short.");
    return;
  }

  for (int t = 0; t < 6; t++) {
    uint8_t high = bytes[t * 2];
    uint8_t low = bytes[t * 2 + 1];
    int16_t raw = (static_cast<int16_t>(high) << 8) | low;
    if (this->temp_sensors_[t]) {
      this->temp_sensors_[t]->publish_state(raw / 10.0f);
      ESP_LOGI(TAG, "Temperature Sensor %d: %.1f°C (Raw: %d)", t, raw / 10.0f, raw);
    }
  }

  for (int r = 0; r < 4; r++) {
    uint8_t val = bytes[12 + r];
    bool state = val != 0;
    if (this->relay_sensors_[r]) {
      this->relay_sensors_[r]->publish_state(state);
      ESP_LOGI(TAG, "Relay %d: %s (Raw: %d)", r, state ? "ON" : "OFF", val);
    }
  }
}


void DLBusSensor::log_bits_() {
  std::string bitstring;
  for (size_t i = 0; i < bit_index_; i++) {
    bitstring += std::to_string(timings_[i]) + " ";
  }
  ESP_LOGI(TAG, "Bitstream (%d bits): %s", (int)bit_index_, bitstring.c_str());
}

void DLBusSensor::log_frame_(const std::vector<uint8_t> &frame) {
  char buf[512];
  std::string out;
  for (size_t i = 0; i < frame.size(); i++) {
    snprintf(buf, sizeof(buf), "%02X ", frame[i]);
    out += buf;
  }
  ESP_LOGI(TAG, "Frame Bytes:\n%s", out.c_str());
}

}  // namespace uvr64_dlbus
}  // namespace esphome
