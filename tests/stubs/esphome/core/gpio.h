#pragma once
#include <cstdint>

namespace esphome {
namespace gpio {
enum InterruptType : uint8_t {
  INTERRUPT_RISING_EDGE = 1,
  INTERRUPT_FALLING_EDGE = 2,
  INTERRUPT_ANY_EDGE = 3,
};
enum Flags : uint8_t {
  FLAG_NONE = 0x00,
  FLAG_INPUT = 0x01,
  FLAG_OUTPUT = 0x02,
};
}

class ISRInternalGPIOPin {
 public:
  bool digital_read() { return false; }
  void digital_write(bool) {}
  void clear_interrupt() {}
  void pin_mode(gpio::Flags) {}
};

class InternalGPIOPin {
 public:
  virtual void setup() {}
  virtual void pin_mode(gpio::Flags) {}
  virtual bool digital_read() { return false; }
  virtual void digital_write(bool) {}
  virtual ISRInternalGPIOPin to_isr() const { return ISRInternalGPIOPin(); }
  template<typename T>
  void attach_interrupt(void (*func)(T *), T *arg, gpio::InterruptType type) const {
    this->attach_interrupt(reinterpret_cast<void (*)(void *)>(func), arg, type);
  }
  virtual void attach_interrupt(void (*)(void *), void *, gpio::InterruptType) const {}
  virtual void detach_interrupt() const {}
  virtual uint8_t get_pin() const { return 0; }
  virtual bool is_inverted() const { return false; }
};

}  // namespace esphome
