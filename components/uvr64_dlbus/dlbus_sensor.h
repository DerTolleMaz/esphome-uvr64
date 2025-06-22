// Header placeholder
#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace uvr64_dlbus {

class DLBusSensor : public Component {
 public:
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_temp_sensor(int index, sensor::Sensor *sensor);
  void set_relay_sensor(int index, binary_sensor::BinarySensor *sensor);

 protected:
  InternalGPIOPin *pin_;
  sensor::Sensor *temp_sensors_[6] = {nullptr};
  binary_sensor::BinarySensor *relay_sensors_[4] = {nullptr};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
