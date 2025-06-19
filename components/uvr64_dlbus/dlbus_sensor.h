#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace uvr64_dlbus {

class DLBusSensor : public PollingComponent {

 public:
  DLBusSensor() : PollingComponent(10000) {}  // Update alle 10 Sekunden

  void setup() override;
  void loop() override;
  void update() override;

  void set_temp_sensor(int index, sensor::Sensor *sensor);
  void set_relay_sensor(int index, binary_sensor::BinarySensor *sensor);

  void set_timings(const std::vector<uint32_t> &timings) {
    size_t count = std::min(timings.size(), sizeof(this->timings_) / sizeof(this->timings_[0]));
    std::copy(timings.begin(), timings.begin() + count, this->timings_);
    this->frame_ready_ = true;
  }
 explicit DLBusSensor(uint32_t update_interval = 10000) : PollingComponent(update_interval) {}
 protected:
  void parse_frame_();

  uint32_t timings_[128] = {};
  bool frame_ready_ = false;

  bool bits_[1024] = {};  // max. 512 Bit für 128 Flankenpaare

  sensor::Sensor *temp_sensors_[6] = {};
  binary_sensor::BinarySensor *relay_sensors_[4] = {};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
