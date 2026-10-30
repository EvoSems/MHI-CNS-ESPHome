#include "protocol.h"

#include <cmath>

namespace esphome::mhi_cns {

namespace {
constexpr size_t DB0 = 3;
constexpr size_t DB1 = 4;
constexpr size_t DB2 = 5;
constexpr size_t DB3 = 6;
constexpr size_t DB4 = 7;
constexpr size_t DB9 = 12;
constexpr size_t DB10 = 13;
constexpr size_t DB11 = 14;
constexpr size_t DB12 = 15;
constexpr size_t DB13 = 16;
constexpr size_t DB14 = 17;
constexpr size_t CHECKSUM_HIGH = 18;
constexpr size_t CHECKSUM_LOW = 19;
constexpr size_t DB16 = 21;
constexpr size_t DB17 = 22;
constexpr size_t LONG_CHECKSUM = 32;
}  // namespace

uint16_t FrameCodec::checksum(const uint8_t *frame, size_t end) {
  uint16_t result = 0;
  for (size_t i = 0; i < end; i++)
    result += frame[i];
  return result;
}

FrameError FrameCodec::validate(const uint8_t *frame, size_t size) {
  if (size != SHORT_FRAME_SIZE && size != LONG_FRAME_SIZE)
    return FrameError::INVALID_SIZE;
  if ((frame[0] & 0xFE) != 0x6C || frame[1] != 0x80 || frame[2] != 0x04)
    return FrameError::INVALID_SIGNATURE;

  const uint16_t short_checksum = checksum(frame, CHECKSUM_HIGH);
  if (frame[CHECKSUM_HIGH] != static_cast<uint8_t>(short_checksum >> 8) ||
      frame[CHECKSUM_LOW] != static_cast<uint8_t>(short_checksum))
    return FrameError::INVALID_CHECKSUM;

  if (size == LONG_FRAME_SIZE) {
    const uint16_t long_checksum = checksum(frame, LONG_CHECKSUM);
    if (frame[LONG_CHECKSUM] != static_cast<uint8_t>(long_checksum))
      return FrameError::INVALID_CHECKSUM;
  }
  return FrameError::OK;
}

bool FrameCodec::decode_state(const uint8_t *frame, size_t size, AcState &state) {
  if (validate(frame, size) != FrameError::OK)
    return false;

  state.valid = true;
  state.power = (frame[DB0] & 0x01) != 0;
  state.mode = static_cast<HvacMode>(frame[DB0] & 0x1C);
  state.fan = frame[DB1] & 0x07;
  state.target_temperature = (frame[DB2] & 0x7F) / 2.0f;
  state.room_temperature = (static_cast<int>(frame[DB3]) - 61) / 4.0f;
  state.error_code = frame[DB4];

  const uint8_t vertical = (frame[DB0] & 0xC0) | ((frame[DB1] & 0xB0) >> 4);
  state.vertical_vanes = (vertical & 0x40) ? 5 : (vertical & 0x03) + 1;

  if (size == LONG_FRAME_SIZE) {
    const uint8_t horizontal = (frame[DB16] & 0x07) | ((frame[DB17] & 0x01) << 4);
    state.horizontal_vanes = (horizontal & 0x10) ? 8 : (horizontal & 0x07) + 1;
    state.auto_3d = (frame[DB17] & 0x04) != 0;
  }
  return true;
}

void FrameCodec::build_response(std::array<uint8_t, LONG_FRAME_SIZE> &frame, size_t size,
                                bool alternate_frame, const CommandSnapshot &command) {
  frame.fill(0);
  frame[0] = size == LONG_FRAME_SIZE ? 0xAA : 0xA9;
  frame[1] = 0x00;
  frame[2] = 0x07;
  frame[DB3] = 0xFF;
  frame[DB9] = 0xFF;
  frame[DB10] = 0xFF;
  frame[DB11] = 0xFF;
  frame[DB12] = 0xFF;
  frame[DB13] = 0x0F;
  frame[DB14] = alternate_frame ? 0x04 : 0x00;

  if (alternate_frame) {
    if (command.set_power)
      frame[DB0] |= 0x02 | (command.power ? 0x01 : 0x00);
    if (command.set_mode)
      frame[DB0] |= 0x20 | static_cast<uint8_t>(command.mode);
    if (command.set_fan)
      frame[DB1] |= 0x08 | (command.fan & 0x07);
    if (command.set_temperature) {
      const float limited = command.target_temperature < 18.0f ? 18.0f : command.target_temperature;
      frame[DB2] = 0x80 | static_cast<uint8_t>(std::lround(limited * 2.0f));
    }
    if (command.set_vertical_vanes) {
      if (command.vertical_vanes == 5) {
        frame[DB0] |= 0xC0;
      } else if (command.vertical_vanes >= 1 && command.vertical_vanes <= 4) {
        frame[DB0] |= 0x80;
        frame[DB1] |= 0x80 | ((command.vertical_vanes - 1) << 4);
      }
    }
    if (size == LONG_FRAME_SIZE && command.set_horizontal_vanes) {
      if (command.horizontal_vanes == 8) {
        frame[DB17] |= 0x0B;
      } else if (command.horizontal_vanes >= 1 && command.horizontal_vanes <= 7) {
        frame[DB17] |= 0x0A;
        frame[DB16] |= 0x10 | (command.horizontal_vanes - 1);
      }
    }
    if (size == LONG_FRAME_SIZE && command.set_auto_3d)
      frame[DB17] |= 0x0A | (command.auto_3d ? 0x04 : 0x00);
  }

  const uint16_t short_checksum = checksum(frame.data(), CHECKSUM_HIGH);
  frame[CHECKSUM_HIGH] = static_cast<uint8_t>(short_checksum >> 8);
  frame[CHECKSUM_LOW] = static_cast<uint8_t>(short_checksum);
  if (size == LONG_FRAME_SIZE)
    frame[LONG_CHECKSUM] = static_cast<uint8_t>(checksum(frame.data(), LONG_CHECKSUM));
}

}  // namespace esphome::mhi_cns
