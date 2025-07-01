#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace uvr64_dlbus {

class DLBusSensor : public Component {
 public:
  // Konstruktoren
  DLBusSensor() = default;
  DLBusSensor(uint8_t pin) : pin_num_(pin) {}
  DLBusSensor(esphome::gpio::InternalGPIOPin *pin) : pin_(pin) {}

  // Setup & Loop
  void setup() override;
  void loop() override;

  // Pin-Getter/Setter
  uint8_t get_pin() const {
    if (this->pin_ != nullptr)
      return this->pin_->get_pin();
    return this->pin_num_;
  }

  void set_pin(uint8_t pin) { this->pin_num_ = pin; }
  void set_pin(esphome::gpio::InternalGPIOPin *pin) { this->pin_ = pin; }

  // Sensor-Zuweisung
  void set_temp_sensor(int idx, sensor::Sensor *s) { temp_sensors_[idx] = s; }
  void set_relay_sensor(int idx, binary_sensor::BinarySensor *s) { relay_sensors_[idx] = s; }

 protected:
  // Interrupt Handler
  static void IRAM_ATTR gpio_isr_(DLBusSensor *sensor);

  // Verarbeitungsroutinen
  void process_frame_();
  void log_frame_(const std::vector<uint8_t> &frame);

  // Pin
  uint8_t pin_num_{0};
  esphome::gpio::InternalGPIOPin *pin_{nullptr};
  esphome::gpio::ISRInternalGPIOPin pin_isr_;

  // Datenpuffer
  uint32_t last_change_{0};
  size_t bit_index_{0};
  uint8_t levels_[1024]{};
  uint16_t timings_[1024]{};

  // Sensoren
  sensor::Sensor *temp_sensors_[6]{};
  binary_sensor::BinarySensor *relay_sensors_[4]{};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
