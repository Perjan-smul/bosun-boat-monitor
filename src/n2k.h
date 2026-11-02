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
  // Diagnose van de bus
  uint32_t rawPerSec;     // alle ontvangen berichten per seconde, ook niet-NMEA 2000
  uint32_t otherPerSec;   // daarvan 11-bits (geen NMEA 2000)
  uint32_t errorsPerSec;  // busfouten per seconde (bits die niet kloppen)
  uint32_t busErrors;     // busfouten sinds het starten
  uint32_t rxErrors;      // foutenteller van de CAN-module (0-255)
  uint32_t rxMissed;      // berichten kwijt omdat de wachtrij vol was
  uint8_t rxLowPct;       // hoe vaak de RX-lijn laag (dominant) stond, in %
};
N2kStats n2k_stats();

// Wat er waarschijnlijk aan de hand is
enum class N2kDiag : uint8_t {
  Off,         // uitgeschakeld
  Ok,          // NMEA 2000-berichten komen binnen
  Silent,      // niets: geen signaal op de draden
  Errors,      // wel signaal, maar onleesbaar
  NotN2k,      // CAN-verkeer, maar geen NMEA 2000 (11-bits id's)
  RxStuckLow,  // RX-lijn steeds laag
};
N2kDiag n2k_diag(const N2kStats &st);
