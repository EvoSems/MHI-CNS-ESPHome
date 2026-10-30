#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esp8266_transport.h"
#include "protocol.h"

#include <array>
#include <vector>

namespace esphome::mhi_cns {

class StateListener {
 public:
  virtual void on_mhi_state(const AcState &state) = 0;
};

class MhiCnsComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_frame_size(uint8_t value) { this->frame_size_ = value; }
  void set_pins(uint8_t sck, uint8_t mosi, uint8_t miso) { this->transport_.configure(sck, mosi, miso); }
  void set_command_retries(uint8_t value) { this->command_retries_ = value; }
  void add_listener(StateListener *listener) { this->listeners_.push_back(listener); }

  const AcState &state() const { return this->state_; }
  void set_power(bool power);
  void set_mode(HvacMode mode);
  void set_fan(uint8_t fan);
  void set_target_temperature(float temperature);
  void set_vertical_vanes(uint8_t value);
  void set_horizontal_vanes(uint8_t value);
  void set_auto_3d(bool value);

  void set_frame_errors_sensor(sensor::Sensor *sensor) { this->frame_errors_sensor_ = sensor; }
  void set_valid_frames_sensor(sensor::Sensor *sensor) { this->valid_frames_sensor_ = sensor; }
  void set_success_rate_sensor(sensor::Sensor *sensor) { this->success_rate_sensor_ = sensor; }
  void set_last_valid_frame_age_sensor(sensor::Sensor *sensor) { this->last_valid_frame_age_sensor_ = sensor; }

 protected:
  struct PersistedSettings {
    uint8_t version{1};
    uint8_t valid{0};
    uint8_t mode{0};
    uint8_t fan{7};
    uint8_t vertical_vanes{0};
    uint8_t horizontal_vanes{0};
    uint8_t auto_3d{0};
    uint8_t target_temperature_half_degrees{40};
  };

  void queue_persisted_settings_();
  void mark_settings_dirty_();
  void save_settings_if_due_();
  void publish_diagnostics_if_due_();
  void notify_state_();
  void update_settings_from_state_(const AcState &state);

  uint8_t frame_size_{SHORT_FRAME_SIZE};
  uint8_t command_retries_{3};
  uint8_t retries_remaining_{0};
  bool alternate_frame_{false};
  CommandSnapshot command_{};
  AcState state_{};
  std::array<uint8_t, LONG_FRAME_SIZE> tx_{};
  std::array<uint8_t, LONG_FRAME_SIZE> rx_{};
  Esp8266Transport transport_{};
  std::vector<StateListener *> listeners_{};

  ESPPreferenceObject preference_{};
  PersistedSettings settings_{};
  bool settings_dirty_{false};
  uint32_t save_at_{0};
  uint32_t valid_frames_{0};
  uint32_t frame_errors_{0};
  uint32_t last_valid_frame_at_{0};
  uint32_t diagnostics_at_{0};

  sensor::Sensor *frame_errors_sensor_{nullptr};
  sensor::Sensor *valid_frames_sensor_{nullptr};
  sensor::Sensor *success_rate_sensor_{nullptr};
  sensor::Sensor *last_valid_frame_age_sensor_{nullptr};
};

}  // namespace esphome::mhi_cns
