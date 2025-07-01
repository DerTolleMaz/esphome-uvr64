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

  // Try clock-synchronized decoding first
  if (decode_sync_(result)) {
    if (debug_)
      ESP_LOGD(TAG, "Clock based decode successful");
    return true;
  }
  if (debug_)
    ESP_LOGD(TAG, "Clock sync failed, falling back to pulse decode");

  uint16_t min_t = UINT16_MAX;
  uint16_t max_t = 0;
  for (uint16_t i = 0; i < bit_index_; i++) {
    uint16_t t = timings_[i];
    if (t < min_t)
      min_t = t;
    if (t > max_t)
      max_t = t;
  }

  double avg_short = static_cast<double>(min_t);
  double avg_long = static_cast<double>(max_t);
  double threshold = (avg_short + avg_long) / 2.0;

  size_t bit_pos = 0;
  size_t errors = 0;

  for (size_t i = 0; i + 1 < bit_index_; i += 2) {
    bool first_long = std::abs(static_cast<double>(timings_[i]) - avg_long) <
                       std::abs(static_cast<double>(timings_[i]) - avg_short);
    if (first_long)
      avg_long = (avg_long * 7.0 + timings_[i]) / 8.0;
    else
      avg_short = (avg_short * 7.0 + timings_[i]) / 8.0;

    threshold = (avg_short + avg_long) / 2.0;

    bool second_long = std::abs(static_cast<double>(timings_[i + 1]) - avg_long) <
                        std::abs(static_cast<double>(timings_[i + 1]) - avg_short);
    if (second_long)
      avg_long = (avg_long * 7.0 + timings_[i + 1]) / 8.0;
    else
      avg_short = (avg_short * 7.0 + timings_[i + 1]) / 8.0;

    threshold = (avg_short + avg_long) / 2.0;

    bool bit;
    if (first_long == second_long) {
      errors++;
      bit = timings_[i] < timings_[i + 1];
      if (debug_)
        ESP_LOGD(TAG,
                 "Invalid Manchester pair at %zu: %d/%d (th=%.1f short=%.1f long=%.1f errors=%zu)",
                 i, timings_[i], timings_[i + 1], threshold, avg_short, avg_long, errors);
      if (errors > 3)
        return false;
    } else {
      bit = second_long;
    }

    if (bit_pos % 8 == 0)
      result.push_back(0);

    result.back() = (result.back() << 1) | (bit ? 1 : 0);
    bit_pos++;
  }

  if (result.empty())
    return false;

  if (result[0] != 0x20) {
    for (size_t idx = 1; idx < result.size(); idx++) {
      if (result[idx] == 0x20 && result.size() - idx >= 17) {
        if (debug_)
          ESP_LOGD(TAG, "Resynchronized at byte %zu", idx);
        result.erase(result.begin(), result.begin() + idx);
        break;
      }
    }
  }

  return !result.empty();
}

bool DLBusSensor::decode_sync_(std::vector<uint8_t> &result) {
  if (bit_index_ < 2)
    return false;

  std::vector<uint32_t> times(bit_index_);
  uint32_t t = 0;
  for (size_t i = 0; i < bit_index_; i++) {
    t += timings_[i];
    times[i] = t;
  }

  size_t sync_idx = SIZE_MAX;
  for (size_t i = 0; i < bit_index_; i++) {
    if (timings_[i] >= SYNC_MIN_US) {
      sync_idx = i;
      break;
    }
  }

  if (sync_idx == SIZE_MAX || sync_idx + 1 >= bit_index_)
    return false;

  const uint32_t BIT_US = 20000;
  const uint32_t SAMPLE_OFFSET_US = 15000;  // middle of second half

  uint32_t frame_start = times[sync_idx];
  size_t edge_ptr = sync_idx + 1;
  bool level = levels_[sync_idx];
  uint32_t next_edge_time = edge_ptr < bit_index_ ? times[edge_ptr] : UINT32_MAX;

  size_t bit_pos = 0;
  uint32_t total_time = times.back();

  while (true) {
    uint32_t sample_time = frame_start + SAMPLE_OFFSET_US + bit_pos * BIT_US;
    if (sample_time >= total_time)
      break;

    while (edge_ptr < bit_index_ && next_edge_time <= sample_time) {
      level = levels_[edge_ptr];
      edge_ptr++;
      next_edge_time = edge_ptr < bit_index_ ? times[edge_ptr] : UINT32_MAX;
    }

    if (bit_pos % 8 == 0)
      result.push_back(0);
    result.back() = (result.back() << 1) | (level ? 1 : 0);

    bit_pos++;
    if (bit_pos > 17 * 8)
      break;
  }

  if (debug_ && !result.empty())
    ESP_LOGD(TAG, "SYNC detected, decoded %zu bits in clock mode", bit_pos);

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
