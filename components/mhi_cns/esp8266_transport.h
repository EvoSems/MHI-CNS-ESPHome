#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::mhi_cns {

enum class TransportResult : int8_t {
  OK = 0,
  NO_FRAME = 1,
  CLOCK_LOW_TIMEOUT = -1,
  CLOCK_HIGH_TIMEOUT = -2,
};

class Esp8266Transport {
 public:
  void configure(uint8_t sck_pin, uint8_t mosi_pin, uint8_t miso_pin);
  void setup();
  TransportResult exchange(const uint8_t *tx, uint8_t *rx, size_t size, uint32_t timeout_us);

 private:
  uint8_t sck_pin_{14};
  uint8_t mosi_pin_{13};
  uint8_t miso_pin_{12};
  uint32_t last_frame_at_us_{0};
};

}  // namespace esphome::mhi_cns
