#pragma once
#include <stdint.h>

// NMEA 2000-decoder (alleen luisteren). Los van de CAN-hardware, zodat hij op de pc te testen is.
// Veldindeling volgens canboat (github.com/canboat/canboat).

constexpr int N2K_ENGINES = 2;
constexpr int N2K_TANKS = 12;
constexpr int N2K_TEMPS = 16;
constexpr int N2K_BATTS = 6;
constexpr int N2K_HUMIDS = 4;

struct N2kEngine {
  uint32_t rapidMs, dynMs;  // 0 = nooit ontvangen
  uint8_t instance;         // motornummer op de bus (0 = enkel/bakboord, 1 = stuurboord; soms anders)
  float rpm;
  float coolantC, oilTempC, oilBar, altV, fuelLph, hours;
  int8_t loadPct;  // -128 = onbekend
};

struct N2kTank {
  uint32_t ms;
  uint8_t type, instance;  // type: 0 brandstof, 1 water, 2 grijs water, 3 levend aas, 4 olie, 5 zwart water
  float levelPct, capacityL;
};

struct N2kTemp {
  uint32_t ms;
  uint8_t source, instance;  // source: 0 zeewater, 1 buiten, 2 binnen, 3 machinekamer, 4 kajuit, 7 koelkast, 13 vriezer ...
  float c;
};

struct N2kBattery {
  uint32_t ms, detailMs;  // 127508 (spanning/stroom/temperatuur), 127506 (SoC/resterende tijd); 0 = nooit
  uint8_t instance;
  float volts, amps, tempC;  // NAN = onbekend; stroom: + laden, - ontladen
  float soc;                 // %, NAN = onbekend
  int32_t ttgMin;            // resterende tijd, -1 = onbekend
};

struct N2kHumidity {
  uint32_t ms;
  uint8_t source, instance;  // source: 0 binnen, 1 buiten
  float pct;
};

struct N2kState {
  // Navigatie (NAN = onbekend); hoeken in graden, snelheden in m/s
  double lat, lon;
  uint32_t posMs;
  float sog, cog;
  uint8_t cogRef;  // 0 waar, 1 magnetisch
  uint32_t cogSogMs;
  float heading;
  uint8_t headingRef;
  uint32_t headingMs;
  float stw;
  uint32_t stwMs;
  float depth, depthOffset;  // offset: + = tot waterlijn, - = tot kiel
  uint32_t depthMs;
  float aws, awa;  // schijnbare wind, hoek 0-360 t.o.v. boeg
  uint32_t appWindMs;
  float tws, twa;  // ware wind; twa t.o.v. boeg (ref 3/4) of richting t.o.v. noord (ref 0/1)
  uint8_t trueRef;
  uint32_t trueWindMs;
  // Tijd van de GPS (UTC)
  uint32_t gpsEpoch, gpsTimeMs;
  // Lokaal verschil met UTC in minuten (PGN 129033, meestal van de kaartplotter); 0 ms = niet ontvangen
  int16_t localOffsetMin;
  uint8_t localOffsetSrc;
  uint32_t localOffsetMs;
  // Luchtdruk (hPa): PGN 130314 (fijn, 0,1 Pa) gaat voor 130311/130310 (in hele hPa)
  float pressureHpa;
  uint32_t pressureMs;
  uint32_t pressurePgn;
  // Roerstand (graden, + = stuurboord) en scheepsstand (helling + = over stuurboord, stampen + = boeg omhoog)
  float rudder;
  uint32_t rudderMs;
  uint8_t rudderInstance;
  float roll, pitch;
  uint32_t attitudeMs;
  N2kBattery batts[N2K_BATTS];
  N2kHumidity humids[N2K_HUMIDS];
  N2kEngine engine[N2K_ENGINES];
  N2kTank tanks[N2K_TANKS];
  N2kTemp temps[N2K_TEMPS];
  // Busstatistiek
  uint32_t frames;
  uint8_t sources;  // aantal verschillende afzenders
};

class N2kDecoder {
 public:
  N2kDecoder();
  void reset();
  // Eén CAN-frame met 29-bits id
  void onFrame(uint32_t id, const uint8_t *data, uint8_t len, uint32_t nowMs);
  // Complete NMEA 2000-boodschap (na samenvoegen van fast packets)
  void onMessage(uint32_t pgn, uint8_t src, const uint8_t *d, int len, uint32_t nowMs);
  N2kState state;
  uint32_t lastDecoded = 0;  // PGN van de laatst verwerkte boodschap (voor tests en statistiek)
  uint32_t decoded = 0;      // aantal verwerkte boodschappen

 private:
  N2kEngine *engineSlot(uint8_t instance, uint32_t nowMs);
  N2kBattery *battSlot(uint8_t instance, uint32_t nowMs);
  struct FastPacket {
    uint32_t pgn, ms;
    uint8_t src, seq, next, total;
    uint16_t received;
    bool used;
    uint8_t buf[64];  // genoeg voor de velden die wij gebruiken
  } fp_[8];
  uint8_t seenSrc_[32];  // bitmap van 256 afzenders
  int16_t offsetCand_;   // lokaal verschil: kandidaat, pas na twee gelijke berichten na elkaar overgenomen
  uint8_t offsetCandN_;
  void onFastFrame(uint32_t pgn, uint8_t src, const uint8_t *d, uint8_t len, uint32_t nowMs);
};

bool n2kIsFastPacket(uint32_t pgn);
uint32_t n2kPgnFromId(uint32_t id);

// Namen voor op het scherm en in meldingen
const char *n2kTankTypeName(uint8_t type);
void n2kTankLabel(const N2kState &s, const N2kTank &t, char *buf, unsigned len);       // "Diesel" of "Diesel 2"
void n2kEngineLabel(const N2kState &s, const N2kEngine &e, char *buf, unsigned len);   // "Motor", "Bakboordmotor" ...
