#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <array>
#include <cstddef>

namespace esphome {
namespace uvr64_dlbus {

static const uint32_t FRAME_TIMEOUT_US = 15000;
static const size_t MAX_BITS = 512;

class DLBusSensor : public Component {
 public:
  DLBusSensor();
  DLBusSensor(uint8_t pin);
  DLBusSensor(InternalGPIOPin *pin);

  void setup() override;
  void loop() override;
  void debug_decode_frame_();

  uint8_t get_pin() const;

  void set_temp_sensor(uint8_t index, sensor::Sensor *sensor) {
    if (index < temp_sensors_.size())
      temp_sensors_[index] = sensor;
  }

  void set_relay_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) {
    if (index < relay_sensors_.size())
      relay_sensors_[index] = sensor;
  }

void set_pin(InternalGPIOPin *pin) {
  this->pin_ = pin;
}

void set_pin(uint8_t pin) {
  this->pin_num_ = pin;
}
 protected:
  static void IRAM_ATTR isr(DLBusSensor *arg);
  void log_bits_();
  void log_frame_(const std::vector<uint8_t> &frame);
  void parse_frame_();

  std::array<uint8_t, MAX_BITS> timings_;
  std::array<uint8_t, MAX_BITS> levels_;
  volatile size_t bit_index_{0};
  volatile bool frame_buffer_ready_{false};
  volatile uint32_t last_change_{0};

  InternalGPIOPin *pin_{nullptr};
  uint8_t pin_num_{255};
  ISRInternalGPIOPin pin_isr_;

  std::array<sensor::Sensor *, 6> temp_sensors_{};
  std::array<binary_sensor::BinarySensor *, 4> relay_sensors_{};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
