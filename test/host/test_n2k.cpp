// Pc-tests voor de NMEA 2000-decoder (src/n2k_decode.cpp).
// Boodschappen uit echte opnames in canboat/samples (github.com/canboat/canboat, Apache 2.0);
// de verwachte waarden zijn die van canboat's analyzer. Met scripts/n2k_compare.py is de decoder
// op alle opnames van canboat (meer dan een miljoen boodschappen) met canboat vergeleken.
#include "n2k_decode.h"
#include "check.h"

namespace {

uint32_t nowMs = 0;

// Eén regel in het Actisense-tekstformaat door de decoder, zoals op het board: als CAN-frames.
// Langer dan 8 bytes = fast packet: in frames gesplitst, zodat ook het samenvoegen getest wordt.
void feed(N2kDecoder &dec, const char *line) {
  unsigned prio, pgn, src, dst, len;
  int pos = 0;
  char ts[64];
  CHECK(sscanf(line, "%63[^,],%u,%u,%u,%u,%u%n", ts, &prio, &pgn, &src, &dst, &len, &pos) == 6);
  std::vector<uint8_t> d;
  unsigned b;
  int k;
  for (const char *p = line + pos; sscanf(p, ",%2x%n", &b, &k) == 1; p += k) d.push_back(b);
  CHECK_EQ(d.size(), len);
  uint32_t id = (prio & 7) << 26 | pgn << 8 | src;
  nowMs += 100;
  if (len <= 8) {
    dec.onFrame(id, d.data(), len, nowMs);
    return;
  }
  CHECK(n2kIsFastPacket(pgn));
  static uint8_t seq = 0;
  seq = (seq + 1) & 7;
  size_t i = 0;
  for (uint8_t frame = 0; i < d.size(); frame++) {
    uint8_t f[8];
    memset(f, 0xFF, sizeof(f));
    f[0] = seq << 5 | frame;
    int n = 1;
    if (frame == 0) f[n++] = len;
    while (n < 8 && i < d.size()) f[n++] = d[i++];
    dec.onFrame(id, f, 8, nowMs);
  }
}

int tankCount(const N2kState &s) {
  int n = 0;
  for (auto &t : s.tanks) n += t.ms != 0;
  return n;
}

int tempCount(const N2kState &s) {
  int n = 0;
  for (auto &t : s.temps) n += t.ms != 0;
  return n;
}

}  // namespace

TEST(n2k_engine) {
  N2kDecoder dec;
  feed(dec, "2026-07-10T06:27:57.534Z,2,127488,236,255,8,00,3e,20,8c,00,7f,ff,ff");
  CHECK_NEAR(dec.state.engine[0].rpm, 2063.5, 0.001);
  // Fast packet (26 bytes)
  feed(dec, "2016-04-09T16:41:39.628Z,2,127489,16,255,26,00,2f,06,ff,ff,e3,73,65,05,ff,7f,72,10,00,00,"
            "ff,ff,ff,ff,ff,06,00,00,00,7f,7f");
  CHECK_EQ(dec.lastDecoded, 127489);
  const N2kEngine &e = dec.state.engine[0];
  CHECK_EQ(e.instance, 0);
  CHECK_NEAR(e.oilBar, 1.583, 0.0001);
  CHECK_NEAR(e.coolantC, 23.52, 0.01);
  CHECK_NEAR(e.altV, 13.81, 0.001);
  CHECK_NEAR(e.hours * 3600, 4210, 0.5);
  CHECK(isnan(e.oilTempC));
  CHECK(isnan(e.fuelLph));
  CHECK_EQ(e.loadPct, -128);
}

TEST(n2k_gnss_position) {
  N2kDecoder dec;
  // Fast packet van 47 bytes (7 frames)
  feed(dec, "2026-07-10T06:27:57.879Z,3,129029,41,255,47,48,a4,50,d0,c9,df,0d,c0,02,05,f6,6c,eb,74,04,00,a4,65,"
            "ae,99,a9,27,13,20,aa,44,00,00,00,00,00,13,fc,0e,34,00,5e,00,1e,0f,00,00,00,ff,ff,ff,ff");
  CHECK_EQ(dec.lastDecoded, 129029);
  CHECK_EQ(dec.state.gpsEpoch, 20644UL * 86400 + 23277);  // 2026-07-10 06:27:57 UTC
  CHECK_NEAR(dec.state.lat, 32.1140327, 1e-7);
  CHECK_NEAR(dec.state.lon, 138.0258288, 1e-7);
  // Een gemiste frame: de boodschap wordt weggegooid, de positie blijft
  N2kDecoder dec2;
  uint32_t id = 3 << 26 | 129029 << 8 | 41;
  uint8_t f0[8] = {0x20, 47, 0x48, 0xa4, 0x50, 0xd0, 0xc9, 0xdf};
  uint8_t f2[8] = {0x22, 0, 0, 0, 0, 0, 0, 0};
  dec2.onFrame(id, f0, 8, 1);
  dec2.onFrame(id, f2, 8, 2);
  CHECK_EQ(dec2.decoded, 0);
  CHECK(isnan(dec2.state.lat));
}

TEST(n2k_position_rapid) {
  N2kDecoder dec;
  feed(dec, "2020-08-22T13:53:02.193Z,2,129025,127,255,8,b0,dd,b1,1f,1b,2f,3d,03");
  CHECK_NEAR(dec.state.lat, 53.1750320, 1e-7);
  CHECK_NEAR(dec.state.lon, 5.4341403, 1e-7);
  // Verminkte boodschap uit een echte opname: lengte -198 graden. Niet overnemen.
  feed(dec, "2011-04-25-10:08:17.065,2,129025,8,255,8,a0,1b,b7,13,73,1f,fe,89");
  CHECK_NEAR(dec.state.lat, 53.1750320, 1e-7);
  CHECK_NEAR(dec.state.lon, 5.4341403, 1e-7);
}

TEST(n2k_time) {
  N2kDecoder dec;
  feed(dec, "2016-04-09T16:41:39.060Z,3,126992,0,255,8,57,f0,04,42,00,ec,d1,23");
  CHECK_EQ(dec.state.gpsEpoch, 16900UL * 86400 + 60096);  // 2016-04-09 16:41:36 UTC
}

TEST(n2k_temperature) {
  N2kDecoder dec;
  feed(dec, "2021-01-27T23:34:19.126Z,5,130316,1,255,8,ff,00,01,44,8a,04,ff,ff");
  CHECK_EQ(tempCount(dec.state), 1);
  CHECK_EQ(dec.state.temps[0].source, 1);  // buiten
  CHECK_NEAR(dec.state.temps[0].c, 24.39, 0.001);
  // NAC-3 stuurautomaat zonder watertemperatuursensor: 0 K. Niet tonen als -273 °C.
  feed(dec, "2026-06-08T02:27:14.668,5,130316,35,255,8,ff,00,00,00,00,00,ff,ff");
  CHECK_EQ(tempCount(dec.state), 1);
}

TEST(n2k_tank) {
  N2kDecoder dec;
  // Vuilwatertank zonder niveau (0x7FFF): niet tonen
  feed(dec, "2016-02-28T19:57:02.364Z,6,127505,114,255,8,50,ff,7f,be,11,00,00,ff");
  CHECK_EQ(tankCount(dec.state), 0);
  feed(dec, "2016-02-28-19:56:58,6,127505,43,255,8,09,a8,61,3c,fd,00,00,ff");
  CHECK_EQ(tankCount(dec.state), 1);
  CHECK_EQ(dec.state.tanks[0].type, 0);
  CHECK_EQ(dec.state.tanks[0].instance, 9);
  CHECK_NEAR(dec.state.tanks[0].levelPct, 100, 0.001);
  CHECK_NEAR(dec.state.tanks[0].capacityL, 6482.8, 0.01);
}

TEST(n2k_battery) {
  N2kDecoder dec;
  // Temperatuur 0 K: niet gemeten
  feed(dec, "2016-02-28T19:57:02.824Z,6,127508,4,255,8,0b,46,0a,00,00,00,00,00");
  const N2kBattery &b = dec.state.batts[0];
  CHECK_EQ(b.instance, 11);
  CHECK_NEAR(b.volts, 26.30, 0.001);
  CHECK_NEAR(b.amps, 0, 0.001);
  CHECK(isnan(b.tempC));
  // DC-status (fast packet, 9 bytes)
  feed(dec, "2016-02-28-19:58:05,6,127506,40,255,9,00,0a,00,64,00,80,05,ff,ff");
  const N2kBattery &d = dec.state.batts[1];
  CHECK_EQ(d.instance, 10);
  CHECK_NEAR(d.soc, 100, 0.001);
  CHECK_EQ(d.ttgMin, 84480 / 60);
}

TEST(n2k_navigation) {
  N2kDecoder dec;
  feed(dec, "2018-02-14T16:32:47.773Z,2,130306,3,255,8,d3,1f,00,21,9d,fa,ff,ff");
  CHECK_NEAR(dec.state.aws, 0.31, 0.001);
  CHECK_NEAR(dec.state.awa, 230.5, 0.05);  // canboat toont op 0,1 graad
  feed(dec, "2011-04-25-10:16:42.554,3,128267,4,255,8,25,1b,01,00,00,00,00,ff");
  CHECK_NEAR(dec.state.depth, 2.83, 0.001);
  CHECK_NEAR(dec.state.depthOffset, 0, 0.001);
  feed(dec, "2016-02-28T19:57:03.053Z,5,130313,130,255,8,17,00,01,b5,3a,ff,7f,ff");
  CHECK_NEAR(dec.state.humids[0].pct, 60.116, 0.001);
  feed(dec, "2024-02-17T05:15:49.768Z,2,127245,204,255,8,00,ff,23,0a,fa,09,ff,ff");
  CHECK_NEAR(dec.state.rudder, 14.6, 0.05);
}
