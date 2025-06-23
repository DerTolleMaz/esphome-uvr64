#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace uvr64_dlbus {

class DLBusSensor : public Component {
 public:
  DLBusSensor();

  void set_pin(uint8_t pin);
  void set_temp_sensor(uint8_t index, sensor::Sensor *sensor);
  void set_relay_sensor(uint8_t index, binary_sensor::BinarySensor *sensor);

  void setup() override;
  void loop() override;

 protected:
  static void IRAM_ATTR isr(void *arg);
  void compute_timing_stats_();
  void parse_frame_();

  uint8_t pin_;
  volatile uint32_t last_change_us_{0};
  std::vector<uint32_t> bit_durations_;
  sensor::Sensor *temp_sensors_[6] = {nullptr};
  binary_sensor::BinarySensor *relay_sensors_[4] = {nullptr};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
