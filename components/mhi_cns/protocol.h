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
  uint8_t vertical_vanes{0};
  uint8_t horizontal_vanes{0};
  bool auto_3d{false};
  uint8_t error_code{0};
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
