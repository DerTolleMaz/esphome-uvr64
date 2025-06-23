#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace uvr64_dlbus {

class DLBusSensor : public Component {
 public:
  explicit DLBusSensor(uint8_t pin);

  void setup() override;
  void loop() override;

  void set_temp_sensor(uint8_t index, sensor::Sensor *sensor);
  void set_relay_sensor(uint8_t index, binary_sensor::BinarySensor *sensor);

 protected:
  void parse_frame_();
  void compute_timing_stats_();

  static void IRAM_ATTR isr(void *arg);

  uint8_t pin_;
  volatile uint32_t last_interrupt_time_{0};
  std::vector<uint32_t> bit_durations_;
  std::vector<uint8_t> frame_buffer_;
  sensor::Sensor *temp_sensors_[6]{};
  binary_sensor::BinarySensor *relay_sensors_[4]{};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
