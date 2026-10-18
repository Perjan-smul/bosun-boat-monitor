#pragma once
#include <stddef.h>
#include <stdint.h>

// Barometer: luchtdruk, het verloop over 48 uur en de trend over 3 uur (stormwaarschuwing).
// Bronnen, de eerste die er is (en daarna blijft hij die zolang hij meet):
//   1. BME280/BMP280 op de I2C-aansluiting
//   2. NMEA 2000 (PGN 130314, 130311 of 130310), bv. een weerstation
//   3. een Bluetooth-sensor met luchtdruk (Ruuvi, BTHome)

enum class BaroSource : uint8_t { None, Bme280, N2k, Ble };

// Tendens volgens de WMO-indeling (verschil in hPa over 3 uur)
enum class BaroTendency : uint8_t { Unknown, Steady, RisingSlow, Rising, RisingFast, RisingVeryFast,
                                    FallingSlow, Falling, FallingFast, FallingVeryFast };
BaroTendency baro_tendency(float diff3h);
const char *baro_tendency_text(BaroTendency t);  // Nederlandse sleutel, tonen met T()

constexpr int BARO_N = 288;         // 48 uur
constexpr uint32_t BARO_SLOT_S = 600;  // per 10 minuten
constexpr int16_t BARO_NONE = INT16_MIN;

// Historie per tijdvak van 10 minuten (los van de hardware, te testen op de pc)
class BaroHistory {
 public:
  void clear();
  // Meting op tijdstip epoch (UTC, s). Het gemiddelde per tijdvak komt in de reeks zodra het volgende begint.
  void add(uint32_t epoch, float hPa);
  float current() const;                     // laatste waarde (lopend gemiddelde), NAN = geen
  float diff3h() const;                      // nu min 3 uur geleden, NAN = nog geen 3 uur gegevens
  int series(int16_t *out, int max) const;   // hPa x10, oudste eerst; BARO_NONE = geen meting
  uint32_t lastSlot() const { return slot_; }
  // Bewaren in NVS: alleen de vaste reeks, niet het lopende gemiddelde
  struct Saved {
    uint32_t magic, slot;
    uint8_t source;
    int16_t v[BARO_N];
  };
  void save(Saved &out, uint8_t source) const;
  bool load(const Saved &in);

 private:
  int16_t at(int ago) const;  // waarde van 'ago' tijdvakken terug (0 = het laatst afgesloten)
  int16_t v_[BARO_N];
  int head_ = 0;          // positie van het laatst afgesloten tijdvak
  uint32_t slot_ = 0;     // nummer van het lopende tijdvak (epoch / 600), 0 = nog niets
  double sum_ = 0;
  int cnt_ = 0;
};

void baro_begin();  // na Wire.begin()
void baro_tick();   // vaak aanroepen vanuit de UI-lus

struct BaroInfo {
  BaroSource source;
  char sourceName[28];  // "BME280", "NMEA 2000" of de naam van de Bluetooth-sensor
  float hPa;            // NAN = geen meting
  float diff3h;         // NAN = nog geen 3 uur gegevens
  BaroTendency tendency;
};
BaroInfo baro_info();
bool baro_available();  // deze start ooit een meting, of een bewaarde historie (dan staat de pagina er)
int baro_series(int16_t *out, int max);  // 48 uur, oudste eerst (hPa x10)
uint32_t baro_version();                 // verandert bij elk nieuw tijdvak (grafiek bijwerken)
