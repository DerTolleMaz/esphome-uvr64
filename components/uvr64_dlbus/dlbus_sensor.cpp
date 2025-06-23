#include "dlbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

// Konstruktor
DLBusSensor::DLBusSensor() = default;

// Pin setzen
void DLBusSensor::set_pin(uint8_t pin) {
  this->pin_ = pin;
}

// Temperatursensoren setzen
void DLBusSensor::set_temp_sensor(uint8_t index, sensor::Sensor *sensor) {
  if (index < 6) {
    this->temp_sensors_[index] = sensor;
  }
}

// Relaiszustände setzen
void DLBusSensor::set_relay_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) {
  if (index < 4) {
    this->relay_sensors_[index] = sensor;
  }
}

// Setup
void DLBusSensor::setup() {
  pinMode(this->pin_, INPUT);
  attachInterruptArg(digitalPinToInterrupt(this->pin_), isr, this, CHANGE);
  ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", this->pin_);
}

// Loop
void DLBusSensor::loop() {
  // Mindestanzahl Bitlängen für ein gültiges Telegramm (Beispiel)
  if (this->bit_durations_.size() > 80) {
    ESP_LOGD(TAG, "DLBus frame received, decoding...");
    this->compute_timing_stats_();
    this->parse_frame_();
    this->bit_durations_.clear();
  }
}

// ISR: sammelt Bitlängen
void IRAM_ATTR DLBusSensor::isr(void *arg) {
  auto *self = static_cast<DLBusSensor *>(arg);
  uint32_t now = micros();
  uint32_t duration = now - self->last_change_us_;
  self->last_change_us_ = now;
  self->bit_durations_.push_back(duration);
}

// Statistik über Bitlängen berechnen
void DLBusSensor::compute_timing_stats_() {
  if (this->bit_durations_.empty()) return;

  std::vector<uint32_t> data = this->bit_durations_;

  uint32_t min = UINT32_MAX, max = 0, sum = 0;
  for (auto d : data) {
    if (d < min) min = d;
    if (d > max) max = d;
    sum += d;
  }

  float mean = sum / static_cast<float>(data.size());

  float variance = 0;
  for (auto d : data)
    variance += (d - mean) * (d - mean);
  float stddev = sqrtf(variance / data.size());

  std::sort(data.begin(), data.end());
  uint32_t median = data[data.size() / 2];

  // Histogramm
  const uint32_t BIN_WIDTH = 2000;  // µs
  const uint32_t BIN_COUNT = 10;
  uint32_t bins[BIN_COUNT] = {0};
  uint32_t above = 0;

  for (auto d : this->bit_durations_) {
    uint32_t bin = d / BIN_WIDTH;
    if (bin < BIN_COUNT)
      bins[bin]++;
    else
      above++;
  }

  ESP_LOGI(TAG, "Bit Timing Histogram (Bin width: %u µs):", BIN_WIDTH);
  for (uint32_t i = 0; i < BIN_COUNT; i++) {
    ESP_LOGI(TAG, "  %2u - %2u ms: %3u", i * 2, i * 2 + 1, bins[i]);
  }
  ESP_LOGI(TAG, "  >%u ms: %u", BIN_COUNT * 2, above);
  ESP_LOGI(TAG, "Bit timing – Min: %u µs, Max: %u µs, Median: %u µs, Mean: %.1f µs, StdDev: %.1f",
           min, max, median, mean, stddev);
}

// Beispielhafte Auswertung
void DLBusSensor::parse_frame_() {
  ESP_LOGD(TAG, "Parsed %u timing values (not yet decoded to bits)", this->bit_durations_.size());

  // Hier könntest du die Manchester-Dekodierung und Bytes extrahieren

  // Debug-Ausgabe
  for (size_t i = 0; i < this->bit_durations_.size(); i++) {
    if (this->bit_durations_[i] > 40000) {
      ESP_LOGV(TAG, "Timing anomaly at %d: duration=%u", static_cast<int>(i), this->bit_durations_[i]);
    }
  }

  // Dummy-Rohdaten zum Testen
  uint8_t raw[] = {0x5C, 0x8E, 0xF2, 0xD5, 0x14, 0x6D, 0xA7, 0xEC, 0x14, 0x20, 0x5A, 0xE2, 0x9C, 0x61, 0x2D, 0x00};

  ESP_LOGD(TAG, "Decoded raw bytes:");
  for (size_t i = 0; i < sizeof(raw); i++) {
    ESP_LOGD(TAG, "[%02u] 0x%02X", static_cast<unsigned>(i), raw[i]);
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
