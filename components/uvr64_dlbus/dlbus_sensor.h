#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/gpio.h"
#include <array>
#include <cstddef>

namespace esphome {
namespace uvr64_dlbus {

class DLBusSensor : public Component {
 public:
  DLBusSensor();
  explicit DLBusSensor(uint8_t pin);
  explicit DLBusSensor(InternalGPIOPin *pin);
  void set_pin(uint8_t pin) { this->pin_num_ = pin; }
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  uint8_t get_pin() const;
  void setup() override;
  void loop() override;

  void set_temp_sensor(uint8_t index, sensor::Sensor *sensor) {
    if (index < 6) this->temp_sensors_[index] = sensor;
  }

  void set_relay_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) {
    if (index < 4) this->relay_sensors_[index] = sensor;
  }

 protected:
  static void IRAM_ATTR isr(DLBusSensor *arg);
  void parse_frame_();
  void compute_timing_stats_();

  uint8_t pin_num_{0};
  InternalGPIOPin *pin_{nullptr};
  ISRInternalGPIOPin pin_isr_;
  static constexpr size_t MAX_BITS = 256;
  std::array<uint8_t, MAX_BITS> timings_{};
  size_t bit_index_ = 0;
  sensor::Sensor *temp_sensors_[6]{};
  binary_sensor::BinarySensor *relay_sensors_[4]{};
  volatile bool frame_buffer_ready_ = false;
  volatile uint32_t last_change_{0};
  uint16_t timing_histogram_[64]{};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
