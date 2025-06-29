#include "arduino_stubs.h"
#include "dlbus_sensor.h"
#include <iostream>

using namespace esphome::uvr64_dlbus;

class TestDLBusSensor : public DLBusSensor {
 public:
  using DLBusSensor::DLBusSensor;
  using DLBusSensor::parse_frame_;
  using DLBusSensor::timings_;
  using DLBusSensor::bit_index_;
};


static void encode_byte(TestDLBusSensor &sensor, uint8_t value) {
  for (int i = 7; i >= 0; --i) {
    bool bit = (value >> i) & 1;
    sensor.timings_[sensor.bit_index_++] = bit ? 1 : 2;
    sensor.timings_[sensor.bit_index_++] = bit ? 2 : 1;
  }
}

int main() {
  TestDLBusSensor sensor(static_cast<uint8_t>(0));

  esphome::sensor::Sensor temps[6];
  esphome::binary_sensor::BinarySensor relays[4];

  for (int i = 0; i < 6; i++) sensor.set_temp_sensor(i, &temps[i]);
  for (int i = 0; i < 4; i++) sensor.set_relay_sensor(i, &relays[i]);

  uint8_t frame[16] = {
      0x00, 0xC8,  // 20.0°C
      0x00, 0xD7,  // 21.5°C
      0xFF, 0xF6,  // -1.0°C
      0x01, 0x02,  // 25.8°C
      0x00, 0xFA,  // 25.0°C
      0xFF, 0xE6,  // -2.6°C
      0x01,        // Relay 0 on
      0x00,        // Relay 1 off
      0x01,        // Relay 2 on
      0x00         // Relay 3 off
  };
  sensor.bit_index_ = 0;
  for (uint8_t b : frame) {
    encode_byte(sensor, b);
  }

  sensor.parse_frame_();

  float expected_temps[6] = {20.0f, 21.5f, -1.0f, 25.8f, 25.0f, -2.6f};
  bool ok = true;
  for (int i = 0; i < 6; i++) {
    if (temps[i].published_state != expected_temps[i]) {
      std::cerr << "Temp " << i << " mismatch: " << temps[i].published_state
                << " != " << expected_temps[i] << std::endl;
      ok = false;
    }
  }
  bool expected_relays[4] = {true, false, true, false};
  for (int i = 0; i < 4; i++) {
    if (relays[i].published_state != expected_relays[i]) {
      std::cerr << "Relay " << i << " mismatch: " << relays[i].published_state
                << " != " << expected_relays[i] << std::endl;
      ok = false;
    }
  }

  if (ok) {
    std::cout << "parse_frame_() test passed" << std::endl;
    return 0;
  } else {
    std::cerr << "parse_frame_() test failed" << std::endl;
    return 1;
  }
}
