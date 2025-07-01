#include "dlbus_sensor.h"
#include "esphome/core/log.h"
#include <vector>
#include <cstdio>

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

void IRAM_ATTR DLBusSensor::gpio_isr_(DLBusSensor *sensor) {
  const uint32_t now = micros();
  const bool level = sensor->pin_isr_.digital_read();
  const uint32_t duration = now - sensor->last_change_;
  sensor->last_change_ = now;

  if (sensor->bit_index_ < 1024) {
    sensor->levels_[sensor->bit_index_] = level;
    sensor->timings_[sensor->bit_index_] = duration > 65535 ? 65535 : duration;
    sensor->bit_index_++;
  }
}

void DLBusSensor::setup() {
  if (this->pin_ != nullptr) {
    this->pin_->setup();
    this->pin_isr_ = this->pin_->to_isr();
    this->pin_->attach_interrupt(&DLBusSensor::gpio_isr_, this, gpio::INTERRUPT_ANY_EDGE);
    ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", this->pin_->get_pin());
  } else {
    pinMode(pin_num_, INPUT);
    attachInterruptArg(digitalPinToInterrupt(pin_num_),
                       reinterpret_cast<void (*)(void *)>(&DLBusSensor::gpio_isr_), this, CHANGE);
    ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", pin_num_);
  }
}

void DLBusSensor::loop() {
  const uint32_t now = micros();

  // Check for frame timeout
  if (bit_index_ > 0 && (now - last_change_) > 500000) {  // 500ms timeout
    ESP_LOGD(TAG, "Processing frame with %d bits", bit_index_);

    // Disable interrupts during processing
    if (this->pin_ != nullptr) {
      this->pin_->detach_interrupt();
    } else {
      detachInterrupt(digitalPinToInterrupt(pin_num_));
    }

    process_frame_();

    // Reset buffer
    bit_index_ = 0;

    // Reattach interrupts
    if (this->pin_ != nullptr) {
      this->pin_->attach_interrupt(&DLBusSensor::gpio_isr_, this, gpio::INTERRUPT_ANY_EDGE);
    } else {
      attachInterruptArg(digitalPinToInterrupt(pin_num_),
                         reinterpret_cast<void (*)(void *)>(&DLBusSensor::gpio_isr_), this, CHANGE);
    }
  }
}

void DLBusSensor::process_frame_() {
  if (bit_index_ < 100) {
    ESP_LOGW(TAG, "Frame ignored – too short.");
    return;
  }

  std::vector<uint8_t> decoded_bytes;
  bool ok = decode_manchester_(decoded_bytes);

  if (!ok) {
    ESP_LOGW(TAG, "Manchester decode failed");
    return;
  }

  log_frame_(decoded_bytes);

  if (decoded_bytes.size() < 16) {
    ESP_LOGW(TAG, "Frame too short to parse.");
    return;
  }

  // Parse temperature values
  for (int t = 0; t < 6; t++) {
    uint8_t high = decoded_bytes[t * 2];
    uint8_t low = decoded_bytes[t * 2 + 1];
    int16_t raw = (static_cast<int16_t>(high) << 8) | low;
    float temp = raw / 10.0f;
    if (this->temp_sensors_[t]) {
      this->temp_sensors_[t]->publish_state(temp);
      ESP_LOGI(TAG, "Temperature Sensor %d: %.1f°C (Raw: %d)", t, temp, raw);
    }
  }

  // Parse relay states
  for (int r = 0; r < 4; r++) {
    uint8_t val = decoded_bytes[12 + r];
    bool state = val != 0;
    if (this->relay_sensors_[r]) {
      this->relay_sensors_[r]->publish_state(state);
      ESP_LOGI(TAG, "Relay %d: %s (Raw: %d)", r, state ? "ON" : "OFF", val);
    }
  }
}

bool DLBusSensor::decode_manchester_(std::vector<uint8_t> &result) {
  size_t sync_pos = SIZE_MAX;

  // Look for 16 consecutive '1' as SYNC
  size_t count = 0;
  for (size_t i = 0; i < bit_index_; i++) {
    if (levels_[i] == 1) {
      count++;
      if (count >= 16) {
        sync_pos = i + 1;
        break;
      }
    } else {
      count = 0;
    }
  }

  if (sync_pos == SIZE_MAX) {
    ESP_LOGW(TAG, "No SYNC found!");
    return false;
  }

  ESP_LOGI(TAG, "SYNC found at bit %d", (int)sync_pos);

  // Manchester decoding (pairs of bits)
  size_t bit_pos = sync_pos;
  uint8_t current_byte = 0;
  int bit_in_byte = 0;

  while (bit_pos + 1 < bit_index_) {
    bool first = levels_[bit_pos];
    bool second = levels_[bit_pos + 1];

    if (first == second) {
      ESP_LOGW(TAG, "Manchester error at bit %d-%d", (int)bit_pos, (int)bit_pos + 1);
      return false;
    }

    bool decoded_bit = (first == 0) ? 1 : 0;
    current_byte = (current_byte << 1) | decoded_bit;
    bit_in_byte++;

    if (bit_in_byte == 8) {
      result.push_back(current_byte);
      current_byte = 0;
      bit_in_byte = 0;
    }

    bit_pos += 2;
  }

  if (bit_in_
