#include "bme280.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <initializer_list>

float bme280_temperature(const Bme280Calib &c, int32_t adcT, int32_t &tFine) {
  int32_t v1 = ((((adcT >> 3) - ((int32_t)c.T1 << 1))) * ((int32_t)c.T2)) >> 11;
  int32_t v2 = (((((adcT >> 4) - ((int32_t)c.T1)) * ((adcT >> 4) - ((int32_t)c.T1))) >> 12) * ((int32_t)c.T3)) >> 14;
  tFine = v1 + v2;
  return ((tFine * 5 + 128) >> 8) / 100.0f;
}

float bme280_pressure(const Bme280Calib &c, int32_t adcP, int32_t tFine) {
  // Datasheet-formule; schuiven naar links van mogelijk negatieve getallen als vermenigvuldiging (zelfde uitkomst)
  int64_t v1 = (int64_t)tFine - 128000;
  int64_t v2 = v1 * v1 * (int64_t)c.P6;
  v2 = v2 + v1 * (int64_t)c.P5 * 131072;      // << 17
  v2 = v2 + (int64_t)c.P4 * 34359738368LL;    // << 35
  v1 = ((v1 * v1 * (int64_t)c.P3) >> 8) + v1 * (int64_t)c.P2 * 4096;  // << 12
  v1 = (140737488355328LL + v1) * (int64_t)c.P1 >> 33;                // 1 << 47
  if (v1 == 0) return NAN;  // deling door nul voorkomen
  int64_t p = 1048576 - adcP;
  p = ((p * 2147483648LL - v2) * 3125) / v1;  // << 31
  v1 = (((int64_t)c.P9) * (p >> 13) * (p >> 13)) >> 25;
  v2 = (((int64_t)c.P8) * p) >> 19;
  p = ((p + v1 + v2) >> 8) + (int64_t)c.P7 * 16;  // << 4
  return p / 256.0f;  // Q24.8 -> Pa
}

bool Bme280::readRegs(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(addr_);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(addr_, len) != len) return false;
  for (int i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool Bme280::writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr_);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool Bme280::begin() {
  for (uint8_t a : {0x76, 0x77}) {
    addr_ = a;
    uint8_t id;
    if (readRegs(0xD0, &id, 1) && (id == 0x60 || id == 0x58)) {  // BME280 of BMP280
      chip_ = id;
      uint8_t b[24];
      if (!readRegs(0x88, b, 24)) break;
      auto u = [&](int i) { return (uint16_t)(b[i] | (b[i + 1] << 8)); };
      auto s = [&](int i) { return (int16_t)u(i); };
      cal_ = {u(0), s(2), s(4), u(6), s(8), s(10), s(12), s(14), s(16), s(18), s(20), s(22)};
      if (cal_.T1 == 0 || cal_.P1 == 0) break;  // geen geldige kalibratie
      if (chip_ == 0x60) writeReg(0xF2, 0x00);  // vocht niet meten
      writeReg(0xF5, 0x00);                     // geen filter (wij middelen zelf)
      return true;
    }
  }
  addr_ = 0;
  return false;
}

bool Bme280::read(float &hPa, float &tempC) {
  if (!addr_) return false;
  // Eén meting: temperatuur x1, druk x16 (ruisarm), forced mode
  if (!writeReg(0xF4, (1 << 5) | (5 << 2) | 1)) return false;
  uint8_t st = 0;
  for (int i = 0; i < 20; i++) {  // duurt ± 40 ms
    delay(5);
    if (!readRegs(0xF3, &st, 1)) return false;
    if (!(st & 0x08)) break;
  }
  uint8_t d[6];
  if (!readRegs(0xF7, d, 6)) return false;
  int32_t adcP = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
  int32_t adcT = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
  if (adcP == 0x80000 || adcT == 0x80000) return false;  // "geen meting"
  int32_t tFine;
  tempC = bme280_temperature(cal_, adcT, tFine);
  float pa = bme280_pressure(cal_, adcP, tFine);
  if (isnan(pa) || pa < 30000 || pa > 110000) return false;
  hPa = pa / 100.0f;
  return true;
}
