#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace uvr64_dlbus {

class DLBusSensor : public Component {
 public:
  void set_pin(uint8_t pin) { pin_ = pin; }

  void set_temp_sensor(int index, sensor::Sensor *sensor);
  void set_relay_sensor(int index, binary_sensor::BinarySensor *sensor);

  void setup() override;
  void loop() override;

 protected:
  void compute_timing_stats_();
  void parse_frame_();
  static void IRAM_ATTR isr(void *arg);

  uint8_t pin_;
  static const int MAX_BITS = 128;
  volatile uint32_t timings_[MAX_BITS];
  volatile int bit_index_ = 0;
  volatile bool frame_ready_ = false;
  unsigned long last_edge_ = 0;

  sensor::Sensor *temp_sensors_[6] = {nullptr};
  binary_sensor::BinarySensor *relay_sensors_[4] = {nullptr};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
