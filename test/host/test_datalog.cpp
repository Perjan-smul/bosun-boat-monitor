// Pc-tests voor de CSV-bestanden op de micro-SD-kaart (src/datalog.cpp)
#include "datalog.h"
#include "check.h"

namespace {
N2kState emptyN2k() {
  N2kDecoder dec;
  return dec.state;
}
}  // namespace

TEST(datalog_collect) {
  static DeviceState d[2];
  memset(d, 0, sizeof(d));
  strcpy(d[0].cfg.name, "Huis;accu");  // puntkomma hoort niet in een kolomkop
  d[0].cfg.type = SensorType::VictronBattery;
  d[0].hasData = true;
  d[0].lastSeenMs = 399000;
  d[0].reading.m = {12.8f, -3.5f, -44.8f, 87, NAN, NAN, NAN, NAN, NAN, 600, 0xFF, 0, 0, NAN, NAN, 0, NAN, NAN, NAN, 3, 0};
  strcpy(d[1].cfg.name, "Oud");
  d[1].hasData = true;
  d[1].lastSeenMs = 1;  // ouder dan 5 minuten
  d[1].reading.m = d[0].reading.m;
  N2kState s = emptyN2k();
  s.depth = 4.25f;
  s.depthMs = 399000;
  s.sog = 2.5f;  // m/s
  s.cog = 271;
  s.cogSogMs = 399000;
  DlValue v[DL_MAX_COLS];
  int n = datalog_collect(d, 2, s, 400000, v, DL_MAX_COLS, 1012.3f);
  // Barometer, accu (V, A, W, SoC, resterend), diepte, SOG, COG
  CHECK_EQ(n, 9);
  CHECK_STR(v[0].name, "Barometer hPa");
  CHECK_STR(v[1].name, "Huis accu V");
  CHECK_STR(v[5].name, "Huis accu resterend min");
  CHECK_STR(v[6].name, "NMEA diepte m");
  CHECK_STR(v[7].name, "NMEA SOG kn");
  CHECK_NEAR(v[7].v, 4.86, 0.01);
}

TEST(datalog_rows) {
  DataLog log;
  DlValue v[2] = {{"Accu V", 12.84f, 2}, {"Accu A", -3.5f, 1}};
  CHECK(log.addColumns(v, 2));
  CHECK(!log.addColumns(v, 2));  // niets nieuws
  char buf[256];
  log.header(buf, sizeof(buf), "Tijd");
  CHECK_STR(buf, "Tijd;Accu V;Accu A\r\n");
  log.row(buf, sizeof(buf), "12:00", v, 2, true);
  CHECK_STR(buf, "12:00;12,84;-3,5\r\n");
  // Een waarde ontbreekt: lege kolom
  log.row(buf, sizeof(buf), "12:01", v + 1, 1, false);
  CHECK_STR(buf, "12:01;;-3.5\r\n");
  // Na een herstart: kolommen uit de kop van het bestand
  DataLog again;
  again.loadHeader("Tijd;Accu V;Accu A\r\n");
  CHECK_EQ(again.columns(), 2);
  CHECK_STR(again.column(1), "Accu A");
  DlValue w = {"Tank %", 50, 0};
  CHECK(again.addColumns(&w, 1));
  again.header(buf, sizeof(buf), "Tijd");
  CHECK_STR(buf, "Tijd;Accu V;Accu A;Tank %\r\n");
  // Te kleine buffer: afgekapt, niet over de rand
  char small[12];
  again.row(small, sizeof(small), "12:02", v, 2, false);
  CHECK(strlen(small) < sizeof(small));
}
