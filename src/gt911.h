#pragma once
#include <Wire.h>
#include <stdio.h>

// Minimale GT911-driver: één aanraakpunt
class GT911 {
 public:
  bool begin(int sda, int scl) {
    Wire.begin(sda, scl, 400000);
    // Na de reset heeft de GT911 even nodig; een paar keer proberen op beide mogelijke adressen
    for (int tries = 0; tries < 10; tries++) {
      for (uint8_t a : {0x5D, 0x14}) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
          addr_ = a;
          return true;
        }
      }
      delay(20);
    }
    return false;
  }

  // Voor het logboek: adres, product-ID ("911"), configversie en resolutie uit de GT911.
  // Geeft false als de chip niet antwoordt.
  bool info(char *out, size_t len) {
    uint8_t id[4] = {0}, cfg[5] = {0};
    bool ok = readReg(0x8140, id, 4) && readReg(0x8047, cfg, 5);
    snprintf(out, len, "adres 0x%02X, ID '%c%c%c%c', config v%u, %ux%u%s", addr_, id[0] ? id[0] : '-',
             id[1] ? id[1] : '-', id[2] ? id[2] : '-', id[3] ? id[3] : '-', cfg[0], cfg[1] | (cfg[2] << 8),
             cfg[3] | (cfg[4] << 8), ok ? "" : " (lezen mislukt)");
    return ok;
  }

  uint32_t errors() const { return errors_; }  // mislukte leesacties sinds de start

  // Geeft true terug zolang het scherm aangeraakt wordt. Zoals Waveshare's driver (SensorLib):
  // het aantal punten uit 0x814E, zonder op de 'buffer klaar'-bit te wachten; daarna wissen.
  bool read(uint16_t &x, uint16_t &y) {
    uint8_t status;
    if (!readReg(0x814E, &status, 1)) {
      errors_++;
      x = x_;
      y = y_;
      return touched_;
    }
    lastStatus_ = status;
    uint8_t points = status & 0x0F;
    if (status & 0x80) {
      touched_ = points > 0 && points <= 5;
      uint8_t p[4];
      if (touched_ && readReg(0x8150, p, 4)) {
        x_ = p[0] | (p[1] << 8);
        y_ = p[2] | (p[3] << 8);
      }
    } else if (points == 0) {
      touched_ = false;
    }
    if (status) writeReg(0x814E, 0);
    x = x_;
    y = y_;
    return touched_;
  }

  uint8_t lastStatus() const { return lastStatus_; }

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
  uint32_t errors_ = 0;
  uint8_t lastStatus_ = 0;
  bool touched_ = false;
  uint16_t x_ = 0, y_ = 0;
};
