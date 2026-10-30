#include "esp8266_transport.h"

#include <Arduino.h>
#include <esp8266_peri.h>

namespace esphome::mhi_cns {

void Esp8266Transport::configure(uint8_t sck_pin, uint8_t mosi_pin, uint8_t miso_pin) {
  this->sck_pin_ = sck_pin;
  this->mosi_pin_ = mosi_pin;
  this->miso_pin_ = miso_pin;
}

void Esp8266Transport::setup() {
  pinMode(this->sck_pin_, INPUT);
  pinMode(this->mosi_pin_, INPUT);
  pinMode(this->miso_pin_, OUTPUT);
  GPOC = 1U << this->miso_pin_;
}

TransportResult Esp8266Transport::exchange(const uint8_t *tx, uint8_t *rx, size_t size,
                                           uint32_t timeout_us) {
  const uint32_t now = micros();
  if (this->last_frame_at_us_ != 0 && static_cast<uint32_t>(now - this->last_frame_at_us_) < 18000)
    return TransportResult::NO_FRAME;

  const uint32_t started = micros();
  uint32_t high_since = micros();
  while (static_cast<uint32_t>(micros() - high_since) < 5000) {
    if ((GPIP(this->sck_pin_)) == 0)
      high_since = micros();
    if (static_cast<uint32_t>(micros() - started) > timeout_us)
      return TransportResult::CLOCK_LOW_TIMEOUT;
    yield();
  }

  for (size_t byte_index = 0; byte_index < size; byte_index++) {
    uint8_t received = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
      const uint8_t mask = 1U << bit;
      while (GPIP(this->sck_pin_) != 0) {
        if (static_cast<uint32_t>(micros() - started) > timeout_us)
          return TransportResult::CLOCK_HIGH_TIMEOUT;
      }
      if ((tx[byte_index] & mask) != 0)
        GPOS = 1U << this->miso_pin_;
      else
        GPOC = 1U << this->miso_pin_;

      while (GPIP(this->sck_pin_) == 0) {
        if (static_cast<uint32_t>(micros() - started) > timeout_us)
          return TransportResult::CLOCK_LOW_TIMEOUT;
      }
      if (GPIP(this->mosi_pin_) != 0)
        received |= mask;
    }
    rx[byte_index] = received;
  }

  GPOC = 1U << this->miso_pin_;
  this->last_frame_at_us_ = micros();
  return TransportResult::OK;
}

}  // namespace esphome::mhi_cns
