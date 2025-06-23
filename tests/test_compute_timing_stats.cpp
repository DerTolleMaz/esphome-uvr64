#include "arduino_stubs.h"
#include "dlbus_sensor.h"
#include <iostream>
#include <cmath>

using namespace esphome::uvr64_dlbus;

class TestDLBusSensor : public DLBusSensor {
 public:
  using DLBusSensor::DLBusSensor;
  using DLBusSensor::compute_timing_stats_;
  using DLBusSensor::timings_;
  using DLBusSensor::bit_index_;
};

int main() {
  TestDLBusSensor sensor(0);

  sensor.bit_index_ = 8;
  uint32_t values[8] = {1000, 2000, 3000, 4000, 1000, 2000, 3000, 4000};
  for (int i = 0; i < 8; i++) {
    sensor.timings_[i] = values[i];
  }

  uint32_t median, min_t, max_t;
  float mean, stddev;
  sensor.compute_timing_stats_(median, mean, stddev, min_t, max_t);

  bool ok = true;
  if (median != 3000) {
    std::cerr << "Median mismatch: " << median << std::endl;
    ok = false;
  }
  if (std::abs(mean - 2500.0f) > 0.1f) {
    std::cerr << "Mean mismatch: " << mean << std::endl;
    ok = false;
  }
  if (std::abs(stddev - 1118.03f) > 0.1f) {
    std::cerr << "Stddev mismatch: " << stddev << std::endl;
    ok = false;
  }
  if (min_t != 1000) {
    std::cerr << "Min mismatch: " << min_t << std::endl;
    ok = false;
  }
  if (max_t != 4000) {
    std::cerr << "Max mismatch: " << max_t << std::endl;
    ok = false;
  }

  if (ok) {
    std::cout << "compute_timing_stats_() test passed" << std::endl;
    return 0;
  } else {
    std::cerr << "compute_timing_stats_() test failed" << std::endl;
    return 1;
  }
}
