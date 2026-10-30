#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace esphome::mhi_cns {

constexpr size_t SHORT_FRAME_SIZE = 20;
constexpr size_t LONG_FRAME_SIZE = 33;

enum class FrameError : int8_t {
  OK = 0,
  INVALID_SIZE = -1,
  INVALID_SIGNATURE = -2,
  INVALID_CHECKSUM = -3,
};

enum class HvacMode : uint8_t {
  AUTO = 0x00,
  DRY = 0x04,
  COOL = 0x08,
  FAN = 0x0C,
  HEAT = 0x10,
};

struct AcState {
  bool valid{false};
  bool power{false};
  HvacMode mode{HvacMode::AUTO};
  uint8_t fan{7};
  float target_temperature{20.0f};
  float room_temperature{NAN};
  float reported_room_temperature{NAN};
  uint8_t vertical_vanes{0};
  uint8_t horizontal_vanes{0};
  bool auto_3d{false};
  uint8_t error_code{0};
  bool silent_operation{false};
  bool defrost{false};
  bool compressor_running{false};
  bool occupancy_detected{false};
  bool self_clean_operation{false};
  float outdoor_temperature{NAN};
  float return_air_temperature{NAN};
  float current_amps{NAN};
  float compressor_frequency{NAN};
  float energy_this_run{NAN};
  float indoor_coil_u_bend{NAN};
  float indoor_coil_capillary{NAN};
  float indoor_coil_header{NAN};
  float outdoor_heat_exchanger_temperature{NAN};
  float discharge_pipe_temperature{NAN};
  float discharge_superheat{NAN};
  uint16_t expansion_valve{0};
  uint8_t indoor_fan_speed{0};
  uint8_t outdoor_fan_speed{0};
  uint8_t protection_code{0};
  uint32_t indoor_run_hours{0};
  uint32_t compressor_run_hours{0};
};

struct CommandSnapshot {
  bool set_power{false};
  bool power{false};
  bool set_mode{false};
  HvacMode mode{HvacMode::AUTO};
  bool set_fan{false};
  uint8_t fan{7};
  bool set_temperature{false};
  float target_temperature{20.0f};
  bool set_vertical_vanes{false};
  uint8_t vertical_vanes{0};
  bool set_horizontal_vanes{false};
  uint8_t horizontal_vanes{0};
  bool set_auto_3d{false};
  bool auto_3d{false};
  bool set_silent{false};
  bool silent{false};
  bool set_room_temperature{false};
  float room_temperature{NAN};
  bool request_operating_data{false};
  uint8_t operating_data_selector{0x80};
  uint8_t operating_data_code{0xFF};
};

class FrameCodec {
 public:
  static uint16_t checksum(const uint8_t *frame, size_t end);
  static FrameError validate(const uint8_t *frame, size_t size);
  static bool decode_state(const uint8_t *frame, size_t size, AcState &state);
  static void build_response(std::array<uint8_t, LONG_FRAME_SIZE> &frame, size_t size,
                             bool alternate_frame, const CommandSnapshot &command);
};

}  // namespace esphome::mhi_cns
