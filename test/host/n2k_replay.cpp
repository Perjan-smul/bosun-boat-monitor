// Speelt een NMEA 2000-opname af door de decoder van Bosun (src/n2k_decode.cpp) en schrijft per
// verwerkte boodschap een regel JSON, in SI-eenheden zoals canboat (analyzer -json -si).
// Daarmee vergelijkt scripts/n2k_compare.py de decoder met canboat op echte opnames.
//
// Invoer: Actisense-tekstformaat (canboat), één boodschap of één CAN-frame per regel:
//   2016-02-28T19:57:02.364Z,2,127250,7,255,8,ff,10,3b,ff,7f,ce,f5,fc
// of het formaat van Yacht Devices (YDWG/YDNR), losse CAN-frames:
//   00:29:58.618 R 09FD020D FF A4 01 51 F2 FB FF FF
// Boodschappen langer dan 8 bytes zijn al samengevoegd; korter gaan als CAN-frame door de decoder
// (dus ook het samenvoegen van fast packets wordt getest).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "n2k_decode.h"

namespace {

constexpr double DEG2RAD = M_PI / 180.0;
constexpr double KELVIN = 273.15;

// NaN wordt null, zoals canboat met -empty; zonder waarde laten we het veld weg
void num(const char *name, double v, bool &first) {
  if (isnan(v)) return;
  printf("%s\"%s\":%.12g", first ? "" : ",", name, v);
  first = false;
}

// De bijgewerkte plek (motor, tank, accu ...) is die met de tijd van deze boodschap
void dump(const N2kDecoder &dec, uint32_t pgn, uint8_t src, const char *ts, const uint8_t *d, uint32_t ms) {
  const N2kState &s = dec.state;
  printf("{\"timestamp\":\"%s\",\"pgn\":%u,\"src\":%u,\"fields\":{", ts, pgn, src);
  bool f = true;
  switch (pgn) {
    case 126992:
    case 129029:  // tijd en positie apart: één van beide kan ontbreken
      if (s.gpsTimeMs == ms) num("Epoch", s.gpsEpoch, f);
      if (pgn == 129029 && s.posMs == ms) num("Latitude", s.lat, f), num("Longitude", s.lon, f);
      break;
    case 129033: num("Local Offset", s.localOffsetMin * 60.0, f); break;
    case 130310:
    case 130311:
    case 130314: num("Pressure", s.pressureHpa * 100.0, f); break;
    case 129025: num("Latitude", s.lat, f), num("Longitude", s.lon, f); break;
    case 129026: num("COG", s.cog * DEG2RAD, f), num("SOG", s.sog, f); break;
    case 127250: num("Heading", s.heading * DEG2RAD, f); break;
    case 128259: num("Speed Water Referenced", s.stw, f); break;
    case 128267: num("Depth", s.depth, f), num("Offset", s.depthOffset, f); break;
    case 130306: {  // altijd één frame: d is de boodschap
      uint8_t ref = d[5] & 0x07;
      num("Wind Speed", ref == 2 ? s.aws : s.tws, f);
      num("Wind Angle", (ref == 2 ? s.awa : s.twa) * DEG2RAD, f);
      break;
    }
    case 127488:
    case 127489:
      for (auto &e : s.engine) {
        if ((pgn == 127488 ? e.rapidMs : e.dynMs) != ms) continue;
        if (pgn == 127488) {
          num("Speed", e.rpm / 60.0, f);  // SI: Hz
        } else {
          num("Oil pressure", e.oilBar * 1e5, f);
          num("Oil temperature", e.oilTempC + KELVIN, f);
          num("Temperature", e.coolantC + KELVIN, f);
          num("Alternator Potential", e.altV, f);
          num("Fuel Rate", e.fuelLph / 3.6e6, f);  // SI: m3/s
          num("Total Engine hours", e.hours * 3600.0, f);
          num("Percent Engine Load", e.loadPct == -128 ? NAN : e.loadPct / 100.0, f);
        }
      }
      break;
    case 127505:
      for (auto &t : s.tanks)
        if (t.ms == ms)
          num("Level", t.levelPct / 100.0, f), num("Capacity", t.capacityL / 1000.0, f);
      break;
    case 130312:
    case 130316:
      for (auto &t : s.temps)
        if (t.ms == ms)
          num(pgn == 130312 ? "Actual Temperature" : "Temperature", t.c + KELVIN, f);
      break;
    case 130313:
      for (auto &h : s.humids)
        if (h.ms == ms) num("Actual Humidity", h.pct / 100.0, f);
      break;
    case 127508:
    case 127506:
      for (auto &b : s.batts) {
        if ((pgn == 127508 ? b.ms : b.detailMs) != ms) continue;
        if (pgn == 127508) {
          num("Voltage", b.volts, f), num("Current", b.amps, f);
          num("Temperature", b.tempC + KELVIN, f);
        } else {
          num("State of Charge", b.soc / 100.0, f);
          num("Time Remaining", b.ttgMin < 0 ? NAN : b.ttgMin * 60.0, f);
        }
      }
      break;
    case 127245: num("Position", s.rudder * DEG2RAD, f); break;
    case 127257: num("Pitch", s.pitch * DEG2RAD, f), num("Roll", s.roll * DEG2RAD, f); break;
  }
  printf("}}\n");
}

}  // namespace

int main(int argc, char **argv) {
  FILE *in = argc > 1 ? fopen(argv[1], "r") : stdin;
  if (!in) {
    perror(argv[1]);
    return 1;
  }
  N2kDecoder dec;
  char line[2048];
  uint32_t ms = 0;
  while (fgets(line, sizeof(line), in)) {
    if (line[0] == '#') continue;
    char ts[64];
    unsigned id;
    int pos = 0;
    // Yacht Devices-formaat: losse CAN-frames, ook van fast packets (test het samenvoegen)
    if (sscanf(line, "%63s R %x%n", ts, &id, &pos) == 2 && strchr(ts, ':')) {
      uint8_t d[8];
      unsigned n = 0, b;
      int k = 0;
      for (const char *p = line + pos; n < 8 && sscanf(p, " %2x%n", &b, &k) == 1; p += k) d[n++] = b;
      ms += 10;
      uint32_t before = dec.decoded;
      dec.onFrame(id, d, n, ms);
      if (dec.decoded != before) dump(dec, dec.lastDecoded, id & 0xFF, ts, d, ms);
      continue;
    }
    if (!strchr(line, ',')) continue;
    unsigned prio, pgn, src, dst, len;
    if (sscanf(line, "%63[^,],%u,%u,%u,%u,%u%n", ts, &prio, &pgn, &src, &dst, &len, &pos) < 6) continue;
    uint8_t d[256];
    unsigned n = 0;
    const char *p = line + pos;
    while (n < len && n < sizeof(d) && *p == ',') {
      unsigned b;
      int k = 0;
      if (sscanf(p, ",%2x%n", &b, &k) < 1) break;
      d[n++] = b;
      p += k;
    }
    if (n != len) continue;
    ms += 10;  // opnames lopen vaak niet gelijk op; een vaste stap is genoeg voor de decoder
    uint32_t before = dec.decoded;
    if (len > 8) {
      dec.onMessage(pgn, src, d, len, ms);
    } else {
      uint32_t id = (prio & 7) << 26 | pgn << 8 | src;
      if (((pgn >> 8) & 0xFF) < 240) id |= (dst & 0xFF) << 8;
      dec.onFrame(id, d, len, ms);
    }
    if (dec.decoded != before) dump(dec, dec.lastDecoded, src, ts, d, ms);
  }
  return 0;
}
