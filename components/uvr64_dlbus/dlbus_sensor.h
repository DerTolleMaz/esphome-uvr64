#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace uvr64_dlbus {

class DLBusSensor : public PollingComponent {
 public:
  explicit DLBusSensor(uint32_t update_interval = 10000)
      : PollingComponent(update_interval) {}

  void setup() override;
  void loop() override;
  void update() override;

void set_temp_sensor(int index, sensor::Sensor *sensor);
void set_relay_sensor(int index, binary_sensor::BinarySensor *sensor);

  void set_timings(const std::vector<uint32_t> &timings) {
    for (size_t i = 0; i < timings.size() && i < 128; i++) {
      timings_[i] = timings[i];
    }
    frame_ready_ = true;
  }

 protected:
  void parse_frame_();

  sensor::Sensor *temp_sensors_[6]{};
  binary_sensor::BinarySensor *relay_sensors_[4]{};

  bool frame_ready_ = false;
  uint32_t timings_[128]{};
  bool bits_[256]{};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
