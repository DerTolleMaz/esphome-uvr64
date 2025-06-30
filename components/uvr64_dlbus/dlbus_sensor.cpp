#include "dlbus_sensor.h"
#include "esphome/core/log.h"

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
    arg->timings_[arg->bit_index_] = duration > 255 ? 255 : duration;
    arg->bit_index_++;
  }
}

void DLBusSensor::parse_frame_() {
  std::vector<bool> bits;
  for (size_t i = 0; i < bit_index_; i++) {
    bits.push_back(levels_[i]);
  }

  size_t sync_pos = 0;
  if (!find_sync_(bits, sync_pos)) {
    ESP_LOGW(TAG, "No SYNC found!");
    return;
  }

  ESP_LOGI(TAG, "SYNC found at bit %d", (int)sync_pos);

  std::vector<bool> data_bits(bits.begin() + sync_pos + 16, bits.end());

  std::vector<uint8_t> bytes;
  if (!decode_manchester_(data_bits, bytes)) {
    ESP_LOGW(TAG, "Manchester decode failed");
    return;
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

bool DLBusSensor::find_sync_(const std::vector<bool> &bits, size_t &sync_pos) {
  for (size_t i = 0; i + 16 < bits.size(); i++) {
    bool sync = true;
    for (size_t j = 0; j < 16; j++) {
      if (!bits[i + j]) {
        sync = false;
        break;
      }
    }
    if (sync) {
      sync_pos = i;
      return true;
    }
  }
  return false;
}

bool DLBusSensor::decode_manchester_(std::vector<bool> &bits, std::vector<uint8_t> &bytes) {
  if (bits.size() < 2)
    return false;
  size_t bit_pos = 0;
  while (bit_pos + 1 < bits.size()) {
    bool first = bits[bit_pos];
    bool second = bits[bit_pos + 1];
    bit_pos += 2;

    if (first == second) {
      ESP_LOGW(TAG, "Manchester error at bit %d-%d", (int)bit_pos - 2, (int)bit_pos - 1);
      return false;
    }

    bool bit = !first && second;

    if (bytes.empty() || (bit_pos / 2 - 1) % 8 == 0)
      bytes.push_back(0);

    bytes.back() = (bytes.back() << 1) | (bit ? 1 : 0);
  }
  return true;
}

void DLBusSensor::log_bits_() {
  std::string bitstring;
  for (size_t i = 0; i < bit_index_; i++) {
    bitstring += levels_[i] ? '1' : '0';
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
