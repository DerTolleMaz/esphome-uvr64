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
  constexpr size_t timing_len = DLBUS_TIMING_BUFFER_SIZE;  // normalerweise 128

  ESP_LOGI(TAG, "Timing sequence (%zu edges):", timing_len);
  //for (size_t i = 0; i < std::min(timing_len, size_t(33)); i++) {
  //  ESP_LOGI(TAG, "  timings[%03zu] = %3u µs", i, this->timings_[i]);
 // }

  std::vector<bool> bits;
  size_t decoded = 0;

  for (size_t i = 0; i + 1 < timing_len; i += 2) {
    uint32_t t1 = this->timings_[i];
    uint32_t t2 = this->timings_[i + 1];

    // Manchester-Zerlegung: t1 + t2 = Bitlänge
    if (t1 < 4 || t2 < 4 || (t1 + t2) < 60 || (t1 + t2) > 280) {
      ESP_LOGV(TAG, "Skipping unplausible timings t1=%u, t2=%u", t1, t2);
      continue;
    }

    if (t2 > 5000) {
      ESP_LOGI(TAG, "Aborting decode: framing break at i=%zu (t1=%u µs, t2=%u µs)", i, t1, t2);
      break;
    }

    bool bit;
    if (t1 < t2)
      bit = false;  // 01 = 0
    else
      bit = true;   // 10 = 1

    bits.push_back(bit);
    decoded++;
    ESP_LOGV(TAG, "Bit Count %zu", decoded);
  }

  ESP_LOGI(TAG, "Decoded bit count: %zu", decoded);
  if (decoded < 64) {
    ESP_LOGW(TAG, "Decoded too few bits: %zu", decoded);
    return;
  }

  // Bitfolge als String anzeigen
  std::string bitstr;
  for (bool bit : bits) bitstr += bit ? "1" : "0";
  ESP_LOGI(TAG, "Bitstream: %s", bitstr.c_str());

  // Versuche, bis zu 12 Byte daraus zu machen (max. 96 Bit)
  size_t byte_len = std::min(bits.size() / 8, size_t(12));
  std::vector<uint8_t> raw_bytes(byte_len, 0);
  for (size_t i = 0; i < byte_len; i++) {
    for (int b = 0; b < 8; b++) {
      if (bits[i * 8 + b])
        raw_bytes[i] |= (1 << (7 - b));
    }
  }

  ESP_LOGI(TAG, "DLBus Raw: %s", format_hex_pretty(raw_bytes).c_str());

  // → hier könntest du die raw_bytes an die eigentliche Frame-Decoder-Logik übergeben
}


}  // namespace uvr64_dlbus
}  // namespace esphome
