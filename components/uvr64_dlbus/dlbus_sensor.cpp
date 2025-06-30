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

  // Wenn seit der letzten Flanke länger als FRAME_TIMEOUT vergangen ist → Frame fertig
  if (!frame_buffer_ready_ && bit_index_ > 0 && (now - last_change_) > FRAME_TIMEOUT_US) {
    frame_buffer_ready_ = true;
  }

  if (frame_buffer_ready_) {
    ESP_LOGD(TAG, "Processing frame with %d bits", bit_index_);

    // 🛑 Sicherheits-Filter: Ignoriere Frames mit weniger als 100 Bits
    if (bit_index_ < 100) {
      ESP_LOGW(TAG, "Frame ignored – too short to be valid.");
      // Signal-Dump für Debugging
      std::string signal_dump;
      for (size_t k = 0; k < bit_index_; k++) {
        char buf[10];
        snprintf(buf, sizeof(buf), "%d:%d ", timings_[k], levels_[k]);
        signal_dump += buf;
      }
      ESP_LOGI(TAG, "Signal: %s", signal_dump.c_str());

      frame_buffer_ready_ = false;
      bit_index_ = 0;
      return;
    }

    // Interrupt deaktivieren während Verarbeitung
    if (this->pin_ != nullptr) {
      this->pin_->detach_interrupt();
    } else {
      detachInterrupt(digitalPinToInterrupt(pin_num_));
    }

    // 🔥 Verarbeitung des Frames
    parse_frame_();

    // Interrupt wieder aktivieren
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

  if (bytes.size() < 16) {
    ESP_LOGW(TAG, "Frame too short");
    return;
  }

  // Temperature values (big endian high/low for each sensor)
  for (int t = 0; t < 6; t++) {
    uint8_t high = bytes[t * 2];
    uint8_t low = bytes[t * 2 + 1];
    int16_t raw = (static_cast<int16_t>(high) << 8) | low;
    if (this->temp_sensors_[t]) {
      this->temp_sensors_[t]->publish_state(raw / 10.0f);
    }
  }

  // Relay states follow after the six temperature pairs
  for (int r = 0; r < 4; r++) {
    uint8_t val = bytes[12 + r];
    bool state = val != 0;
    if (this->relay_sensors_[r]) {
      this->relay_sensors_[r]->publish_state(state);
    }
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
