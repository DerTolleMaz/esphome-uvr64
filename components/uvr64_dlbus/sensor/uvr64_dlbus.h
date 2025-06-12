#pragma once
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>

namespace esphome {
namespace uvr64_dlbus {

class UVR64DLBusSensor : public Component, public uart::UARTDevice {
 public:
  UVR64DLBusSensor() : UARTDevice() {}

  void setup() override;
  void loop() override;

  void set_parent(uart::UARTComponent *parent) {
    this->set_uart_parent(parent);
  }

  void set_temps(const std::vector<sensor::Sensor *> &sensors) { temp_sensors = sensors; }
  void set_relays(const std::vector<sensor::Sensor *> &sensors) { relay_sensors = sensors; }
  void set_decode_xor(bool value) { decode_xor_ = value; }

 protected:
  std::vector<sensor::Sensor *> temp_sensors;
  std::vector<sensor::Sensor *> relay_sensors;

  std::vector<uint8_t> buffer_;

  bool decode_xor_{false};

  static const uint8_t START_BYTE_1 = 0xAA;
  static const uint8_t START_BYTE_2 = 0x55;
  static const size_t FRAME_SIZE = 32;

  void parse_dl_bus(const std::vector<uint8_t> &data);
  bool is_valid(const std::vector<uint8_t> &data);
};

}  // namespace uvr64_dlbus
}  // namespace esphome
