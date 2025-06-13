#pragma once
#include "esphome/core/component.h"

namespace esphome {
namespace sensor {
class Sensor {
 public:
  float published_state = 0;
  void publish_state(float state) { published_state = state; }
};
}  // namespace sensor
}  // namespace esphome
