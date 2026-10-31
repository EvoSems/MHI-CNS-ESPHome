#include "protocol.h"

#include <cmath>

namespace esphome::mhi_cns {

namespace {
constexpr size_t DB0 = 3;
constexpr size_t DB1 = 4;
constexpr size_t DB2 = 5;
constexpr size_t DB3 = 6;
constexpr size_t DB4 = 7;
constexpr size_t DB7 = 10;
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
  // 0xFF is a protocol placeholder meaning "use the indoor sensor", not a
  // measured 48.5 C. Preserve the latest real DB3 measurement on those frames.
  if (frame[DB3] != 0xFF)
    state.room_temperature = (static_cast<int>(frame[DB3]) - 61) / 4.0f;
  state.error_code = frame[DB4];
  state.compressor_running = (frame[DB7] & 0x02) != 0;

  const uint8_t vertical = (frame[DB0] & 0xC0) | ((frame[DB1] & 0xB0) >> 4);
  state.vertical_vanes = (vertical & 0x40) ? 5 : (vertical & 0x03) + 1;

  if (size == LONG_FRAME_SIZE) {
    // Match the original MHI mapping: DB17 bit 0 marks horizontal swing;
    // otherwise DB16 bits 0-2 encode the selected left/right position.
    const uint8_t horizontal = (frame[DB16] & 0x07) | ((frame[DB17] & 0x01) << 4);
    state.horizontal_vanes = (horizontal & 0x10) ? 8 : (horizontal & 0x07) + 1;
    state.auto_3d = (frame[DB17] & 0x0C) == 0x04;
  }

  const bool indoor = (frame[9] & 0x80) != 0;
  const uint8_t data_class = frame[DB10] & 0x30;
  const bool operation_data = data_class == 0x10 || data_class == 0x20;
  if (operation_data) {
    switch (frame[DB9]) {
      case 0x94:
        if (indoor) state.energy_this_run = ((frame[DB12] << 8) | frame[DB11]) * 0.25f;
        break;
      case 0x80:
        if (indoor && (frame[DB10] & 0x30) == 0x20)
          state.return_air_temperature = frame[DB11] * 0.25f - 15.0f;
        else if (!indoor)
          state.outdoor_temperature = (static_cast<int>(frame[DB11]) - 94) * 0.25f;
        break;
      case 0x81:
        if (indoor && (frame[DB10] & 0x30) == 0x20)
          state.indoor_coil_u_bend = 0.327f * frame[DB11] - 11.4f;
        else if (!indoor && (frame[DB10] & 0x30) == 0x10)
          state.indoor_coil_capillary = 0.275f * frame[DB11] - 47.0f;
        break;
      case 0x87:
        if (indoor) state.indoor_coil_header = 0.327f * frame[DB11] - 11.4f;
        break;
      case 0x1F:
        if (indoor) state.indoor_fan_speed = frame[DB10] & 0x0F;
        else state.outdoor_fan_speed = frame[DB10] & 0x0F;
        break;
      case 0x1E:
        if (indoor) state.indoor_run_hours = frame[DB11] * 100U;
        else state.compressor_run_hours = frame[DB11] * 100U;
        break;
      case 0x11: {
        if (!indoor) {
          const uint16_t raw = ((frame[DB10] << 8) | frame[DB11]) & 0x0FFF;
          state.compressor_frequency = (raw >> 8) * 25.6f + (raw & 0xFF) * 0.1f;
        }
        break;
      }
      case 0x85:
        if (!indoor) state.discharge_pipe_temperature = frame[DB11] < 0x12 ? NAN : frame[DB11] / 2.0f + 32.0f;
        break;
      case 0x82:
        if (!indoor) state.outdoor_heat_exchanger_temperature = 0.327f * frame[DB11] - 11.4f;
        break;
      case 0xB1:
        if (!indoor) state.discharge_superheat = frame[DB11] / 2.0f;
        break;
      case 0x90:
        if (!indoor) state.current_amps = frame[DB11] * 14.0f / 51.0f;
        break;
      case 0x7C:
        if (!indoor) state.protection_code = frame[DB11];
        break;
      case 0x0C:
        if (!indoor) state.defrost = (frame[DB10] & 0x01) != 0;
        break;
      case 0x13:
        if (!indoor) state.expansion_valve = (frame[DB12] << 8) | frame[DB11];
        break;
      default:
        break;
    }
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
  frame[9] = 0x80;
  frame[DB9] = 0xFF;
  frame[DB10] = 0xFF;
  frame[DB11] = 0xFF;
  frame[DB12] = 0xFF;
  frame[DB13] = 0x0F;
  frame[DB14] = alternate_frame ? 0x04 : 0x00;
  if (command.set_room_temperature && !std::isnan(command.room_temperature))
    frame[DB3] = static_cast<uint8_t>(std::lround(command.room_temperature * 4.0f + 61.0f));

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
    if (command.request_operating_data) {
      frame[9] = command.operating_data_selector;
      frame[DB9] = command.operating_data_code;
    }
  }

  const uint16_t short_checksum = checksum(frame.data(), CHECKSUM_HIGH);
  frame[CHECKSUM_HIGH] = static_cast<uint8_t>(short_checksum >> 8);
  frame[CHECKSUM_LOW] = static_cast<uint8_t>(short_checksum);
  if (size == LONG_FRAME_SIZE)
    frame[LONG_CHECKSUM] = static_cast<uint8_t>(checksum(frame.data(), LONG_CHECKSUM));
}

}  // namespace esphome::mhi_cns
