#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace uvr64_dlbus {

class DLBusSensor : public PollingComponent {
 public:
  DLBusSensor() : PollingComponent(10000) {}  // alle 10 Sekunden

  void setup() override;
  void update() override;
  void loop() override;

  void set_temp_sensor(int index, sensor::Sensor *sensor);
  void set_relay_sensor(int index, binary_sensor::BinarySensor *sensor);

  void set_timings(const std::array<uint32_t, 128> &timings) {
    this->timings_ = timings;
    this->frame_ready_ = true;
  }

 protected:
  void parse_frame_();

  std::array<uint32_t, 128> timings_;
  std::array<bool, 256> bits_;
  sensor::Sensor *temp_sensors_[6]{};
  binary_sensor::BinarySensor *relay_sensors_[4]{};
  bool frame_ready_{false};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
