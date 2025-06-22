// MIT License - see LICENSE file in the project root for full details.
#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace uvr64_dlbus {

class DLBusSensor : public Component {
 public:
  explicit DLBusSensor(uint8_t pin) : pin_(pin) {}
  void setup() override;
  void loop() override;

  void set_temp_sensor(int index, sensor::Sensor *sensor);
  void set_relay_sensor(int index, binary_sensor::BinarySensor *sensor);

 protected:
  static const int MAX_BITS = 128;
  volatile uint32_t timings_[MAX_BITS];
  volatile int bit_index_ = 0;
  volatile bool frame_ready_ = false;
  volatile uint32_t min_valid_timing_ = 1000;

  uint8_t pin_;
  unsigned long last_edge_ = 0;

  sensor::Sensor *temp_sensors_[6] = {nullptr};
  binary_sensor::BinarySensor *relay_sensors_[4] = {nullptr};

  static void IRAM_ATTR isr(void *arg);
  void parse_frame_();
  void compute_timing_stats_(uint32_t &median, float &mean, float &stddev, uint32_t &min_t, uint32_t &max_t);
};

}  // namespace uvr64_dlbus
}  // namespace esphome
