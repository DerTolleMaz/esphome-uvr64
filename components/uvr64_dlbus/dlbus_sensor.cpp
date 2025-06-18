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

  if (timing_len < 2) {
    ESP_LOGW(TAG, "Too few edge timings for decoding");
    return;
  }

  // Optional: Logge die ersten Timing-Werte
  ESP_LOGI(TAG, "Timing sequence (%zu edges):", timing_len);
  for (size_t i = 0; i < std::min(timing_len, size_t(33)); i++) {
    ESP_LOGI(TAG, "  timings[%03zu] = %3u µs", i, this->timings_[i]);
  }

  // --- Pulsweitenbasierte Bit-Dekodierung ---
  std::vector<bool> bits;
  int zero_count = 0, one_count = 0, skip_count = 0;

  for (size_t i = 0; i + 1 < timing_len; i += 2) {
    uint32_t t1 = this->timings_[i];
    uint32_t t2 = this->timings_[i + 1];

    if (t1 < 20 && t2 > 40) {
      bits.push_back(false);  // 0
      zero_count++;
    } else if (t1 > 40 && t2 < 20) {
      bits.push_back(true);   // 1
      one_count++;
    } else {
      skip_count++;
      ESP_LOGV(TAG, "Skipping unplausible timings t1=%u, t2=%u", t1, t2);
      continue;
    }

    ESP_LOGV(TAG, "Bit Count %zu (t1=%u, t2=%u): %d", bits.size(), t1, t2, bits.back());
  }

  ESP_LOGI(TAG, "Decoded %zu bits: %dx 0, %dx 1, %dx skipped", bits.size(), zero_count, one_count, skip_count);

  if (bits.size() < 64) {
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
