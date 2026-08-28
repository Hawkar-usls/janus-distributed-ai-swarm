#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace janus_adv_elite {

// Non-blocking single-shot SHT3X reader.
// The stock M5Unit-ENV SHT3X::update() sleeps for 200 ms + 50 ms. That is
// unacceptable in the same loop that owns a 30/60 Hz UI. This helper sends a
// no-clock-stretch single-shot command, returns immediately, and collects the
// six-byte result on a later loop iteration.
class AdvSht3xAsync {
 public:
  bool begin(TwoWire* wire, uint8_t addr = 0x44) {
    wire_ = wire;
    addr_ = addr;
    if (!wire_) return false;
    wire_->beginTransmission(addr_);
    present_ = wire_->endTransmission() == 0;
    return present_;
  }

  bool present() const { return present_; }
  bool pending() const { return pending_; }

  bool start(uint32_t nowMs) {
    if (!present_ || !wire_ || pending_) return false;
    wire_->beginTransmission(addr_);
    // SHT3X single shot, high repeatability, clock stretching disabled.
    wire_->write(0x24);
    wire_->write(0x00);
    if (wire_->endTransmission() != 0) {
      failures_++;
      return false;
    }
    pending_ = true;
    readyAtMs_ = nowMs + 20UL;
    return true;
  }

  // Returns true only when a new CRC-valid sample has been produced.
  bool poll(uint32_t nowMs, float& tempC, float& humidity) {
    if (!pending_ || !wire_) return false;
    if ((int32_t)(nowMs - readyAtMs_) < 0) return false;
    pending_ = false;

    const uint8_t got = wire_->requestFrom(addr_, (uint8_t)6);
    if (got != 6) {
      while (wire_->available()) (void)wire_->read();
      failures_++;
      return false;
    }

    uint8_t b[6];
    for (uint8_t i = 0; i < 6; ++i) b[i] = (uint8_t)wire_->read();
    if (crc8(b, 2) != b[2] || crc8(b + 3, 2) != b[5]) {
      failures_++;
      return false;
    }

    const uint16_t rawT = ((uint16_t)b[0] << 8) | b[1];
    const uint16_t rawH = ((uint16_t)b[3] << 8) | b[4];
    tempC = -45.0f + 175.0f * ((float)rawT / 65535.0f);
    humidity = 100.0f * ((float)rawH / 65535.0f);
    samples_++;
    return true;
  }

  uint32_t samples() const { return samples_; }
  uint32_t failures() const { return failures_; }

 private:
  TwoWire* wire_ = nullptr;
  uint8_t addr_ = 0x44;
  bool present_ = false;
  bool pending_ = false;
  uint32_t readyAtMs_ = 0;
  uint32_t samples_ = 0;
  uint32_t failures_ = 0;

  static uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; ++i) {
      crc ^= data[i];
      for (uint8_t bit = 0; bit < 8; ++bit) {
        crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
      }
    }
    return crc;
  }
};

}  // namespace janus_adv_elite
