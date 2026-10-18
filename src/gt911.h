#pragma once
#include <Wire.h>

// Minimale GT911-driver: één aanraakpunt
class GT911 {
 public:
  bool begin(int sda, int scl) {
    Wire.begin(sda, scl, 400000);
    for (uint8_t a : {0x5D, 0x14}) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) {
        addr_ = a;
        return true;
      }
    }
    return false;
  }

  // Geeft true terug zolang het scherm aangeraakt wordt
  bool read(uint16_t &x, uint16_t &y) {
    uint8_t status;
    if (readReg(0x814E, &status, 1) && (status & 0x80)) {
      touched_ = (status & 0x0F) > 0;
      uint8_t p[4];
      if (touched_ && readReg(0x8150, p, 4)) {
        x_ = p[0] | (p[1] << 8);
        y_ = p[2] | (p[3] << 8);
      }
      writeReg(0x814E, 0);
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

  uint8_t addr_ = 0x5D;
  bool touched_ = false;
  uint16_t x_ = 0, y_ = 0;
};
