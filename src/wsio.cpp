#include "board.h"
#include "wsio.h"
#include <Arduino.h>
#include <Wire.h>
#include "log.h"

// Registers en pinnen zoals in Waveshare's eigen voorbeelden (WS_CH32_IO en IOExpander_general_example)
namespace {

constexpr uint8_t CH32_ADDR = 0x24;
constexpr uint8_t CH32_REG_DIR = 0x02, CH32_REG_OUT = 0x03, CH32_REG_PWM = 0x05;
constexpr uint8_t CH32_TP_RST = 1 << 1, CH32_LCD_RST = 1 << 3, CH32_SYS_EN = 1 << 5, CH32_BEE_EN = 1 << 6;

constexpr uint8_t TCA_ADDR = 0x20;
constexpr uint8_t TCA_REG_OUT = 0x01, TCA_REG_CFG = 0x03;  // configuratie: 1 = ingang
constexpr uint8_t TCA_TP_RST = 1 << 0, TCA_BL = 1 << 1, TCA_LCD_RST = 1 << 2, TCA_BEE = 1 << 5;

WsIo type = WsIo::None;
uint8_t out = 0;  // laatst geschreven uitgangen (beide chips schrijven de hele poort in één keer)

bool probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  for (int i = 0; i < 3; i++) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    if (Wire.endTransmission() == 0) return true;
    delay(10);
  }
  return false;
}

void writeOut() {
  if (type == WsIo::Ch32) writeReg(CH32_ADDR, CH32_REG_OUT, out);
  else if (type == WsIo::Tca9554) writeReg(TCA_ADDR, TCA_REG_OUT, out);
}

}  // namespace

WsIo wsio_begin() {
  if (probe(CH32_ADDR)) type = WsIo::Ch32;
  else if (probe(TCA_ADDR)) type = WsIo::Tca9554;
  else type = WsIo::None;

  if (type == WsIo::Ch32) {
    // Alles laag (scherm en touch in reset), dan systeemvoeding aan en resets los; zoemer uit
    writeReg(CH32_ADDR, CH32_REG_DIR, 0xFF);
    out = 0;
    writeOut();
    delay(200);
    writeReg(CH32_ADDR, CH32_REG_DIR, 0xFF);
    out = CH32_SYS_EN | CH32_LCD_RST | CH32_TP_RST;
    writeOut();
    writeReg(CH32_ADDR, CH32_REG_PWM, 0);  // achtergrondverlichting fadet daarna in
    delay(200);
    log_line("[board] Waveshare V4 (CH32V003) gevonden");
  } else if (type == WsIo::Tca9554) {
    out = 0;
    writeOut();
    writeReg(TCA_ADDR, TCA_REG_CFG, (uint8_t) ~(TCA_TP_RST | TCA_BL | TCA_LCD_RST | TCA_BEE));
    delay(100);
    out = TCA_TP_RST | TCA_LCD_RST;
    writeOut();
    delay(200);
    log_line("[board] Waveshare V3 (TCA9554) gevonden");
  } else {
    log_line("[board] geen hulpchip gevonden (0x24/0x20): scherm en touch mogelijk zonder reset");
  }
  return type;
}

WsIo wsio_type() { return type; }

void wsio_backlight(uint8_t level) {
  if (type == WsIo::Ch32) {
    writeReg(CH32_ADDR, CH32_REG_PWM, level);
  } else if (type == WsIo::Tca9554) {
    uint8_t n = level ? (out | TCA_BL) : (out & ~TCA_BL);
    if (n != out) {
      out = n;
      writeOut();
    }
  }
}

void wsio_buzzer(bool on) {
  uint8_t bit = type == WsIo::Ch32 ? CH32_BEE_EN : TCA_BEE;
  uint8_t n = on ? (out | bit) : (out & ~bit);
  if (type != WsIo::None && n != out) {
    out = n;
    writeOut();
  }
}
