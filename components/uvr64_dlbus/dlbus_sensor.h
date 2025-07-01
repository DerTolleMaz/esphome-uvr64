#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/log.h"

#include <vector>

namespace esphome {
namespace uvr64_dlbus {

static const uint32_t FRAME_TIMEOUT_US = 100000;  // 100ms Frame Timeout
static const size_t MAX_BITS = 1024;

class DLBusSensor : public Component {
 public:
  DLBusSensor() = default;
  explicit DLBusSensor(uint8_t pin) : pin_num_(pin) {}

  void set_pin(uint8_t pin) { this->pin_num_ = pin; }
  uint8_t get_pin() const { return this->pin_num_; }

  void set_temp_sensor(uint8_t index, sensor::Sensor *sensor) { temp_sensors_[index] = sensor; }
  void set_relay_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) { relay_sensors_[index] = sensor; }

  void setup() override;
  void loop() override;

 protected:
  static void IRAM_ATTR gpio_isr_(DLBusSensor *sensor);

  void process_frame_();
  bool decode_manchester_(std::vector<uint8_t> &result);
  void parse_frame_(const std::vector<uint8_t> &frame);

  void log_bits_();
  void log_frame_(const std::vector<uint8_t> &frame);

  uint8_t pin_num_;
  volatile uint32_t last_change_{0};
  volatile size_t bit_index_{0};
  volatile bool frame_buffer_ready_{false};

  std::array<uint8_t, MAX_BITS> timings_{};
  std::array<uint8_t, MAX_BITS> levels_{};

  sensor::Sensor *temp_sensors_[6]{};
  binary_sensor::BinarySensor *relay_sensors_[4]{};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
