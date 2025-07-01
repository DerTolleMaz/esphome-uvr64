#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/gpio.h"
#include <vector>

namespace esphome {
namespace uvr64_dlbus {

static const uint32_t FRAME_TIMEOUT_US = 100000;  // 100ms
static const int MAX_BITS = 1024;

class DLBusSensor : public Component {
 public:
  explicit DLBusSensor(InternalGPIOPin *pin) : pin_(pin) {}

  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  InternalGPIOPin *get_pin() const { return pin_; }
  uint8_t get_pin_num() const { return pin_ ? pin_->get_pin() : 0; }

  void set_debug(bool enable) { debug_ = enable; }
  bool is_debug() const { return debug_; }

  void set_temp_sensor(uint8_t index, sensor::Sensor *sensor) { temp_sensors_[index] = sensor; }
  void set_relay_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) { relay_sensors_[index] = sensor; }

  void setup() override;
  void loop() override;

 protected:
  void process_frame_();
  void parse_frame_(const std::vector<uint8_t> &frame);
  void parse_frame_();
  void log_frame_(const std::vector<uint8_t> &frame);
  bool decode_manchester_(std::vector<uint8_t> &result);
  void log_bits_();
  bool validate_frame_(const std::vector<uint8_t> &frame);

  static void IRAM_ATTR gpio_isr_(DLBusSensor *arg);

  InternalGPIOPin *pin_;
  uint32_t last_change_{0};
  bool frame_buffer_ready_{false};

  uint8_t levels_[MAX_BITS];
  uint16_t timings_[MAX_BITS];
  uint16_t bit_index_{0};

  bool debug_{false};

  sensor::Sensor *temp_sensors_[6]{};
  binary_sensor::BinarySensor *relay_sensors_[4]{};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
