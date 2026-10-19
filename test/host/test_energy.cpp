// Pc-tests voor het optellen van de energiestroom (src/energy.cpp): overzicht en historie
#include "energy.h"
#include "check.h"

namespace {
DeviceState dev(SensorType t) {
  DeviceState d;
  memset(&d, 0, sizeof(d));
  d.cfg.type = t;
  d.hasData = true;
  d.lastSeenMs = fakeMillis;
  d.reading.m = {NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, -1, 0xFF, 0, 0, NAN, NAN, 0, NAN, NAN, NAN, 3, 0};
  return d;
}
}  // namespace

TEST(energy_solar_and_shunt) {
  fakeMillis = 1000000;
  DeviceState d[2] = {dev(SensorType::VictronSolar), dev(SensorType::VictronBattery)};
  d[0].reading.m.pvWatts = 200;
  d[0].reading.m.watts = 190;  // naar de accu
  d[0].reading.m.volts = 13.5f;
  d[0].reading.m.state = 3;  // bulk
  d[1].reading.m.watts = 120;
  d[1].reading.m.amps = 8.9f;
  d[1].reading.m.volts = 13.5f;
  d[1].reading.m.soc = 80;
  EnergySummary e = energy_summarize(d, 2);
  CHECK(e.haveSolar && e.haveShunt && !e.haveShore);
  CHECK_NEAR(e.pv, 200, 0.01);
  CHECK_NEAR(e.load, 70, 0.01);  // 190 binnen, 120 de accu in
  CHECK_NEAR(e.soc, 80, 0.01);
  CHECK_EQ(e.chargeState, 3);
  // Oud: telt niet mee
  fakeMillis += (FRESH_S + 1) * 1000;
  e = energy_summarize(d, 2);
  CHECK(!e.haveSolar && !e.haveShunt);
}

TEST(energy_dcdc_counts_as_charger) {
  // Motor draait: Orion XS laadt 300 W, de accu krijgt 250 W: verbruik 50 W (zonder de Orion: 0)
  fakeMillis = 2000000;
  DeviceState d[2] = {dev(SensorType::VictronOrionXs), dev(SensorType::VictronBattery)};
  d[0].reading.m.watts = 300;
  d[0].reading.m.state = 3;
  d[1].reading.m.watts = 250;
  d[1].reading.m.amps = 18;
  d[1].reading.m.volts = 13.9f;
  EnergySummary e = energy_summarize(d, 2);
  CHECK(e.haveDcIn);
  CHECK_NEAR(e.dcIn, 300, 0.01);
  CHECK_NEAR(e.load, 50, 0.01);
  CHECK_EQ(e.chargeState, 3);
}

TEST(energy_dc_meter) {
  fakeMillis = 3000000;
  DeviceState d[4] = {dev(SensorType::VictronDcMeter), dev(SensorType::VictronDcMeter),
                      dev(SensorType::VictronDcMeter), dev(SensorType::VictronBattery)};
  d[0].reading.m.meterType = -6;  // dynamo
  d[0].reading.m.watts = 400;
  d[1].reading.m.meterType = -9;  // zonnelader zonder Bluetooth
  d[1].reading.m.watts = 100;
  d[2].reading.m.meterType = 3;  // koelkast: zit al in het verbruik
  d[2].reading.m.watts = 45;
  d[3].reading.m.watts = 380;
  d[3].reading.m.amps = 27;
  d[3].reading.m.volts = 14.0f;
  EnergySummary e = energy_summarize(d, 4);
  CHECK_NEAR(e.dcIn, 400, 0.01);
  CHECK(e.haveSolar);
  CHECK_NEAR(e.solarOut, 100, 0.01);
  CHECK_NEAR(e.load, 120, 0.01);  // 400 + 100 binnen, 380 de accu in
}
