#include "dlbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

// Schwellenwerte
static const uint32_t FRAME_TIMEOUT_US = 100000;  // 100ms
static const uint32_t MIN_FLANK_TIME_US = 2000;   // 2ms minimale gültige Flanke
static const size_t MAX_BITS = 1024;               // Sicherheits-Puffer

void IRAM_ATTR DLBusSensor::gpio_isr_(DLBusSensor *sensor) {
  const uint32_t now = micros();
  const uint32_t duration = now - sensor->last_change_;
  const bool level = sensor->pin_isr_.digital_read();
  sensor->last_change_ = now;

  // Flanken kürzer als 2ms ignorieren
  if (duration < MIN_FLANK_TIME_US) return;

  if (sensor->bit_index_ < MAX_BITS) {
    sensor->timings_[sensor->bit_index_] = duration;
    sensor->levels_[sensor->bit_index_] = level;
    sensor->bit_index_++;
  }
}

void DLBusSensor::setup() {
  ESP_LOGI(TAG, "DLBusSensor setup on pin %d", pin_num_);
  pinMode(pin_num_, INPUT);
  attachInterruptArg(
    digitalPinToInterrupt(pin_num_),
    reinterpret_cast<void (*)(void *)>(&DLBusSensor::gpio_isr_),
    this,
    CHANGE
  );
}

void DLBusSensor::loop() {
  const uint32_t now = micros();

  // Prüfen ob Frame Timeout
  if (bit_index_ > 0 && (now - last_change_) > FRAME_TIMEOUT_US) {
    detachInterrupt(digitalPinToInterrupt(pin_num_));
    process_frame_();
    bit_index_ = 0;
    attachInterruptArg(
      digitalPinToInterrupt(pin_num_),
      reinterpret_cast<void (*)(void *)>(&DLBusSensor::gpio_isr_),
      this,
      CHANGE
    );
  }
}

void DLBusSensor::process_frame_() {
  ESP_LOGI(TAG, "Processing frame with %d bits", bit_index_);

  if (bit_index_ < 100) {
    ESP_LOGW(TAG, "Frame too short, ignored");
    return;
  }

  // Manchester-Dekodierung
  std::vector<bool> bits;
  for (size_t i = 0; i + 1 < bit_index_; i += 2) {
    bool b = (timings_[i] < timings_[i + 1]);
    bits.push_back(b);
  }

  ESP_LOGI(TAG, "Bitstream length %d", bits.size());

  // Suche SYNC (mindestens 16 HIGH)
  size_t sync_index = 0;
  size_t high_count = 0;
  for (size_t i = 0; i < bits.size(); i++) {
    if (bits[i]) {
      high_count++;
      if (high_count >= 16) {
        sync_index = i + 1;
        ESP_LOGI(TAG, "SYNC found at bit %d", sync_index);
        break;
      }
    } else {
      high_count = 0;
    }
  }
  if (sync_index == 0) {
    ESP_LOGW(TAG, "No SYNC found!");
    return;
  }

  // Byteweise Dekodierung
  std::vector<uint8_t> bytes;
  for (size_t i = sync_index; i + 10 <= bits.size(); i += 10) {
    if (!bits[i]) {
      // Startbit = 0 korrekt
      uint8_t data = 0;
      for (int b = 0; b < 8; b++) {
        data |= (bits[i + 1 + b] << b);  // LSB first
      }
      if (bits[i + 9] != 1) {
        ESP_LOGW(TAG, "Stopbit error at byte %d", bytes.size());
        continue;
      }
      bytes.push_back(data);
    } else {
      ESP_LOGW(TAG, "Startbit error at byte %d", bytes.size());
    }
  }

  ESP_LOGI(TAG, "Decoded %d bytes", bytes.size());
  if (bytes.size() == 0) return;

  log_frame_(bytes);

  // Frame auswerten, Beispiel: Temperatur 1
  if (bytes.size() >= 14) {
    int16_t temp_raw = (bytes[3] << 8) | bytes[2]; // Temp1 high + low
    float temp_c = temp_raw / 10.0f;
    if (temp_sensors_[0] != nullptr)
      temp_sensors_[0]->publish_state(temp_c);
    ESP_LOGI(TAG, "Temp1: %.1f°C", temp_c);
    // → Analog alle weiteren Temperaturen und Relais
  }
}

void DLBusSensor::log_frame_(const std::vector<uint8_t> &frame) {
  std::string out;
  char buf[8];
  for (auto b : frame) {
    snprintf(buf, sizeof(buf), "%02X ", b);
    out += buf;
  }
  ESP_LOGI(TAG, "Frame Bytes: %s", out.c_str());
}

}  // namespace uvr64_dlbus
}  // namespace esphome
