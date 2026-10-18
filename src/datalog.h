#pragma once
#include <stddef.h>
#include <math.h>
#include <stdint.h>
#include "n2k_decode.h"
#include "store.h"

// Metingen als CSV (voor de SD-kaart): één regel per minuut, een kolom per meetwaarde.
// Los van de kaart zelf, zodat het op de pc te testen is.

constexpr int DL_MAX_COLS = 120;
constexpr int DL_NAME_LEN = 48;

struct DlValue {
  char name[DL_NAME_LEN];  // kolomkop, bv. "Accu SoC %"
  float v;
  uint8_t decimals;
};

// Alle actuele waarden (alleen verse metingen: Bluetooth < 5 min, NMEA 2000 < 30 s)
// baroHpa: luchtdruk van de barometer (gekozen bron), NAN = geen
int datalog_collect(const DeviceState *devs, int nDevs, const N2kState &n2k, uint32_t nowMs, DlValue *out, int max,
                    float baroHpa = NAN);

// Kolommen van het huidige bestand. Een kolom blijft de hele dag bestaan (leeg als er even niets is);
// komt er een nieuwe bij, dan begint een nieuw bestand met de nieuwe kop.
class DataLog {
 public:
  void reset() { count_ = 0; }
  // Kolommen overnemen uit de kop van een bestaand bestand (na een herstart dezelfde dag)
  void loadHeader(const char *line);
  // true als er een kolom bijkomt (de kolommen zijn dan al uitgebreid: nieuw bestand met nieuwe kop)
  bool addColumns(const DlValue *vals, int n);
  void header(char *out, size_t len, const char *timeTitle) const;
  // decimalComma: "3,25" in plaats van "3.25" (voor Excel in de meeste talen)
  void row(char *out, size_t len, const char *timeText, const DlValue *vals, int n, bool decimalComma) const;
  int columns() const { return count_; }
  const char *column(int i) const { return names_[i]; }

 private:
  int find(const char *name) const;
  char names_[DL_MAX_COLS][DL_NAME_LEN];
  int count_ = 0;
};
