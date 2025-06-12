#include "dlbus_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

void DLBusSensor::setup() {
  pinMode(pin_, INPUT);
  last_edge_ = micros();
  attachInterruptArg(digitalPinToInterrupt(pin_), isr, this, CHANGE);
}

void DLBusSensor::update() {
  if (frame_ready_) {
    parse_frame_();
    bit_index_ = 0;
    last_edge_ = micros();
    attachInterruptArg(digitalPinToInterrupt(pin_), isr, this, CHANGE);
    frame_ready_ = false;
  }
}

void IRAM_ATTR DLBusSensor::isr(void *arg) {
  auto *self = static_cast<DLBusSensor *>(arg);
  unsigned long now = micros();
  uint32_t duration = now - self->last_edge_;
  self->last_edge_ = now;
  if (self->bit_index_ < MAX_BITS) {
    self->timings_[self->bit_index_++] = duration;
  } else {
    self->frame_ready_ = true;
    detachInterrupt(digitalPinToInterrupt(self->pin_));
  }
}

void DLBusSensor::parse_frame_() {
  if (bit_index_ < 80) return;

  uint8_t raw_bytes[16] = {0};
  int byte_i = 0, bit_i = 0;

  for (int i = 0; i < bit_index_ - 1; i += 2) {
    uint32_t t1 = timings_[i];
    uint32_t t2 = timings_[i + 1];
    bool bit = t1 < t2;
    raw_bytes[byte_i] <<= 1;
    raw_bytes[byte_i] |= bit;
    bit_i++;
    if (bit_i == 8) {
      bit_i = 0;
      byte_i++;
    }
    if (byte_i >= 16) break;
  }

  for (int i = 0; i < 6; i++) {
    int16_t raw = (raw_bytes[2 * i] << 8) | raw_bytes[2 * i + 1];
    float temp = raw / 10.0f;
    if (temp_sensors_[i] != nullptr)
      temp_sensors_[i]->publish_state(temp);
  }

  uint8_t relays = raw_bytes[12];
  for (int i = 0; i < 4; i++) {
    if (relay_sensors_[i] != nullptr)
      relay_sensors_[i]->publish_state((relays >> i) & 0x01);
  }
}

}  // namespace uvr64_dlbus
}  // namespace esphome
