#pragma once
#include <cstdint>

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

namespace esphome {
class Component {
 public:
  virtual void setup() {}
  virtual void loop() {}
  virtual float get_setup_priority() const { return 0; }
  virtual float get_loop_priority() const { return 0; }
};

class PollingComponent : public Component {
 public:
  explicit PollingComponent(uint32_t update_interval = 0) {}
  virtual void set_update_interval(uint32_t) {}
  virtual void update() {}
  virtual uint32_t get_update_interval() const { return 0; }
};
}  // namespace esphome
