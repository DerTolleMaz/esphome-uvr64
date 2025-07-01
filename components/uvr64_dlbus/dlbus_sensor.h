#include "dlbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

void DLBusSensor::setup() {
  pinMode(pin_num_, INPUT);
  attachInterruptArg(digitalPinToInterrupt(pin_num_),
                     reinterpret_cast<void (*)(void *)>(&DLBusSensor::gpio_isr_),
                     this, CHANGE);
  ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", pin_num_);
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

    detachInterrupt(digitalPinToInterrupt(pin_num_));

    process_frame_();

    attachInterruptArg(digitalPinToInterrupt(pin_num_),
                       reinterpret_cast<void (*)(void *)>(&DLBusSensor::gpio_isr_),
                       this, CHANGE);

    frame_buffer_ready_ = false;
    bit_index_ = 0;
  }
}

void IRAM_ATTR DLBusSensor::gpio_isr_(DLBusSensor *sensor) {
  uint32_t now = micros();
  bool level = digitalRead(sensor->pin_num_);
  uint32_t duration = now - sensor->last_change_;
  sensor->last_change_ = now;

  if (sensor->bit_index_ < MAX_BITS) {
    sensor->levels_[sensor->bit_index_] = level;
    sensor->timings_[sensor->bit_index_] = (duration > 255) ? 255 : duration;
    sensor->bit_index_++;
  }
}

void DLBusSensor::process_frame_() {
  std::vector<uint8_t> decoded_bytes;
  bool ok = decode_manchester_(decoded_bytes);
  if (!ok) {
    ESP_LOGW(TAG, "Manchester decode failed");
    log_bits_();
    return;
  }

  log_frame_(decoded_bytes);
  parse_frame_(decoded_bytes);
}

bool DLBusSensor::decode_manchester_(std::vector<uint8_t> &result) {
  size_t bitstream_size = bit_index_;
  size_t i = 0;

  // Suche SYNC: mindestens 16 Highs
  size_t sync_index = 0;
  size_t high_count = 0;
  for (i = 0; i < bitstream_size; i++) {
    if (levels_[i] == 1) {
      high_count++;
      if (high_count >= 16) {
        sync_index = i + 1;
        break;
      }
    } else {
      high_count = 0;
    }
  }
  if (high_count < 16) {
    ESP_LOGW(TAG, "No SYNC found!");
    return false;
  }

  ESP_LOGI(TAG, "SYNC found at bit %d", static_cast<int>(sync_index));

  // Manchester dekodieren (Startbit 0, danach Paare)
  size_t pos = sync_index;
  bool current_bit = false;
  size_t bit_count = 0;
  uint8_t byte = 0;

  while (pos + 1 < bitstream_size) {
    bool first = levels_[pos];
    bool second = levels_[pos + 1];

    if (first == second) {
      ESP_LOGW(TAG, "Manchester error at bit %d-%d", static_cast<int>(pos), static_cast<int>(pos + 1));
      return false;
    }

    current_bit = first == 0;
    byte = (byte << 1) | (current_bit ? 1 : 0);
    bit_count++;

    if (bit_count == 8) {
      result.push_back(byte);
      byte = 0;
      bit_count = 0;
    }

    pos += 2;
  }

  if (bit_count != 0) {
    byte <<= (8 - bit_count);
    result.push_back(byte);
  }

  return true;
}

void DLBusSensor::parse_frame_(const std::vector<uint8_t> &frame) {
  if (frame.size() < 16) {
    ESP_LOGW(TAG, "Frame too short (%d bytes)", static_cast<int>(frame.size()));
    return;
  }

  // Temperaturen
  for (int i = 0; i < 6; i++) {
    uint8_t high = frame[i * 2];
    uint8_t low = frame[i * 2 + 1];
    int16_t raw = (static_cast<int16_t>(high) << 8) | low;
    float value = raw / 10.0f;

    if (this->temp_sensors_[i]) {
      this->temp_sensors_[i]->publish_state(value);
    }

    ESP_LOGI(TAG, "Temperature Sensor %d: %.1f°C (Raw: %d)", i, value, raw);
  }

  // Relais
  for (int i = 0; i < 4; i++) {
    uint8_t val = frame[12 + i];
    bool state = val != 0;

    if (this->relay_sensors_[i]) {
      this->relay_sensors_[i]->publish_state(state);
    }

    ESP_LOGI(TAG, "Relay %d: %s (Raw: %d)", i, state ? "ON" : "OFF", val);
  }
}

void DLBusSensor::log_bits_() {
  std::string bits;
  for (size_t i = 0; i < bit_index_; i++) {
    char buf[10];
    snprintf(buf, sizeof(buf), "%d", levels_[i]);
    bits += buf;
  }
  ESP_LOGI(TAG, "Bits: %s", bits.c_str());
}

void DLBusSensor::log_frame_(const std::vector<uint8_t> &frame) {
  std::string str;
  for (auto b : frame) {
    char buf[5];
    snprintf(buf, sizeof(buf), "%02X ", b);
    str += buf;
  }
  ESP_LOGI(TAG, "Frame Bytes:\n%s", str.c_str());
}

}  // namespace uvr64_dlbus
}  // namespace esphome
