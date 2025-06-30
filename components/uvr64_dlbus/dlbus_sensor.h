#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/gpio.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <array>
#include <vector>

namespace esphome {
namespace uvr64_dlbus {

static const uint32_t FRAME_TIMEOUT_US = 40000;  // 40 ms Frame-Timeout
static const size_t MAX_BITS = 1024;              // Buffer size

class DLBusSensor : public Component {
 public:
  DLBusSensor();
  explicit DLBusSensor(uint8_t pin);
  explicit DLBusSensor(InternalGPIOPin *pin);

  void setup() override;
  void loop() override;

  uint8_t get_pin() const;

  void set_temp_sensor(uint8_t index, sensor::Sensor *sensor) { temp_sensors_[index] = sensor; }
  void set_relay_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) { relay_sensors_[index] = sensor; }

 protected:
  static void IRAM_ATTR isr(DLBusSensor *arg);

  void parse_frame_();
  bool decode_manchester_(std::vector<bool> &bits, std::vector<uint8_t> &bytes);
  bool find_sync_(const std::vector<bool> &bits, size_t &sync_pos);
  void log_bits_();
  void log_frame_(const std::vector<uint8_t> &frame);

  // Pin handling
  InternalGPIOPin *pin_{nullptr};
  ISRInternalGPIOPin pin_isr_;
  uint8_t pin_num_{255};

  // Signal buffers
  std::array<uint8_t, MAX_BITS> levels_;
  std::array<uint8_t, MAX_BITS> timings_;
  size_t bit_index_{0};
  uint32_t last_change_{0};
  bool frame_buffer_ready_{false};

  // Sensor assignment
  std::array<sensor::Sensor *, 6> temp_sensors_{};
  std::array<binary_sensor::BinarySensor *, 4> relay_sensors_{};
};

}  // namespace uvr64_dlbus
}  // namespace esphome
