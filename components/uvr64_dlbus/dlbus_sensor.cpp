// MIT License - see LICENSE file in the project root for full details.
#include "dlbus_sensor.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>
#include <vector>

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

  // Adaptive Software-Entprellung
  if (duration < (self->min_valid_timing_ / 2)) return;

  self->last_edge_ = now;

  if (self->bit_index_ < MAX_BITS) {
    self->timings_[self->bit_index_++] = duration;
  } else {
    self->frame_ready_ = true;
    detachInterrupt(digitalPinToInterrupt(self->pin_));
  }
}

void DLBusSensor::compute_timing_stats_(uint32_t &median, float &mean, float &stddev, uint32_t &min_t, uint32_t &max_t) {
  std::vector<uint32_t> sorted_timings(timings_, timings_ + bit_index_);
  std::sort(sorted_timings.begin(), sorted_timings.end());

  min_t = UINT32_MAX;
  max_t = 0;
  uint64_t sum = 0;

  for (uint32_t t : sorted_timings) {
    min_t = std::min(min_t, t);
    max_t = std::max(max_t, t);
    sum += t;
  }

  median = sorted_timings[bit_index_ / 2];
  mean = static_cast<float>(sum) / bit_index_;

  float variance = 0;
  for (auto t : sorted_timings)
    variance += (t - mean) * (t - mean);
  stddev = sqrt(variance / bit_index_);

  // Histogramm der Bitlängen
  const int BIN_COUNT = 10;
  const uint32_t BIN_WIDTH = 2000;  // µs
  int histogram[BIN_COUNT] = {0};
  int overflows = 0;
  
  for (int i = 0; i < bit_index_; i++) {
    uint32_t t = timings_[i];
    int bin = t / BIN_WIDTH;
    if (bin >= BIN_COUNT)
      overflows++;
    else
      histogram[bin]++;
}

// Ausgabe des Histogramms
ESP_LOGI(TAG, "Bit Timing Histogram (Bin width: %u µs):", BIN_WIDTH);
for (int i = 0; i < BIN_COUNT; i++) {
  ESP_LOGI(TAG, "  %2u - %2u ms: %3d", i * 2, (i + 1) * 2 - 1, histogram[i]);
}
if (overflows > 0)
  ESP_LOGI(TAG, "  >%u ms: %3d", BIN_COUNT * 2, overflows);
  
}

void DLBusSensor::parse_frame_() {
  if (bit_index_ < 80) {
    ESP_LOGW(TAG, "Received frame too short: %d bits", bit_index_);
    return;
  }

  uint32_t median, min_t, max_t;
  float mean, stddev;
  compute_timing_stats_(median, mean, stddev, min_t, max_t);
  this->min_valid_timing_ = min_t;

  ESP_LOGI(TAG, "Bit timing – Min: %u µs, Max: %u µs, Median: %u µs, Mean: %.1f µs, StdDev: %.1f",
           min_t, max_t, median, mean, stddev);

  uint32_t threshold = median;
  uint8_t raw_bytes[16] = {0};
  int byte_i = 0, bit_i = 0;

  for (int i = 0; i < bit_index_; i++) {
    if (timings_[i] < 100 || timings_[i] > 50000) {
      ESP_LOGV(TAG, "Timing anomaly at %d: duration=%u", i, timings_[i]);
      continue;
    }
    bool bit = (timings_[i] > threshold);
    raw_bytes[byte_i] <<= 1;
    raw_bytes[byte_i] |= bit;
    bit_i++;
    if (bit_i == 8) {
      bit_i = 0;
      byte_i++;
    }
    if (byte_i >= 16) break;
  }

  ESP_LOGD(TAG, "Decoded raw bytes:");
  for (int i = 0; i < 16; i++) {
    ESP_LOGD(TAG, "[%02d] 0x%02X", i, raw_bytes[i]);
  }

  uint8_t checksum = 0;
  for (int i = 0; i < 15; i++) checksum ^= raw_bytes[i];
  if (checksum != raw_bytes[15]) {
    ESP_LOGW(TAG, "Checksum mismatch: expected 0x%02X, got 0x%02X", raw_bytes[15], checksum);
  }

  int sync_offset = -1;
  for (int i = 0; i < 13; i++) {
    if (raw_bytes[i] == 0x20 && raw_bytes[i + 1] == 0x00 && raw_bytes[i + 2] == 0x10) {
      sync_offset = i - 3;
      ESP_LOGD(TAG, "UVR64 device ID found at offset %d", i);
      break;
    }
  }
  if (sync_offset == -1) {
    sync_offset = 0;
    ESP_LOGW(TAG, "No device ID sync found – falling back to offset 0");
  }

  if (sync_offset + 13 >= 16) {
    ESP_LOGW(TAG, "Not enough data after sync offset %d", sync_offset);
    return;
  }

  for (int i = 0; i < 6; i++) {
    uint8_t low = raw_bytes[sync_offset + 2 * i];
    uint8_t high = raw_bytes[sync_offset + 2 * i + 1];
    int16_t raw = (high << 8) | low;
    if (raw == 0x7FFF || raw == 0x8000 || raw < -1000 || raw > 2000) {
      ESP_LOGW(TAG, "Temp[%d] raw value 0x%04X out of range or invalid", i, raw & 0xFFFF);
      continue;
    }
    float temp = raw / 10.0f;
    ESP_LOGI(TAG, "Temp[%d] = %.1f °C (raw: 0x%04X)", i, temp, raw & 0xFFFF);
    if (temp_sensors_[i] != nullptr)
      temp_sensors_[i]->publish_state(temp);
  }

  uint8_t relays = raw_bytes[sync_offset + 12];
  for (int i = 0; i < 4; i++) {
    bool state = (relays >> i) & 0x01;
    ESP_LOGI(TAG, "Relais[%d] = %s", i, state ? "ON" : "OFF");
    if (relay_sensors_[i] != nullptr)
      relay_sensors_[i]->publish_state(state);
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
