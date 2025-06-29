#include "dlbus_sensor.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <vector>

namespace esphome {
namespace uvr64_dlbus {

static const char *const TAG = "uvr64_dlbus";

DLBusSensor::DLBusSensor() {
  this->bit_index_ = 0;
  this->timings_.fill(0);
  this->levels_.fill(0);
}

DLBusSensor::DLBusSensor(uint8_t pin) : pin_num_(pin) {
  this->bit_index_ = 0;
  this->timings_.fill(0);
  this->levels_.fill(0);
}

DLBusSensor::DLBusSensor(InternalGPIOPin *pin) : pin_(pin) {
  this->bit_index_ = 0;
  this->timings_.fill(0);
  this->levels_.fill(0);
}

uint8_t DLBusSensor::get_pin() const {
  if (this->pin_ != nullptr) {
    return this->pin_->get_pin();
  }
  return this->pin_num_;
}

void DLBusSensor::setup() {
  if (this->pin_ != nullptr) {
    this->pin_->setup();
    this->pin_isr_ = this->pin_->to_isr();
    this->pin_->attach_interrupt(&DLBusSensor::isr, this, gpio::INTERRUPT_ANY_EDGE);
    ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", this->pin_->get_pin());
  } else {
    pinMode(pin_num_, INPUT);
    attachInterruptArg(digitalPinToInterrupt(pin_num_),
                      reinterpret_cast<void (*)(void *)>(&DLBusSensor::isr), this, CHANGE);
    ESP_LOGI(TAG, "DLBusSensor setup complete, listening on pin %d", pin_num_);
  }
}

void DLBusSensor::loop() {
  uint32_t now = micros();
  if (!frame_buffer_ready_ && bit_index_ > 0 && (now - last_change_) > FRAME_TIMEOUT_US) {
    frame_buffer_ready_ = true;
  }
  if (frame_buffer_ready_) {
    ESP_LOGD(TAG, "Processing frame with %d bits", bit_index_);
    if (this->pin_ != nullptr) {
      this->pin_->detach_interrupt();
    } else {
      detachInterrupt(digitalPinToInterrupt(pin_num_));
    }
    parse_frame_();
    if (this
