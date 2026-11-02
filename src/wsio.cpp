#include "board.h"
#include "wsio.h"
#include <Arduino.h>
#include <Wire.h>
#include "log.h"

// Registers en pinnen zoals in Waveshare's eigen voorbeelden (WS_CH32_IO en IOExpander_general_example)
namespace {

constexpr uint8_t CH32_ADDR = 0x24;
constexpr uint8_t CH32_REG_DIR = 0x02, CH32_REG_OUT = 0x03, CH32_REG_IN = 0x04, CH32_REG_PWM = 0x05;
constexpr uint8_t CH32_TP_RST = 1 << 1, CH32_TP_INT = 1 << 2, CH32_LCD_RST = 1 << 3, CH32_SYS_EN = 1 << 5,
                  CH32_BEE_EN = 1 << 6;
// Richting: 1 = uitgang. Waveshare zet alle pinnen op uitgang (0xFF), ook TP_INT.
// De achtergrondverlichting van V4 is omgekeerd: PWM 0 = vol aan, 255 = uit. Boven 247 laat
// Waveshare's schakeling hem flikkeren (zie de ESPHome-driver waveshare_io_ch32v003).
constexpr uint8_t CH32_PWM_OFF = 247;

constexpr uint8_t TCA_ADDR = 0x20;
constexpr uint8_t TCA_REG_IN = 0x00, TCA_REG_OUT = 0x01, TCA_REG_CFG = 0x03;  // configuratie: 1 = ingang
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

// Registeradres en lezen in één transactie (herhaalde start, geen stop ertussen), zoals Waveshare's
// WS_CH32_IO. De CH32 is een microcontroller met eigen firmware. Met een stop ertussen las 1.0.6 de
// richting als 0x2A terug (gelijk aan de laatst geschreven uitgangen, zonder de zoemerpin): waarschijnlijk
// ziet de CH32 een losse schrijfactie met alleen het registeradres als schrijfopdracht met een oude
// waarde. Dan is de zoemerpin een ingang en piept hij nooit.
bool readReg(uint8_t addr, uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return false;
  val = Wire.read();
  return true;
}

void writeOut() {
  if (type == WsIo::Ch32) {
    // Richting elke keer opnieuw: alle pinnen uitgang (zoals Waveshare). Kost één I2C-schrijfactie en
    // herstelt het als de CH32 de richting toch kwijt is.
    writeReg(CH32_ADDR, CH32_REG_DIR, 0xFF);
    writeReg(CH32_ADDR, CH32_REG_OUT, out);
  } else if (type == WsIo::Tca9554) {
    writeReg(TCA_ADDR, TCA_REG_OUT, out);
  }
}

}  // namespace

WsIo wsio_begin() {
  // Bij koud opstarten is de hulpchip (CH32V003, een eigen microcontroller) soms later klaar dan de
  // ESP32. Niet gevonden = scherm blijft in reset en de zoemer blijft aan (begintoestand van de CH32),
  // dus tot 3 s blijven zoeken.
  type = WsIo::None;
  uint32_t t0 = millis();
  int tries = 0;
  while (millis() - t0 < 3000) {
    tries++;
    if (probe(CH32_ADDR)) { type = WsIo::Ch32; break; }
    if (probe(TCA_ADDR)) { type = WsIo::Tca9554; break; }
    delay(50);
  }
  if (tries > 1) logf("[board] hulpchip na %d pogingen (%lu ms)\n", tries, (unsigned long)(millis() - t0));

  if (type == WsIo::Ch32) {
    // Precies zoals Waveshare's eigen Arduino-voorbeelden voor dit bord (WS_CH32_IO.cpp,
    // initDisplayPower): alle pinnen uitgang en laag (ook SYS_EN en TP_INT), 200 ms,
    // dan voeding en resets hoog; TP_INT blijft laag (GT911 op 0x5D), 200 ms opstarttijd.
    // Tot 3 keer, met controle: lees terug of richting en uitgangen echt zijn overgenomen
    uint8_t dir = 0, o = 0;
    for (int attempt = 0; attempt < 3; attempt++) {
      writeReg(CH32_ADDR, CH32_REG_PWM, CH32_PWM_OFF);  // backlight uit tot het scherm draait
      out = 0;
      writeOut();  // schrijft ook de richting
      delay(200);
      out = CH32_SYS_EN | CH32_LCD_RST | CH32_TP_RST;
      writeOut();
      delay(200);
      if (readReg(CH32_ADDR, CH32_REG_DIR, dir) && readReg(CH32_ADDR, CH32_REG_OUT, o) && dir == 0xFF && o == out)
        break;
      logf("[board] CH32 niet goed ingesteld (richting 0x%02X, uitgangen 0x%02X), opnieuw\n", dir, o);
    }
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

void wsio_reset_panel(uint16_t low_ms) {
  if (type == WsIo::Ch32) {
    out = 0;
    writeOut();
    delay(low_ms);
    out = CH32_SYS_EN | CH32_LCD_RST | CH32_TP_RST;
    writeOut();
    delay(200);
  } else if (type == WsIo::Tca9554) {
    out &= ~(TCA_TP_RST | TCA_LCD_RST);
    writeOut();
    delay(low_ms);
    out |= TCA_TP_RST | TCA_LCD_RST;
    writeOut();
    delay(200);
  }
}

void wsio_backlight(uint8_t level) {
  if (type == WsIo::Ch32) {
    // level 0 = uit .. 255 = vol aan; omzetten naar de omgekeerde PWM van V4
    writeReg(CH32_ADDR, CH32_REG_PWM, (uint8_t)(CH32_PWM_OFF - (uint16_t)level * CH32_PWM_OFF / 255));
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
    // Diagnose: de eerste keren loggen wat er in de hulpchip staat
    static uint8_t logged = 0;
    if (logged < 6) {
      logged++;
      delay(2);  // de CH32 verwerkt de opdracht in zijn eigen lus
      uint8_t dir = 0, o = 0, in = 0;
      bool ok;
      if (type == WsIo::Ch32) {
        ok = readReg(CH32_ADDR, CH32_REG_DIR, dir) && readReg(CH32_ADDR, CH32_REG_OUT, o) &&
             readReg(CH32_ADDR, CH32_REG_IN, in);
      } else {
        ok = readReg(TCA_ADDR, TCA_REG_CFG, dir) && readReg(TCA_ADDR, TCA_REG_OUT, o) &&
             readReg(TCA_ADDR, TCA_REG_IN, in);
      }
      logf("[zoemer] %s: uitgangen 0x%02X geschreven, gelezen 0x%02X, richting 0x%02X, pinnen 0x%02X%s\n",
           on ? "aan" : "uit", out, o, dir, in, ok ? "" : " (lezen mislukt)");
    }
  }
}
