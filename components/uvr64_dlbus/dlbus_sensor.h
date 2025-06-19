// DL-Bus Dekodierungskomponente für ESPHome (mit Manchester-Decoding und Telegrammauswertung für UVR64)
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

  void set_temp_sensor(int index, sensor::Sensor *sensor) {
    if (index >= 0 && index < 6) this->temp_sensors_[index] = sensor;
  }

  void set_relay_sensor(int index, binary_sensor::BinarySensor *sensor) {
    if (index >= 0 && index < 4) this->relay_sensors_[index] = sensor;
  }

  void set_timings(const std::array<uint32_t, 128> &timings) {
    this->timings_ = timings;
    this->frame_ready_ = true;
  }

 protected:
  void parse_frame_();

  std::array<uint32_t, 128> timings_;
  std::array<bool, 256> bits_;
  bool frame_ready_{false};

  sensor::Sensor *temp_sensors_[6] = {nullptr};
  binary_sensor::BinarySensor *relay_sensors_[4] = {nullptr};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
