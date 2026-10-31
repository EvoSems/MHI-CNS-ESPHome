#include "../components/mhi_cns/protocol.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

using namespace esphome::mhi_cns;

static std::array<uint8_t, LONG_FRAME_SIZE> make_status(size_t size) {
  std::array<uint8_t, LONG_FRAME_SIZE> frame{};
  frame[0] = size == LONG_FRAME_SIZE ? 0x6D : 0x6C;
  frame[1] = 0x80;
  frame[2] = 0x04;
  frame[3] = 0x01 | static_cast<uint8_t>(HvacMode::COOL);
  frame[4] = 0x07;
  frame[5] = 48;
  frame[6] = 157;
  frame[7] = 0;
  frame[10] = 2;
  if (size == LONG_FRAME_SIZE) {
    frame[21] = 2;
    frame[22] = 4;
  }
  const uint16_t short_sum = FrameCodec::checksum(frame.data(), 18);
  frame[18] = short_sum >> 8;
  frame[19] = short_sum;
  if (size == LONG_FRAME_SIZE)
    frame[32] = FrameCodec::checksum(frame.data(), 32);
  return frame;
}

static void refresh_checksum(std::array<uint8_t, LONG_FRAME_SIZE> &frame) {
  const uint16_t short_sum = FrameCodec::checksum(frame.data(), 18);
  frame[18] = short_sum >> 8;
  frame[19] = short_sum;
  frame[32] = FrameCodec::checksum(frame.data(), 32);
}

int main() {
  for (const size_t size : {SHORT_FRAME_SIZE, LONG_FRAME_SIZE}) {
    auto frame = make_status(size);
    assert(FrameCodec::validate(frame.data(), size) == FrameError::OK);
    AcState state{};
    assert(FrameCodec::decode_state(frame.data(), size, state));
    assert(state.power);
    assert(state.mode == HvacMode::COOL);
    assert(state.fan == 7);
    assert(std::fabs(state.target_temperature - 24.0f) < 0.01f);
    assert(std::fabs(state.room_temperature - 24.0f) < 0.01f);
    assert(state.compressor_running);
    if (size == LONG_FRAME_SIZE) {
      assert(state.horizontal_vanes == 3);
      assert(state.auto_3d);
    }

    frame[5] ^= 1;
    assert(FrameCodec::validate(frame.data(), size) == FrameError::INVALID_CHECKSUM);
  }

  CommandSnapshot command{};
  command.set_power = true;
  command.power = true;
  command.set_mode = true;
  command.mode = HvacMode::HEAT;
  command.set_temperature = true;
  command.target_temperature = 22.5f;
  std::array<uint8_t, LONG_FRAME_SIZE> tx{};
  FrameCodec::build_response(tx, LONG_FRAME_SIZE, true, command);
  assert((tx[3] & 0x03) == 0x03);
  assert((tx[3] & 0x1C) == static_cast<uint8_t>(HvacMode::HEAT));
  assert(tx[5] == (0x80 | 45));

  command = {};
  command.set_silent = true;
  command.silent = true;
  command.set_room_temperature = true;
  command.room_temperature = 23.5f;
  FrameCodec::build_response(tx, LONG_FRAME_SIZE, true, command);
  assert(tx[6] == 155);
  assert(tx[9] == 0x00);
  assert(tx[12] == 0x21);
  assert(tx[13] == 0x01);

  command = {};
  command.request_operating_data = true;
  command.operating_data_selector = 0x40;
  command.operating_data_code = 0x90;
  FrameCodec::build_response(tx, LONG_FRAME_SIZE, true, command);
  assert(tx[9] == 0x40);
  assert(tx[12] == 0x90);

  // Operating-data replies arrive one value at a time. Decoding a later reply
  // into the accumulated state must preserve values from earlier replies.
  AcState operating_state{};
  auto outdoor = make_status(LONG_FRAME_SIZE);
  outdoor[9] = 0x40;
  outdoor[12] = 0x80;
  outdoor[13] = 0x10;
  outdoor[14] = 134;  // 10.0 C
  refresh_checksum(outdoor);
  assert(FrameCodec::decode_state(outdoor.data(), LONG_FRAME_SIZE, operating_state));
  assert(std::fabs(operating_state.outdoor_temperature - 10.0f) < 0.01f);

  auto header = make_status(LONG_FRAME_SIZE);
  header[9] = 0xC0;
  header[12] = 0x87;
  header[13] = 0x10;
  header[14] = 88;  // 17.376 C
  refresh_checksum(header);
  assert(FrameCodec::decode_state(header.data(), LONG_FRAME_SIZE, operating_state));
  assert(std::fabs(operating_state.indoor_coil_header - 17.376f) < 0.01f);
  assert(std::fabs(operating_state.outdoor_temperature - 10.0f) < 0.01f);

  auto return_air = make_status(LONG_FRAME_SIZE);
  return_air[9] = 0xC0;
  return_air[12] = 0x80;
  return_air[13] = 0x20;
  return_air[14] = 156;  // 24.0 C
  refresh_checksum(return_air);
  assert(FrameCodec::decode_state(return_air.data(), LONG_FRAME_SIZE, operating_state));
  assert(std::fabs(operating_state.return_air_temperature - 24.0f) < 0.01f);

  auto capillary = make_status(LONG_FRAME_SIZE);
  capillary[9] = 0x40;
  capillary[12] = 0x81;
  capillary[13] = 0x10;
  capillary[14] = 200;  // 8.0 C
  refresh_checksum(capillary);
  assert(FrameCodec::decode_state(capillary.data(), LONG_FRAME_SIZE, operating_state));
  assert(std::fabs(operating_state.indoor_coil_capillary - 8.0f) < 0.01f);

  auto placeholder = make_status(LONG_FRAME_SIZE);
  placeholder[6] = 0xFF;
  refresh_checksum(placeholder);
  assert(FrameCodec::decode_state(placeholder.data(), LONG_FRAME_SIZE, operating_state));
  assert(std::fabs(operating_state.room_temperature - 24.0f) < 0.01f);

  std::cout << "protocol tests passed\n";
  return 0;
}
