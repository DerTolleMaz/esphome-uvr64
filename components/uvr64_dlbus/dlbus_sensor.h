
class DLBusSensor : public Component {
 public:
  explicit DLBusSensor(uint8_t pin);

  void setup() override;
  void loop() override;

  void set_temp_sensor(uint8_t index, sensor::Sensor *sensor) {
    if (index < 6) this->temp_sensors_[index] = sensor;
  }

  void set_relay_sensor(uint8_t index, binary_sensor::BinarySensor *sensor) {
    if (index < 4) this->relay_sensors_[index] = sensor;
  }

 protected:
  static void IRAM_ATTR isr(void *arg);

  void parse_frame_();
  void compute_timing_stats_();

  uint8_t pin_;

  sensor::Sensor *temp_sensors_[6]{};
  binary_sensor::BinarySensor *relay_sensors_[4]{};

  // Hier kannst du weitere interne Variablen einbauen (Buffer etc.)
};

}  // namespace uvr64_dlbus
}  // namespace esphome
