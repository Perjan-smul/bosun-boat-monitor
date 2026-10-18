#include "log.h"
#include "buzzer.h"
#include <Arduino.h>
#include <Wire.h>
#include "board.h"
#if defined(BOARD_WAVESHARE_4)
#include "wsio.h"
#endif

namespace {
uint8_t pcfAddr = 0;  // 0 = geen PCF8574
bool present = false;

bool probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}
}  // namespace

void buzzer_begin() {
#if defined(BOARD_WAVESHARE_4)
  // Zoemer op het board, via de hulpchip. Niet naar een PCF8574 zoeken: 0x24 is de CH32V003.
  present = wsio_type() != WsIo::None;
  if (present) buzzer_set(false);
  return;
#endif
  if (BUZZER_GPIO >= 0) {
    pinMode(BUZZER_GPIO, OUTPUT);
    present = true;
  } else {
    // 0x20 is de XL9535 van de T-Panel, die slaan we over
    for (uint8_t a = 0x21; a <= 0x3F && !pcfAddr; a++) {
      if (a == 0x28) a = 0x38;
      if (probe(a)) pcfAddr = a;
    }
    present = pcfAddr != 0;
    if (present) logf("[buzzer] PCF8574 gevonden op 0x%02X\n", pcfAddr);
  }
  buzzer_set(false);
}

bool buzzer_present() { return present; }

void buzzer_set(bool on) {
#if defined(BOARD_WAVESHARE_4)
  wsio_buzzer(on);
  return;
#endif
  if (BUZZER_GPIO >= 0) {
    digitalWrite(BUZZER_GPIO, on != (bool)BUZZER_ACTIVE_LOW);
  } else if (pcfAddr) {
    Wire.beginTransmission(pcfAddr);
    // Werkt voor beide aansluitwijzen tegelijk:
    //  - LCD-backpack: P3 hoog = transistor van de achtergrondverlichting aan = zoemer aan
    //  - kale PCF8574-module: P0 laag = zoemer aan (de PCF8574 trekt de stroom naar massa)
    // Aan: P0 laag, P3 hoog (0xFE). Uit: P0 hoog, P3 laag (0xF7). Overige pinnen hoog (ingang).
    Wire.write(on ? 0xFE : 0xF7);
    Wire.endTransmission();
  }
}
