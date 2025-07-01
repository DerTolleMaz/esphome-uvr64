#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace uvr64_dlbus {

class DLBusSensor : public Component {
 public:
  DLBusSensor(uint8_t pin) : pin_num_(pin) {}

  void set_pin(uint8_t pin) { this->pin_num_ = pin; }
  uint8_t get_pin() const { return pin_num_; }

  void set_temp_sensor(uint8_t index, sensor::Sensor *sensor) { temp_sensors_[index] = sensor; }
  void set_relay_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) { relay_sensors_[index] = sensor; }

  void setup() override;
  void loop() override;

 protected:
  void process_frame_();
  void parse_frame_(const std::vector<uint8_t> &frame);
  void log_frame_(const std::vector<uint8_t> &frame);
  bool decode_manchester_(std::vector<uint8_t> &result);
  void log_bits_();

  static void IRAM_ATTR gpio_isr_(DLBusSensor *arg);

  uint8_t pin_num_;
  uint32_t last_change_{0};
  bool frame_buffer_ready_{false};

  static const int MAX_BITS = 1024;
  uint8_t levels_[MAX_BITS];
  uint8_t timings_[MAX_BITS];
  uint16_t bit_index_{0};

  sensor::Sensor *temp_sensors_[6]{};
  binary_sensor::BinarySensor *relay_sensors_[4]{};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
