#include "dlbus_sensor.h"
#include "esphome/core/log.h"
#ifdef ARDUINO
#include <Arduino.h>
#else
#include "arduino_stubs.h"
#endif
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

void DLBusSensor::setup() {
  if (this->pin_ == nullptr) {
    ESP_LOGW(TAG, "No pin configured for DLBusSensor");
    return;
  }
  this->pin_->setup();
  this->pin_->pin_mode(gpio::FLAG_INPUT);
  this->pin_->attach_interrupt(&DLBusSensor::gpio_isr_, this,
                               gpio::INTERRUPT_ANY_EDGE);
  ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", this->pin_->get_pin());
}

void DLBusSensor::loop() {
  uint32_t now = micros();

  if (!frame_buffer_ready_ && bit_index_ > 0 && (now - last_change_) > FRAME_TIMEOUT_US) {
    frame_buffer_ready_ = true;
  }

  if (frame_buffer_ready_) {
    ESP_LOGD(TAG, "Processing frame with %d bits", static_cast<int>(bit_index_));

    if (bit_index_ < 100) {
      ESP_LOGW(TAG, "Frame ignored – too short to be valid.");
      log_bits_();
      bit_index_ = 0;
      frame_buffer_ready_ = false;
      return;
    }

    this->pin_->detach_interrupt();

    process_frame_();

    this->pin_->attach_interrupt(&DLBusSensor::gpio_isr_, this,
                                 gpio::INTERRUPT_ANY_EDGE);

    frame_buffer_ready_ = false;
    bit_index_ = 0;
  }
}

void IRAM_ATTR DLBusSensor::gpio_isr_(DLBusSensor *sensor) {
  uint32_t now = micros();
  bool level = sensor->pin_->digital_read();
  uint32_t diff = now - sensor->last_change_;
  uint16_t duration = diff > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(diff);
  sensor->last_change_ = now;

  if (sensor->bit_index_ < MAX_BITS) {
    sensor->levels_[sensor->bit_index_] = level;
    sensor->timings_[sensor->bit_index_] = duration;
    sensor->bit_index_++;
  }
}

void DLBusSensor::process_frame_() {
  // Log the raw pin signal before attempting to decode so the user can see the
  // exact timings and logic levels observed on the DLBus pin.
  log_bits_();

  std::vector<uint8_t> decoded_bytes;

  bool ok = decode_manchester_(decoded_bytes);
  if (!ok) {
    ESP_LOGW(TAG, "Manchester decode failed");
    return;
  }

  log_frame_(decoded_bytes);
  if (!validate_frame_(decoded_bytes)) {
    return;
  }
  parse_frame_(decoded_bytes);
}

void DLBusSensor::parse_frame_() {
  // Provide the raw bitstream for debugging before decoding.
  log_bits_();

  std::vector<uint8_t> decoded;
  if (!decode_manchester_(decoded)) {
    ESP_LOGW(TAG, "Manchester decode failed");
    return;
  }
  log_frame_(decoded);
  if (!validate_frame_(decoded)) {
    return;
  }
  parse_frame_(decoded);
}

bool DLBusSensor::decode_manchester_(std::vector<uint8_t> &result) {
  result.clear();
  if (bit_index_ < 2)
    return false;

  uint16_t min_t = UINT16_MAX;
  uint16_t max_t = 0;
  for (uint16_t i = 0; i < bit_index_; i++) {
    uint16_t t = timings_[i];
    if (t < min_t)
      min_t = t;
    if (t > max_t)
      max_t = t;
  }

  double threshold = (static_cast<double>(min_t) + static_cast<double>(max_t)) / 2.0;
  size_t bit_pos = 0;
  for (size_t i = 0; i + 1 < bit_index_; i += 2) {
    bool first_long = static_cast<double>(timings_[i]) >= threshold;
    bool second_long = static_cast<double>(timings_[i + 1]) >= threshold;

    if (first_long == second_long) {
      if (debug_)
        ESP_LOGD(TAG, "Invalid Manchester pair at %zu: %d/%d", i, timings_[i], timings_[i + 1]);
      return false;
    }

    bool bit = second_long;
    if (bit_pos % 8 == 0)
      result.push_back(0);

    result.back() = (result.back() << 1) | (bit ? 1 : 0);
    bit_pos++;
  }

  return !result.empty();
}

void DLBusSensor::parse_frame_(const std::vector<uint8_t> &frame) {
  if (!validate_frame_(frame)) {
    return;
  }

  const size_t offset = 1;  // skip device id

  for (int i = 0; i < 6; i++) {
    uint8_t high = frame[offset + i * 2];
    uint8_t low = frame[offset + i * 2 + 1];
    int16_t raw = (static_cast<int16_t>(high) << 8) | low;
    if (temp_sensors_[i] != nullptr) {
      temp_sensors_[i]->publish_state(raw / 10.0f);
      ESP_LOGI(TAG, "Temperature Sensor %d: %.1f°C (Raw: %d)", i, raw / 10.0f, raw);
    }
  }

  for (int r = 0; r < 4; r++) {
    uint8_t val = frame[offset + 12 + r];
    bool state = val != 0;
    if (relay_sensors_[r] != nullptr) {
      relay_sensors_[r]->publish_state(state);
      ESP_LOGI(TAG, "Relay %d: %s (Raw: %d)", r, state ? "ON" : "OFF", val);
    }
  }
}

void DLBusSensor::log_bits_() {
  if (!debug_)
    return;
  std::string dump;
  for (size_t i = 0; i < bit_index_; i++) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%d ", timings_[i], levels_[i]);
    dump += buf;
  }
  ESP_LOGD(TAG, "Bitstream: %s", dump.c_str());
}

void DLBusSensor::log_frame_(const std::vector<uint8_t> &frame) {
  if (!debug_)
    return;
  std::string out;
  for (auto b : frame) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02X ", b);
    out += buf;
  }
  ESP_LOGD(TAG, "Bus Bytes: %s", out.c_str());
}

bool DLBusSensor::validate_frame_(const std::vector<uint8_t> &frame) {
  bool valid = true;
  if (frame.size() != 17) {
    ESP_LOGW(TAG, "Frame length invalid: %u", static_cast<unsigned>(frame.size()));
    valid = false;
  } else if (frame[0] != 0x20) {
    ESP_LOGW(TAG, "Invalid device id: 0x%02X", frame[0]);
    valid = false;
  }

  if (!valid) {
    std::string out;
    for (auto b : frame) {
      char buf[8];
      snprintf(buf, sizeof(buf), "%02X ", b);
      out += buf;
    }
    ESP_LOGW(TAG, "Invalid frame content: %s", out.c_str());
  }

  return valid;
}

}  // namespace uvr64_dlbus
}  // namespace esphome
