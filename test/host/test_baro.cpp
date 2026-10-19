// Pc-tests voor de barometer: tendens (stormwaarschuwing) en de historie van 48 uur (src/baro.cpp)
#include "baro.h"
#include "check.h"

namespace {
constexpr uint32_t T0 = 1780000200;  // begin van een tijdvak van 10 minuten (deelbaar door 600)

// Elke minuut een meting, van t0 tot t1 (exclusief), lineair van p0 naar p1 hPa
void feed(BaroHistory &h, uint32_t t0, uint32_t t1, float p0, float p1) {
  for (uint32_t t = t0; t < t1; t += 60) h.add(t, p0 + (p1 - p0) * (t - t0) / (float)(t1 - t0));
}
}  // namespace

TEST(baro_tendency) {
  CHECK_EQ((int)baro_tendency(NAN), (int)BaroTendency::Unknown);
  CHECK_EQ((int)baro_tendency(0.4f), (int)BaroTendency::Steady);
  CHECK_EQ((int)baro_tendency(-0.4f), (int)BaroTendency::Steady);
  CHECK_EQ((int)baro_tendency(1.0f), (int)BaroTendency::RisingSlow);
  CHECK_EQ((int)baro_tendency(-1.0f), (int)BaroTendency::FallingSlow);
  CHECK_EQ((int)baro_tendency(-2.0f), (int)BaroTendency::Falling);
  CHECK_EQ((int)baro_tendency(-4.0f), (int)BaroTendency::FallingFast);
  CHECK_EQ((int)baro_tendency(-6.5f), (int)BaroTendency::FallingVeryFast);
  CHECK_EQ((int)baro_tendency(6.5f), (int)BaroTendency::RisingVeryFast);
}

TEST(baro_history_trend) {
  BaroHistory h;
  h.clear();
  CHECK(isnan(h.current()));
  CHECK(isnan(h.diff3h()));
  // 2 uur stabiel: nog geen trend
  feed(h, T0, T0 + 7200, 1013, 1013);
  CHECK_NEAR(h.current(), 1013, 0.01);
  CHECK(isnan(h.diff3h()));
  // Daarna 3 uur lang 6 hPa dalen: na 3 uur vanaf het begin is er een trend
  feed(h, T0 + 7200, T0 + 7200 + 3 * 3600, 1013, 1007);
  float d = h.diff3h();
  CHECK(!isnan(d));
  CHECK(d < -5 && d > -6.5);
  CHECK_EQ((int)baro_tendency(d), (int)BaroTendency::FallingFast);
}

TEST(baro_history_series) {
  BaroHistory h;
  h.clear();
  feed(h, T0, T0 + 1800, 1000, 1000);  // 3 tijdvakken
  int16_t s[BARO_N];
  CHECK_EQ(h.series(s, BARO_N), BARO_N);
  CHECK_EQ(s[BARO_N - 1], 10000);  // lopend tijdvak: 1000,0 hPa x10
  CHECK_EQ(s[BARO_N - 2], 10000);
  CHECK_EQ(s[BARO_N - 3], 10000);
  CHECK_EQ(s[BARO_N - 4], BARO_NONE);  // daarvoor: niets
  // Een uur geen metingen: 6 lege tijdvakken (5 + het onvolledige), dan weer metingen
  feed(h, T0 + 1800 + 3600, T0 + 1800 + 3600 + 600, 1002, 1002);
  CHECK_EQ(h.series(s, BARO_N), BARO_N);
  CHECK_EQ(s[BARO_N - 1], 10020);
  CHECK_EQ(s[BARO_N - 2], BARO_NONE);
  CHECK_EQ(s[BARO_N - 7], BARO_NONE);
  CHECK_EQ(s[BARO_N - 8], 10000);
}

TEST(baro_history_clock) {
  BaroHistory h;
  h.clear();
  feed(h, T0, T0 + 3600, 1010, 1010);
  // Klok een dag terug gezet: opnieuw beginnen
  h.add(T0 - 86400, 1005);
  int16_t s[BARO_N];
  h.series(s, BARO_N);
  CHECK_EQ(s[BARO_N - 2], BARO_NONE);
  CHECK_NEAR(h.current(), 1005, 0.01);
  // Ongeldige meting
  h.add(T0 - 86400 + 60, NAN);
  CHECK_NEAR(h.current(), 1005, 0.01);
}

TEST(baro_history_save_load) {
  BaroHistory h;
  h.clear();
  feed(h, T0, T0 + 3 * 3600 + 600, 1020, 1017);
  static BaroHistory::Saved saved;
  h.save(saved, 2);
  CHECK_EQ(saved.source, 2);
  BaroHistory g;
  g.clear();
  CHECK(g.load(saved));
  int16_t a[BARO_N], b[BARO_N];
  h.series(a, BARO_N);
  g.series(b, BARO_N);
  // Het lopende tijdvak wordt niet bewaard; de rest is gelijk
  for (int i = 0; i < BARO_N - 1; i++) CHECK_EQ(a[i], b[i]);
  CHECK_EQ(g.lastSlot(), h.lastSlot());
  saved.magic = 0;
  CHECK(!g.load(saved));
}
