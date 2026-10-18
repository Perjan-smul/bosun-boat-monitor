#pragma once
#include <stdint.h>

// Bosch BME280 / BMP280 luchtdruksensor op I2C (0x76 of 0x77), in "forced mode": alleen meten als erom
// gevraagd wordt, zodat hij niet opwarmt. Rekenwerk volgens de datasheet (gehele getallen, sectie 4.2.3 / 8.2).
struct Bme280Calib {
  uint16_t T1;
  int16_t T2, T3;
  uint16_t P1;
  int16_t P2, P3, P4, P5, P6, P7, P8, P9;
};

// Los van de I2C-bus, zodat het op de pc te testen is. Temperatuur in °C, druk in Pa.
float bme280_temperature(const Bme280Calib &c, int32_t adcT, int32_t &tFine);
float bme280_pressure(const Bme280Calib &c, int32_t adcP, int32_t tFine);

class Bme280 {
 public:
  bool begin();                  // zoekt op 0x76 en 0x77 (na Wire.begin)
  bool read(float &hPa, float &tempC);
  bool present() const { return addr_ != 0; }
  bool isBmp() const { return chip_ == 0x58; }

 private:
  bool readRegs(uint8_t reg, uint8_t *buf, uint8_t len);
  bool writeReg(uint8_t reg, uint8_t val);
  uint8_t addr_ = 0, chip_ = 0;
  Bme280Calib cal_{};
};
