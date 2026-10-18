#pragma once
#include "n2k_decode.h"

// NMEA 2000 via de CAN-module (alleen luisteren: de bus wordt nooit verstoord)
void n2k_begin();
bool n2k_available();  // heeft dit board een CAN-aansluiting?
void n2k_set_enabled(bool on);
void n2k_get(N2kState &out);
void n2k_tick();  // vanuit de UI-lus: klok gelijkzetten met GPS, statistiek

struct N2kStats {
  bool running;
  uint32_t framesPerSec;
  uint8_t sources;
  uint32_t lastClockSync;  // millis() van de laatste GPS-tijdsynchronisatie, 0 = nooit
};
N2kStats n2k_stats();
