
#include "uvr64_dlbus.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

void UVR64DLBusSensor::loop() {
  while (this->available()) {
    uint8_t byte = this->read();
    this->buffer_.push_back(byte);

    if (this->buffer_.size() >= 32) {
      if (is_valid(this->buffer_)) {
        parse_dl_bus(this->buffer_);
        this->buffer_.clear();
      } else {
        this->buffer_.erase(this->buffer_.begin());
      }
    }
  }
}

bool UVR64DLBusSensor::is_valid(const std::vector<uint8_t> &data) {
  return data.size() >= 32;
}

void UVR64DLBusSensor::parse_dl_bus(const std::vector<uint8_t> &data) {
  for (size_t i = 0; i < temp_sensors.size() && i < 6; i++) {
    uint16_t raw = (data[5 + i * 2] << 8) | data[6 + i * 2];
    float value = raw * 0.1f;
    temp_sensors[i]->publish_state(value);
  }

  if (data.size() > 20) {
    uint8_t status = data[20];
    for (size_t i = 0; i < relay_sensors.size() && i < 8; i++) {
      relay_sensors[i]->publish_state((status >> i) & 0x01);
    }
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
