// MIT License - see LICENSE file in the project root for full details.
#include "dlbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

void DLBusSensor::setup() {
  pinMode(pin_, INPUT);
  last_edge_ = micros();
  attachInterruptArg(digitalPinToInterrupt(pin_), isr, this, CHANGE);
  ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", pin_);
}

void DLBusSensor::update() {
  if (frame_ready_) {
    ESP_LOGD(TAG, "DLBus frame received, decoding...");
    parse_frame_();
    bit_index_ = 0;
    last_edge_ = micros();
    attachInterruptArg(digitalPinToInterrupt(pin_), isr, this, CHANGE);
    frame_ready_ = false;
  }
}

void DLBusSensor::loop() {
 // if (frame_ready_) {
 //   ESP_LOGD("uvr64_dlbus", "DLBus frame received, decoding...");
 //   parse_frame_();
 //   bit_index_ = 0;
 //   last_edge_ = micros();
 //   attachInterruptArg(digitalPinToInterrupt(pin_), isr, this, CHANGE);
 //   frame_ready_ = false;
 // }
}

void DLBusSensor::set_temp_sensor(int index, sensor::Sensor *sensor) {
  if (index >= 0 && index < 6)
    temp_sensors_[index] = sensor;
}

void DLBusSensor::set_relay_sensor(int index, binary_sensor::BinarySensor *sensor) {
  if (index >= 0 && index < 4)
    relay_sensors_[index] = sensor;
}

void IRAM_ATTR DLBusSensor::isr(void *arg) {
  auto *self = static_cast<DLBusSensor *>(arg);
  unsigned long now = micros();
  uint32_t duration = now - self->last_edge_;
  self->last_edge_ = now;
  if (self->bit_index_ < MAX_BITS) {
    self->timings_[self->bit_index_++] = duration;
  } else {
    self->frame_ready_ = true;
    detachInterrupt(digitalPinToInterrupt(self->pin_));
  }
}

void DLBusSensor::parse_frame_() {
  constexpr size_t timing_len = 128;

  ESP_LOGD(TAG, "DLBus frame received (update), decoding...");

 std::vector<bool> bits;
  bits.reserve(64);

  ESP_LOGD(TAG, "DLBus frame received (update), decoding...");
  //ESP_LOGI(TAG, "Timing sequence (%zu edges):", timing_len);
  //for (size_t i = 0; i < std::min(timing_len, size_t(33)); i++) {
  //  ESP_LOGI(TAG, "  timings[%03zu] = %3u µs", i, this->timings_[i]);
  //}

  for (size_t i = 0; i + 1 < timing_len; i += 2) {
    uint32_t t1 = this->timings_[i];
    uint32_t t2 = this->timings_[i + 1];

    // harter Abbruch bei offensichtlicher Trennung
    if (t1 > 3000 || t2 > 3000) {
      ESP_LOGI(TAG, "Aborting decode: framing break at i=%zu (t1=%u µs, t2=%u µs)", i, t1, t2);
      break;
    }

    uint32_t total = t1 + t2;
    if (total < 120 || total > 220) {
      ESP_LOGV(TAG, "Skipping unplausible timings t1=%u, t2=%u", t1, t2);
      continue;
    }

    // Manchester decoding: transition in middle = valid bit
    if (t1 < t2) {
      bits.push_back(1);  // 0->1
    } else {
      bits.push_back(0);  // 1->0
    }

    ESP_LOGV(TAG, "Bit Count %zu", bits.size());
  }

  ESP_LOGI(TAG, "Decoded bit count: %zu", bits.size());
  if (bits.size() < 32) {
    ESP_LOGW(TAG, "Decoded too few bits: %zu", bits.size());
    return;
  }

  // --- Bits → Bytes ---
  uint8_t raw_bytes[16] = {0};
  int max_bytes = std::min((int)bits.size() / 8, 16);

  for (int i = 0; i < max_bytes; i++) {
    for (int b = 0; b < 8; b++) {
      raw_bytes[i] <<= 1;
      raw_bytes[i] |= bits[i * 8 + b] ? 1 : 0;
    }
  }

  // Hex-Dump
  char hexbuf[3 * 16 + 1] = {0};
  for (int i = 0; i < max_bytes; i++) {
    sprintf(hexbuf + i * 3, "%02X ", raw_bytes[i]);
  }
  ESP_LOGI(TAG, "Raw Bytes: %s", hexbuf);

  // TODO: DL-Bus-Inhalt auswerten
}


}  // namespace uvr64_dlbus
}  // namespace esphome
