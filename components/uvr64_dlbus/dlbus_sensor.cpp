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
  uint32_t duration = now - sensor->last_change_;
  sensor->last_change_ = now;

  if (sensor->bit_index_ < MAX_BITS) {
    sensor->levels_[sensor->bit_index_] = level;
    sensor->timings_[sensor->bit_index_] = (duration > 255) ? 255 : duration;
    sensor->bit_index_++;
  }
}

void DLBusSensor::process_frame_() {
  std::vector<uint8_t> decoded_bytes;

  bool ok = decode_manchester_(decoded_bytes);
  if (!ok) {
    ESP_LOGW(TAG, "Manchester decode failed");
    log_bits_();
    return;
  }

  log_frame_(decoded_bytes);
  parse_frame_(decoded_bytes);
}

void DLBusSensor::parse_frame_() {
  std::vector<uint8_t> decoded;
  if (!decode_manchester_(decoded)) {
    ESP_LOGW(TAG, "Manchester decode failed");
    log_bits_();
    return;
  }
  log_frame_(decoded);
  parse_frame_(decoded);
}

bool DLBusSensor::decode_manchester_(std::vector<uint8_t> &result) {
  result.clear();
  size_t bit_pos = 0;

  // Manchester-Dekodierung: 2 Pegel = 1 Bit
  for (size_t i = 0; i + 1 < bit_index_; i += 2) {
    bool bit = timings_[i] < timings_[i + 1];

    if (bit_pos % 8 == 0) {
      result.push_back(0);
    }

    result.back() = (result.back() << 1) | (bit ? 1 : 0);
    bit_pos++;
  }

  return !result.empty();
}

void DLBusSensor::parse_frame_(const std::vector<uint8_t> &frame) {
  if (frame.size() < 16) {
    ESP_LOGW(TAG, "Frame too short for parsing.");
    return;
  }

  for (int i = 0; i < 6; i++) {
    uint8_t high = frame[i * 2];
    uint8_t low = frame[i * 2 + 1];
    int16_t raw = (static_cast<int16_t>(high) << 8) | low;
    if (temp_sensors_[i] != nullptr) {
      temp_sensors_[i]->publish_state(raw / 10.0f);
      ESP_LOGI(TAG, "Temperature Sensor %d: %.1f°C (Raw: %d)", i, raw / 10.0f, raw);
    }
  }

  for (int r = 0; r < 4; r++) {
    uint8_t val = frame[12 + r];
    bool state = val != 0;
    if (relay_sensors_[r] != nullptr) {
      relay_sensors_[r]->publish_state(state);
      ESP_LOGI(TAG, "Relay %d: %s (Raw: %d)", r, state ? "ON" : "OFF", val);
    }
  }
}

void DLBusSensor::log_bits_() {
  std::string dump;
  for (size_t i = 0; i < bit_index_; i++) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%d ", timings_[i], levels_[i]);
    dump += buf;
  }
  ESP_LOGI(TAG, "Bitstream: %s", dump.c_str());
}

void DLBusSensor::log_frame_(const std::vector<uint8_t> &frame) {
  std::string out;
  for (auto b : frame) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02X ", b);
    out += buf;
  }
  ESP_LOGI(TAG, "Frame Bytes: %s", out.c_str());
}

}  // namespace uvr64_dlbus
}  // namespace esphome
