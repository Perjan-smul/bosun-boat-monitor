#pragma once
#include <Wire.h>

namespace cst3240_detail {
volatile bool intFlag = false;
void IRAM_ATTR onInt() { intFlag = true; }
}  // namespace cst3240_detail

// Minimale CST3240-driver (T-Panel S3), protocol zoals LilyGO's TouchLib "CST mutual".
// De chip trekt INT laag bij elk nieuw rapport; zolang er aangeraakt wordt komen die
// rapporten continu. Blijven ze uit, dan is de vinger weg.
class CST3240 {
 public:
  bool begin(uint8_t addr, int intPin) {
    addr_ = addr;
    pinMode(intPin, INPUT_PULLUP);
    attachInterrupt(intPin, cst3240_detail::onInt, FALLING);
    Wire.beginTransmission(addr_);
    return Wire.endTransmission() == 0;
  }

  bool read(uint16_t &x, uint16_t &y) {
    if (cst3240_detail::intFlag) {
      cst3240_detail::intFlag = false;
      uint8_t raw[7];
      if (readReg(0xD000, raw, sizeof(raw))) {
        writeReg(0xD000, 0xAB);  // sync, volgende meting vrijgeven
        uint8_t points = raw[5] & 0x0F;
        touched_ = points > 0 && (raw[0] & 0x0F) != 0 && raw[4] > 0;
        if (touched_) {
          x_ = (raw[1] << 4) | (raw[3] >> 4);
          y_ = (raw[2] << 4) | (raw[3] & 0x0F);
        }
        lastReport_ = millis();
      }
    } else if (touched_ && millis() - lastReport_ > 60) {
      touched_ = false;
    }
    x = x_;
    y = y_;
    return touched_;
  }

 private:
  bool readReg(uint16_t reg, uint8_t *buf, size_t len) {
    Wire.beginTransmission(addr_);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    if (Wire.endTransmission() != 0) return false;
    if (Wire.requestFrom(addr_, (uint8_t)len) != len) return false;
    for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
  }

  void writeReg(uint16_t reg, uint8_t val) {
    Wire.beginTransmission(addr_);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    Wire.write(val);
    Wire.endTransmission();
  }

  uint8_t addr_ = 0x5A;
  bool touched_ = false;
  uint32_t lastReport_ = 0;
  uint16_t x_ = 0, y_ = 0;
};
