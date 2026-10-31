#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
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
  void set_silent_operation(bool value);
  void set_external_temperature_sensor(sensor::Sensor *sensor) { this->external_temperature_sensor_ = sensor; }
  void set_external_temperature_timeout(uint32_t value) { this->external_temperature_timeout_ms_ = value; }

  void set_frame_errors_sensor(sensor::Sensor *sensor) { this->frame_errors_sensor_ = sensor; }
  void set_valid_frames_sensor(sensor::Sensor *sensor) { this->valid_frames_sensor_ = sensor; }
  void set_success_rate_sensor(sensor::Sensor *sensor) { this->success_rate_sensor_ = sensor; }
  void set_last_valid_frame_age_sensor(sensor::Sensor *sensor) { this->last_valid_frame_age_sensor_ = sensor; }
  void set_clock_timeouts_sensor(sensor::Sensor *sensor) { this->clock_timeouts_sensor_ = sensor; }
  void set_signature_errors_sensor(sensor::Sensor *sensor) { this->signature_errors_sensor_ = sensor; }
  void set_checksum_errors_sensor(sensor::Sensor *sensor) { this->checksum_errors_sensor_ = sensor; }
  void set_command_failures_sensor(sensor::Sensor *sensor) { this->command_failures_sensor_ = sensor; }
#define MHI_SENSOR_SETTER(name) void set_##name##_sensor(sensor::Sensor *s) { this->name##_sensor_ = s; }
  MHI_SENSOR_SETTER(error_code)
  MHI_SENSOR_SETTER(indoor_temperature)
  MHI_SENSOR_SETTER(outdoor_temperature)
  MHI_SENSOR_SETTER(return_air_temperature)
  MHI_SENSOR_SETTER(current)
  MHI_SENSOR_SETTER(compressor_frequency)
  MHI_SENSOR_SETTER(energy_this_run)
  MHI_SENSOR_SETTER(indoor_coil_u_bend)
  MHI_SENSOR_SETTER(indoor_coil_capillary)
  MHI_SENSOR_SETTER(indoor_coil_header)
  MHI_SENSOR_SETTER(outdoor_heat_exchanger_temperature)
  MHI_SENSOR_SETTER(discharge_pipe_temperature)
  MHI_SENSOR_SETTER(discharge_superheat)
  MHI_SENSOR_SETTER(expansion_valve)
  MHI_SENSOR_SETTER(indoor_fan_speed)
  MHI_SENSOR_SETTER(outdoor_fan_speed)
  MHI_SENSOR_SETTER(protection_code)
  MHI_SENSOR_SETTER(indoor_run_hours)
  MHI_SENSOR_SETTER(compressor_run_hours)
#undef MHI_SENSOR_SETTER
  void set_healthy_sensor(binary_sensor::BinarySensor *s) { this->healthy_sensor_ = s; }
  void set_defrost_sensor(binary_sensor::BinarySensor *s) { this->defrost_sensor_ = s; }
  void set_compressor_running_sensor(binary_sensor::BinarySensor *s) { this->compressor_running_sensor_ = s; }

 protected:
  struct PersistedSettings {
    uint8_t version{2};
    uint8_t valid{0};
    uint8_t mode{0};
    uint8_t fan{7};
    uint8_t vertical_vanes{0};
    uint8_t horizontal_vanes{0};
    uint8_t auto_3d{0};
    uint8_t target_temperature_half_degrees{40};
    uint8_t silent_operation{0};
  };

  struct LegacyPersistedSettingsV1 {
    uint8_t version;
    uint8_t valid;
    uint8_t mode;
    uint8_t fan;
    uint8_t vertical_vanes;
    uint8_t horizontal_vanes;
    uint8_t auto_3d;
    uint8_t target_temperature_half_degrees;
  };

  void queue_persisted_settings_();
  void mark_settings_dirty_();
  void save_settings_if_due_();
  void publish_diagnostics_if_due_();
  void notify_state_();
  void update_settings_from_state_(const AcState &state);
  void publish_operating_data_();
  bool command_acknowledged_(const AcState &state) const;

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
  uint32_t clock_timeouts_{0};
  uint32_t signature_errors_{0};
  uint32_t checksum_errors_{0};
  uint32_t last_valid_frame_at_{0};
  uint32_t diagnostics_at_{0};
  uint32_t operating_data_at_{0};
  uint8_t operating_data_index_{0};
  uint32_t command_failures_{0};
  float room_temperature_sum_{0.0f};
  uint16_t room_temperature_samples_{0};
  uint32_t room_temperature_report_at_{0};
  bool silent_ack_pending_{false};
  bool pending_silent_restore_{false};
  bool silent_ack_value_{false};
  uint32_t silent_ack_deadline_{0};
  sensor::Sensor *external_temperature_sensor_{nullptr};
  float external_temperature_{NAN};
  uint32_t external_temperature_at_{0};
  uint32_t external_temperature_timeout_ms_{300000};

  sensor::Sensor *frame_errors_sensor_{nullptr};
  sensor::Sensor *valid_frames_sensor_{nullptr};
  sensor::Sensor *success_rate_sensor_{nullptr};
  sensor::Sensor *last_valid_frame_age_sensor_{nullptr};
  sensor::Sensor *clock_timeouts_sensor_{nullptr};
  sensor::Sensor *signature_errors_sensor_{nullptr};
  sensor::Sensor *checksum_errors_sensor_{nullptr};
  sensor::Sensor *command_failures_sensor_{nullptr};
#define MHI_SENSOR_MEMBER(name) sensor::Sensor *name##_sensor_{nullptr};
  MHI_SENSOR_MEMBER(error_code)
  MHI_SENSOR_MEMBER(indoor_temperature)
  MHI_SENSOR_MEMBER(outdoor_temperature)
  MHI_SENSOR_MEMBER(return_air_temperature)
  MHI_SENSOR_MEMBER(current)
  MHI_SENSOR_MEMBER(compressor_frequency)
  MHI_SENSOR_MEMBER(energy_this_run)
  MHI_SENSOR_MEMBER(indoor_coil_u_bend)
  MHI_SENSOR_MEMBER(indoor_coil_capillary)
  MHI_SENSOR_MEMBER(indoor_coil_header)
  MHI_SENSOR_MEMBER(outdoor_heat_exchanger_temperature)
  MHI_SENSOR_MEMBER(discharge_pipe_temperature)
  MHI_SENSOR_MEMBER(discharge_superheat)
  MHI_SENSOR_MEMBER(expansion_valve)
  MHI_SENSOR_MEMBER(indoor_fan_speed)
  MHI_SENSOR_MEMBER(outdoor_fan_speed)
  MHI_SENSOR_MEMBER(protection_code)
  MHI_SENSOR_MEMBER(indoor_run_hours)
  MHI_SENSOR_MEMBER(compressor_run_hours)
#undef MHI_SENSOR_MEMBER
  binary_sensor::BinarySensor *healthy_sensor_{nullptr};
  binary_sensor::BinarySensor *defrost_sensor_{nullptr};
  binary_sensor::BinarySensor *compressor_running_sensor_{nullptr};
};

}  // namespace esphome::mhi_cns
