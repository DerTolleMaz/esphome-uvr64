#pragma once
#include "esphome/core/component.h"

namespace esphome {
namespace binary_sensor {
class BinarySensor {
 public:
  bool published_state = false;
  void publish_state(bool state) { published_state = state; }
};
}  // namespace binary_sensor
}  // namespace esphome
