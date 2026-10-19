// Pc-tests voor de BLE-decoders (src/sensors.cpp).
// Testdata uit de tests van de referentiebibliotheken (echte pakketten van echte apparaten):
//   victron-ble (github.com/keshavdv/victron-ble, Unlicense), bthome-ble, thermobeacon-ble en
//   mopeka-iot-ble (github.com/Bluetooth-Devices, MIT), ruuvitag-sensor (github.com/ttu/ruuvitag-sensor, MIT),
//   ble_monitor (github.com/custom-components/ble_monitor, MIT). Zie THIRD_PARTY.md.
#include "sensors.h"
#include "check.h"
#include <mbedtls/aes.h>

namespace {

std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> v;
  for (; s[0] && s[1]; s += 2) {
    unsigned b;
    sscanf(s, "%2x", &b);
    v.push_back(b);
  }
  return v;
}

std::vector<uint8_t> keyOf(const char *s) {
  auto k = hex(s);
  CHECK(k.size() == 16);
  return k;
}

// Victron-pakket zoals victron-ble het in de tests heeft (vanaf 0x10), met het bedrijfs-id ervoor
struct VictronAdv {
  std::vector<uint8_t> mfg;
  AdvData adv;
  explicit VictronAdv(const char *data) {
    mfg = {0xE1, 0x02};
    auto d = hex(data);
    mfg.insert(mfg.end(), d.begin(), d.end());
    adv.mfg = mfg.data();
    adv.mfgLen = mfg.size();
  }
};

// Ontsleutelde victron-ble-testdata weer versleutelen, zodat hij door de echte decoder gaat
struct VictronPlain {
  std::vector<uint8_t> mfg;
  AdvData adv;
  uint8_t key[16];
  VictronPlain(uint8_t record, const std::vector<uint8_t> &plain) {
    for (int i = 0; i < 16; i++) key[i] = 0x11 * i + 3;
    mfg = {0xE1, 0x02, 0x10, 0x00, 0x34, 0x12, record, 0x5A, 0xA5, key[0]};
    uint8_t nonce[16] = {0x5A, 0xA5}, stream[16];
    size_t off = 0;
    std::vector<uint8_t> enc(plain.size());
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);
    mbedtls_aes_crypt_ctr(&aes, plain.size(), &off, nonce, stream, plain.data(), enc.data());
    mfg.insert(mfg.end(), enc.begin(), enc.end());
    adv.mfg = mfg.data();
    adv.mfgLen = mfg.size();
  }
  VictronPlain(uint8_t record, const char *plainHex) : VictronPlain(record, hex(plainHex)) {}
};

// Tekst van een regel op de tegel, of nullptr als die er niet is
const char *value(const Reading &r, const char *label) {
  for (int i = 0; i < r.count; i++)
    if (!strcmp(r.values[i].label, label)) return r.values[i].text;
  return nullptr;
}

Reading victron(const char *data, const char *key, SensorType expect) {
  VictronAdv v(data);
  CHECK_EQ((int)detectType(v.adv), (int)expect);
  auto k = keyOf(key);
  Reading r;
  CHECK_EQ((int)parseReading(expect, v.adv, k.data(), r), (int)ParseResult::Ok);
  return r;
}

Reading victronPlain(uint8_t record, const char *plain) {
  VictronPlain v(record, plain);
  SensorType t = detectType(v.adv);
  Reading r;
  CHECK_EQ((int)parseReading(t, v.adv, v.key, r), (int)ParseResult::Ok);
  return r;
}

}  // namespace

TEST(victron_battery_monitor) {
  // SmartShunt 500A/50mV
  Reading r = victron("100289a302b040af925d09a4d89aa0128bdef48c6298a9", "aff4d0995b7d1e176c0c33ecb9e70dcd",
                      SensorType::VictronBattery);
  CHECK_NEAR(r.m.volts, 12.53, 0.001);
  CHECK_NEAR(r.m.amps, 0, 0.001);
  CHECK_NEAR(r.m.soc, 50.0, 0.001);
  CHECK_EQ(r.m.ttgMin, -1);  // 0xFFFF: niet beschikbaar
  CHECK_EQ(r.m.alarm, 0);
  CHECK(isnan(r.m.tempC));
  CHECK_STR(value(r, "Verbruikt"), "-50.0 Ah");
  CHECK_STR(r.primary, "50.0 %");
  CHECK(!value(r, "Startaccu"));
  CHECK(!value(r, "Middenpunt"));
}

TEST(victron_battery_monitor_aux) {
  Reading r = victronPlain(0x02, "ffffe6040000feff010000000080fe0c");
  CHECK_STR(value(r, "Middenpunt"), "655.34 V");
  r = victronPlain(0x02, "ffffe6040000feff000000000080feac");
  CHECK_STR(value(r, "Startaccu"), "-0.02 V");
  r = victronPlain(0x02, "ffffe6040000ffff020000000080fede");
  CHECK_NEAR(r.m.tempC, 382.2, 0.01);
}

TEST(victron_key_mismatch) {
  VictronAdv v("100289a302bb01af129087600b9b97bc2c32867c8238da");
  auto k = keyOf("ffffffffffffffffffffffffffffffff");
  Reading r;
  CHECK_EQ((int)parseReading(SensorType::VictronBattery, v.adv, k.data(), r), (int)ParseResult::BadKey);
  CHECK_EQ((int)parseReading(SensorType::VictronBattery, v.adv, nullptr, r), (int)ParseResult::NeedKey);
}

TEST(victron_battery_sense) {
  // Smart Battery Sense: spanning en temperatuur, geen stroom en geen laadtoestand
  Reading r = victron("1000a4a3025f150d8dcbff517f30eb65e76b22a04ac4e1", "0da694539597f9cf6c613cde60d7bf05",
                      SensorType::VictronBattery);
  CHECK_NEAR(r.m.volts, 12.22, 0.001);
  CHECK_NEAR(r.m.tempC, 22.5, 0.01);
  CHECK(isnan(r.m.amps));
  CHECK(isnan(r.m.soc));
  CHECK(!value(r, "Stroom"));
  CHECK(!value(r, "Verbruikt"));
  CHECK_STR(r.primary, "12.22 V");
  r = victronPlain(0x02, "fffff80400008971feff7fffffffff5c");
  CHECK_NEAR(r.m.volts, 12.72, 0.001);
  CHECK_NEAR(r.m.tempC, 17.5, 0.01);
}

TEST(victron_solar) {
  // BlueSolar MPPT 75/15
  Reading r = victron("100242a0016207adceb37b605d7e0ee21b24df5c", "adeccb947395801a4dd45a2eaa44bf17",
                      SensorType::VictronSolar);
  CHECK_EQ(r.m.state, 4);  // absorptie
  CHECK_NEAR(r.m.volts, 13.88, 0.001);
  CHECK_NEAR(r.m.amps, 1.4, 0.001);
  CHECK_NEAR(r.m.yieldKwh, 0.03, 0.0001);
  CHECK_NEAR(r.m.pvWatts, 19, 0.001);
  CHECK_NEAR(r.m.loadAmps, 0, 0.001);
  CHECK_STR(r.primary, "19 W");
  // MPPT 100/x zonder load-uitgang
  r = victronPlain(0x01, "0300fb09650032000901ffff31bc45ad");
  CHECK_EQ(r.m.state, 3);
  CHECK_NEAR(r.m.volts, 25.55, 0.001);
  CHECK_NEAR(r.m.amps, 10.1, 0.001);
  CHECK_NEAR(r.m.pvWatts, 265, 0.001);
  CHECK_NEAR(r.m.yieldKwh, 0.5, 0.0001);  // victron-ble: 500 Wh
  CHECK(isnan(r.m.loadAmps));
  CHECK(!value(r, "Load"));
}

TEST(victron_dcdc) {
  // Orion Smart 12/12-18, uit (motor stil): uitgangsspanning niet beschikbaar
  Reading r = victron("1000c0a304121d64ca8d442b90bbdf6a8cba", "64ba49f1a8562e45197a8e1fe50d7658",
                      SensorType::VictronDcDc);
  CHECK_EQ(r.m.state, 0);
  CHECK_EQ(r.m.error, 0);
  CHECK_STR(value(r, "Ingang"), "13.15 V");
  CHECK(isnan(r.m.volts));
  CHECK(!value(r, "Uitgang"));
  CHECK_STR(r.primary, "--");
}

TEST(victron_ac_charger) {
  // Blue Smart IP22 12/30
  Reading r = victron("100030a308f926c1b5170a0d2280335bf12d5ed083", "c129cf8f75c3fe5a1655b481e205fb7d",
                      SensorType::VictronAcCharger);
  CHECK_EQ(r.m.state, 6);  // druppelladen (storage)
  CHECK_EQ(r.m.error, 0);
  CHECK_NEAR(r.m.volts, 13.49, 0.001);
  CHECK_NEAR(r.m.amps, 0.4, 0.001);
  CHECK_NEAR(r.m.tempC, 21, 0.001);
  CHECK(!value(r, "AC-stroom"));
  r = victronPlain(0x08, "060046a500ffffffffffffbdffeb3d1f");
  CHECK_NEAR(r.m.volts, 13.5, 0.001);
  CHECK_NEAR(r.m.amps, 0.5, 0.001);
  CHECK_NEAR(r.m.tempC, 21, 0.001);
}

TEST(victron_smart_lithium) {
  // victron-ble test_parse_4 en _5: 8 cellen (24 V)
  struct {
    std::vector<uint8_t> raw;
    float volts, temp;
    const char *balancer;
    float cells[8];
  } cases[] = {
      {{0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 'G', 0xa3, 0xd1, 'h', ',', 0x1a, 0x8d, 'P', '\n', 0xcd, 0x96, 'd', 0xf2,
        0xf5, 0x0b, 0x85, 0xed, 0xe8, 'h', 0xf6, 0x1d, 0xc1, 0xae, 0xa1, 0xce, 0x83},
       26.4f, 37, nullptr, {3.31f, 3.30f, 3.30f, 3.30f, 3.30f, 3.29f, 3.30f, 3.30f}},
      {{0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0xcc, 0xe5, 0x92, 0xb9, '\\', '.', 0x99, '{', 0x1a, 0xd0, 'P', '%', 0xcf,
        0xa5, 0xa6, 0xb2, '[', 0x91, '<', 'T', 0x85, 0xee, 0x85, 'b', 'y', 0xae},
       26.83f, 40, "in balans", {3.36f, 3.35f, 3.35f, 3.36f, 3.35f, 3.35f, 3.35f, 3.36f}},
  };
  for (auto &c : cases) {
    VictronPlain v(0x05, c.raw);
    Reading r;
    CHECK_EQ((int)parseReading(SensorType::VictronLithium, v.adv, v.key, r), (int)ParseResult::Ok);
    CHECK_NEAR(r.m.volts, c.volts, 0.001);
    CHECK_NEAR(r.m.tempC, c.temp, 0.001);
    for (int i = 0; i < 8; i++) CHECK_NEAR(r.cellV[i], c.cells[i], 0.001);
    if (c.balancer) CHECK_STR(value(r, "Balancer"), c.balancer);
    else CHECK(!value(r, "Balancer"));
  }
  // 12 V-accu met 4 cellen (test_parse): cellen 5-8 bestaan niet
  std::vector<uint8_t> raw = {0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0xc7, 0xe3, 0xf1, 0xf8, 0xff, 0xff, 0xff, ',', '5', 0xb5,
                              0xfa, 0xb4, 'x', 0x01, 0x0f, 0xd2, 'I', 0xd2, 0xae, '_', 'i', 'V', 0xe1, 0xf8, 0xa9, 'e'};
  VictronPlain v(0x05, raw);
  Reading r;
  CHECK_EQ((int)parseReading(SensorType::VictronLithium, v.adv, v.key, r), (int)ParseResult::Ok);
  CHECK_NEAR(r.m.volts, 13.24, 0.001);
  CHECK_NEAR(r.m.tempC, 13, 0.001);
  for (int i = 0; i < 4; i++) CHECK_NEAR(r.cellV[i], 3.31, 0.001);
  for (int i = 4; i < 8; i++) CHECK(isnan(r.cellV[i]));
  CHECK_STR(value(r, "Balancer"), "onbalans");
  CHECK_STR(value(r, "Cellen"), "3.31-3.31 V");
}

TEST(victron_lynx) {
  std::vector<uint8_t> raw = {0x00, '@', '8', 0x8b, '\n', 0xfa, 0xff, 0x95, 0x15, 'U', 0x14, 0x8c, 0xcf, 0x02, 0x00, 0xff,
                              0xb3, 0xea, 0xf1, 't', 0xd6, 0xfc, 'z', 'H', 'T', 0xb8, 0xec, 0x00, 0x86, '\t', 0xe9, 0xca};
  VictronPlain v(0x0A, raw);
  Reading r;
  CHECK_EQ((int)parseReading(SensorType::VictronLynx, v.adv, v.key, r), (int)ParseResult::Ok);
  CHECK_NEAR(r.m.volts, 26.99, 0.001);
  CHECK_NEAR(r.m.amps, -0.6, 0.001);
  CHECK_NEAR(r.m.soc, 99.5, 0.001);
  CHECK_EQ(r.m.ttgMin, 14400);
  CHECK(isnan(r.m.tempC));
  CHECK_STR(value(r, "Verbruikt"), "4.4 Ah");
  CHECK(!value(r, "BMS-fout"));
}

TEST(victron_battery_protect) {
  Reading r = victron("1080b0a3093523fadedea38b1af8bcbde91ca8b6dbb60e", "fac570d66380b797a5b7543758be00e4",
                      SensorType::VictronBatteryProtect);
  CHECK_STR(r.primary, "Aan");
  CHECK_NEAR(r.m.volts, 13.07, 0.001);
  CHECK_STR(value(r, "Uitgang"), "13.07 V");
  CHECK_EQ(r.m.alarm, 0);
  CHECK_EQ(r.m.error, 0);
  CHECK(!value(r, "Waarschuwing"));
  CHECK(!value(r, "Reden uit"));
}

TEST(victron_vebus) {
  const char *key = "da3f5fa2860cb1cf86ba7a6d1d16b9dd";
  // MultiPlus-II, druppelladen op walstroom
  Reading r = victron("100380270c1252dad26f0b8eb39162074d140df410", key, SensorType::VictronVeBus);
  CHECK_EQ(r.m.state, 5);
  CHECK_NEAR(r.m.volts, 14.45, 0.001);
  CHECK_NEAR(r.m.amps, 23.2, 0.001);
  CHECK_EQ(r.m.acIn, 0);
  CHECK_NEAR(r.m.acInW, 1459, 0.001);
  CHECK_NEAR(r.m.acOutW, 1046, 0.001);
  CHECK_NEAR(r.m.tempC, 32, 0.001);
  CHECK(isnan(r.m.soc));
  CHECK(!value(r, "Alarm"));
  CHECK(!value(r, "Waarschuwing"));
  CHECK(!value(r, "Fout"));
  // Omvormen zonder walstroom
  r = victron("100380270ce1b2dabd34912a6ecec963899227a220", key, SensorType::VictronVeBus);
  CHECK_EQ(r.m.state, 9);
  CHECK_NEAR(r.m.volts, 12.43, 0.001);
  CHECK_NEAR(r.m.amps, -5.9, 0.001);
  CHECK_EQ(r.m.acIn, 2);
  CHECK_NEAR(r.m.acInW, 0, 0.001);
  CHECK_NEAR(r.m.acOutW, 45, 0.001);
  CHECK_NEAR(r.m.tempC, 30, 0.001);
  CHECK_STR(value(r, "AC-ingang"), "niet aangesloten");
  // Uit
  r = victron("100380270c65b8dad9dd594c7f57d1de9e27a3e2ab", key, SensorType::VictronVeBus);
  CHECK_EQ(r.m.state, 0);
  CHECK(!value(r, "Fout"));
}

TEST(victron_multi_rs) {
  Reading r = victron("100043a40bf4e434af0e46c3b8eb68e08e616993f70c", "346c410e8c824dd723c0f5b13b9eabc8",
                      SensorType::VictronMultiRs);
  CHECK_EQ(r.m.state, 9);
  CHECK_EQ(r.m.error, 0);
  CHECK_NEAR(r.m.amps, -12.8, 0.001);
  CHECK_NEAR(r.m.volts, 51.71, 0.001);
  CHECK_EQ(r.m.acIn, 2);
  CHECK_NEAR(r.m.acInW, 0, 0.001);
  CHECK_NEAR(r.m.acOutW, 722, 0.001);
  CHECK_NEAR(r.m.pvWatts, 0, 0.001);
  CHECK_NEAR(r.m.yieldKwh, 5.32, 0.0001);
  // Niet beschikbaar
  r = victronPlain(0x0B, "0900ff7f3294ff7fff7fffff1402");
  CHECK(isnan(r.m.amps));
  CHECK(isnan(r.m.acInW));
  CHECK(isnan(r.m.acOutW));
  CHECK(isnan(r.m.pvWatts));
  CHECK(!value(r, "PV"));
}

TEST(victron_dc_meter) {
  // SmartShunt als DC-energiemeter, hulpingang = startaccu
  Reading r = victron("100289a30d787fafde83ccec982199fd815286", "aff4d0995b7d1e176c0c33ecb9e70dcd",
                      SensorType::VictronDcMeter);
  CHECK_NEAR(r.m.volts, 12.52, 0.001);
  CHECK_NEAR(r.m.amps, 0, 0.001);
  CHECK_STR(value(r, "Startaccu"), "-0.01 V");
  CHECK_EQ(r.m.alarm, 0);
  CHECK(isnan(r.m.tempC));
  CHECK_EQ(r.m.meterType, -3);  // DC-DC-lader
  r = victronPlain(0x0D, "fdffe4040000ffff020000ae28af8a5c");
  CHECK(isnan(r.m.tempC));
  CHECK(!value(r, "Temperatuur"));
  r = victronPlain(0x0D, "fdffe40400008888020000ae28af8a5c");
  CHECK_NEAR(r.m.tempC, 76.37, 0.01);
  r = victronPlain(0x0D, "ffff6f0a0000000003000043698118eb");
  CHECK_NEAR(r.m.volts, 26.71, 0.001);
  CHECK_EQ(r.m.meterType, -1);  // algemene bron
  CHECK(!value(r, "Startaccu"));
  CHECK(!value(r, "Temperatuur"));
}

TEST(victron_key_text) {
  uint8_t k[16];
  CHECK(parseVictronKey("aff4d0995b7d1e176c0c33ecb9e70dcd", k));
  CHECK_EQ(k[0], 0xAF);
  CHECK_EQ(k[15], 0xCD);
  CHECK(parseVictronKey("AF F4-D0:99 5B7D1E176C0C33ECB9E70DCD\n", k));
  CHECK(!parseVictronKey("aff4d0995b7d1e176c0c33ecb9e70dc", k));    // te kort
  CHECK(!parseVictronKey("aff4d0995b7d1e176c0c33ecb9e70dcd0", k));  // te lang
  CHECK(!parseVictronKey("gff4d0995b7d1e176c0c33ecb9e70dcd", k));   // geen hex
}

TEST(ruuvi_format5) {
  // ruuvitag-sensor test_decoder.py (dataformaat 5)
  std::vector<uint8_t> mfg = {0x99, 0x04};
  auto d = hex("0512FC5394C37C0004FFFC040CAC364200CDCBB8334C884F");
  mfg.insert(mfg.end(), d.begin(), d.end());
  AdvData adv;
  adv.mfg = mfg.data();
  adv.mfgLen = mfg.size();
  CHECK_EQ((int)detectType(adv), (int)SensorType::Ruuvi);
  Reading r;
  CHECK_EQ((int)parseReading(SensorType::Ruuvi, adv, nullptr, r), (int)ParseResult::Ok);
  CHECK_NEAR(r.m.tempC, 24.3, 0.001);
  CHECK_NEAR(r.m.humidity, 53.49, 0.001);
  CHECK_NEAR(r.m.pressureHpa, 1000.44, 0.001);
  CHECK_STR(value(r, "Batterij"), "2.98 V");
  CHECK_STR(value(r, "Bewegingen"), "66");
  // Alle waarden "niet beschikbaar"
  mfg = {0x99, 0x04};
  d = hex("058000FFFFFFFF800080008000FFFFFFFFFFFFFFFFFFFFFF");
  mfg.insert(mfg.end(), d.begin(), d.end());
  adv.mfg = mfg.data();
  adv.mfgLen = mfg.size();
  CHECK_EQ((int)parseReading(SensorType::Ruuvi, adv, nullptr, r), (int)ParseResult::Ok);
  CHECK(isnan(r.m.tempC));
  CHECK(isnan(r.m.humidity));
  CHECK(isnan(r.m.pressureHpa));
  CHECK(!value(r, "Batterij"));
  CHECK_STR(r.primary, "--");
}

TEST(atc_thermometer) {
  // ble_monitor test_atc_parser.py: ATC1441-formaat (big-endian): MAC, temperatuur 0,1 °C, vocht, batterij %, mV, teller
  auto d = hex("a4c1380283f400a22f5f0bf819");
  AdvData adv;
  adv.svc181a = d.data();
  adv.svc181aLen = d.size();
  CHECK_EQ((int)detectType(adv), (int)SensorType::AtcThermo);
  Reading r;
  CHECK_EQ((int)parseReading(SensorType::AtcThermo, adv, nullptr, r), (int)ParseResult::Ok);
  CHECK_NEAR(r.m.tempC, 16.2, 0.001);
  CHECK_NEAR(r.m.humidity, 47, 0.001);
  CHECK_STR(value(r, "Batterij"), "95 %");
  CHECK_STR(value(r, "Batterijspanning"), "3.06 V");
  // pvvx-formaat (little-endian): temperatuur 0,01 °C, vocht 0,01 %, mV, batterij %, teller, vlaggen
  d = hex("f4830238c1a4a9066911b60b58f70d");
  adv.svc181a = d.data();
  adv.svc181aLen = d.size();
  CHECK_EQ((int)detectType(adv), (int)SensorType::AtcThermo);
  CHECK_EQ((int)parseReading(SensorType::AtcThermo, adv, nullptr, r), (int)ParseResult::Ok);
  CHECK_NEAR(r.m.tempC, 17.05, 0.001);
  CHECK_NEAR(r.m.humidity, 44.57, 0.001);
  CHECK_STR(value(r, "Batterij"), "88 %");
  CHECK_STR(value(r, "Batterijspanning"), "3.00 V");
}

namespace {
Reading btHome(const char *data) {
  auto d = hex(data);
  AdvData adv;
  adv.bthome = d.data();
  adv.bthomeLen = d.size();
  CHECK_EQ((int)detectType(adv), (int)SensorType::BTHome);
  Reading r;
  CHECK_EQ((int)parseReading(SensorType::BTHome, adv, nullptr, r), (int)ParseResult::Ok);
  return r;
}
}  // namespace

TEST(bthome_v2) {
  // bthome-ble test_parser_v2.py
  Reading r = btHome("4002ca0903bf13");
  CHECK_NEAR(r.m.tempC, 25.06, 0.001);
  CHECK_NEAR(r.m.humidity, 50.55, 0.001);
  CHECK_STR(r.primary, "25.1 °C");
  r = btHome("4004138a01");
  CHECK_NEAR(r.m.pressureHpa, 1008.83, 0.001);
  r = btHome("400c020c");
  CHECK_NEAR(r.m.volts, 3.074, 0.0001);
  r = btHome("40434e34");
  CHECK_NEAR(r.m.amps, 13.39, 0.0001);
  r = btHome("40451101");
  CHECK_NEAR(r.m.tempC, 27.3, 0.001);
  r = btHome("404a020c");
  CHECK_NEAR(r.m.volts, 307.4, 0.001);
  r = btHome("40015d025d0903b718");
  CHECK_STR(value(r, "Batterij"), "93 %");
  CHECK_NEAR(r.m.tempC, 23.97, 0.001);
  CHECK_NEAR(r.m.humidity, 63.27, 0.001);
  // Shelly BLU H&T (test_shelly_blu_ht_display_zb): binaire sensor (licht) tussen de waarden
  r = btHome("40002e0164" "1e01" "2e34" "451601");
  CHECK_STR(value(r, "Batterij"), "100 %");
  CHECK_NEAR(r.m.humidity, 52, 0.001);
  CHECK_NEAR(r.m.tempC, 27.8, 0.001);
  // 1-byte temperaturen en stroom met teken
  r = btHome("4057ea");
  CHECK_NEAR(r.m.tempC, -22, 0.001);
  r = btHome("4058ea");
  CHECK_NEAR(r.m.tempC, -7.7, 0.001);
  r = btHome("405d02ea");
  CHECK_NEAR(r.m.amps, -5.63, 0.0001);
  // Objecten die we niet tonen worden overgeslagen, ook vóór een waarde (objecten staan op volgorde van id):
  // knop (0x3A, test_bthome_event_button_long_press), firmwareversie (0xF1, test_bthome_firmware_version_4),
  // commando (0x3B) en tekst (0x53)
  r = btHome("4000210164" "3a04" "451601");
  CHECK_NEAR(r.m.tempC, 27.8, 0.001);
  CHECK_STR(value(r, "Batterij"), "100 %");
  r = btHome("4002ca09" "f100010204");
  CHECK_NEAR(r.m.tempC, 25.06, 0.001);
  r = btHome("40" "3b010305" "451601");
  CHECK_NEAR(r.m.tempC, 27.8, 0.001);
  r = btHome("44" "530c48656c6c6f20576f726c6421" "5d02ea");
  CHECK_NEAR(r.m.amps, -5.63, 0.0001);
  // Versleuteld (bit 0 van de eerste byte): niet ondersteund
  auto d = hex("4102ca09");
  AdvData adv;
  adv.bthome = d.data();
  adv.bthomeLen = d.size();
  CHECK_EQ((int)detectType(adv), (int)SensorType::Unknown);
}

namespace {
Reading thermoBeacon(uint16_t id, const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> mfg = {(uint8_t)id, (uint8_t)(id >> 8)};
  mfg.insert(mfg.end(), payload.begin(), payload.end());
  AdvData adv;
  adv.thermoBeaconSvc = true;
  adv.mfg = mfg.data();
  adv.mfgLen = mfg.size();
  CHECK_EQ((int)detectType(adv), (int)SensorType::ThermoBeacon);
  Reading r;
  CHECK_EQ((int)parseReading(SensorType::ThermoBeacon, adv, nullptr, r), (int)ParseResult::Ok);
  return r;
}
}  // namespace

TEST(thermobeacon) {
  // thermobeacon-ble test_parser.py (manufacturer data na het "bedrijfs-id" = apparaattype)
  Reading r = thermoBeacon(16, hex("0000b002000047a4e20c8001b6024a000000"));
  CHECK_NEAR(r.m.tempC, 24.0, 0.001);
  CHECK_NEAR(r.m.humidity, 43.38, 0.01);
  CHECK_NEAR(r.m.batteryPct, 100, 0.001);
  CHECK_STR(value(r, "Batterijspanning"), "3.30 V");
  r = thermoBeacon(16, hex("00009c0d0000fe44450cc501870217000000"));
  CHECK_NEAR(r.m.tempC, 28.31, 0.01);
  CHECK_NEAR(r.m.humidity, 40.44, 0.01);
  CHECK_STR(value(r, "Batterijspanning"), "3.14 V");
  r = thermoBeacon(48, hex("00008b0100005d44d60b5901590344000c00"));
  CHECK_NEAR(r.m.tempC, 21.56, 0.01);
  CHECK_NEAR(r.m.humidity, 53.56, 0.01);
  CHECK_STR(value(r, "Batterijspanning"), "3.03 V");
  r = thermoBeacon(27, hex("0000832c0000d063820c6f01730205230000"));
  CHECK_NEAR(r.m.tempC, 22.94, 0.01);
  CHECK_NEAR(r.m.humidity, 39.19, 0.01);
  r = thermoBeacon(20, hex("000005030000f5eb330d56013c034c030000"));
  CHECK_NEAR(r.m.tempC, 21.38, 0.01);
  CHECK_NEAR(r.m.humidity, 51.75, 0.01);
}

namespace {
Reading mopeka(const char *payload, TankMedium medium) {
  std::vector<uint8_t> mfg = {0x59, 0x00};
  auto d = hex(payload);
  mfg.insert(mfg.end(), d.begin(), d.end());
  AdvData adv;
  adv.mopekaSvc = true;
  adv.mfg = mfg.data();
  adv.mfgLen = mfg.size();
  CHECK_EQ((int)detectType(adv), (int)SensorType::MopekaPro);
  Reading r;
  CHECK_EQ((int)parseReading(SensorType::MopekaPro, adv, nullptr, r, (uint8_t)medium), (int)ParseResult::Ok);
  return r;
}
}  // namespace

TEST(mopeka_pro) {
  // mopeka-iot-ble test_parser.py: Pro Plus op een propaanfles, geplaatst (kwaliteit 3)
  Reading r = mopeka("087043b6c3e0f509fae3", TankMedium::Gas);
  CHECK_NEAR(r.m.tempC, 27, 0.001);
  CHECK_EQ(r.m.quality, 3);
  CHECK_NEAR(r.m.batteryPct, 100, 0.001);
  CHECK_NEAR(r.m.distanceMm, 341.96, 0.01);  // referentie rondt af naar beneden: 341
  // Slechte meting (kwaliteit 0): geen afstand
  r = mopeka("0872460030e0f509f0d8", TankMedium::Gas);
  CHECK_EQ(r.m.quality, 0);
  CHECK(isnan(r.m.distanceMm));
  CHECK_STR(r.primary, "--");
  CHECK_NEAR(r.m.tempC, 30, 0.001);
  // Lage kwaliteit (1): wel een afstand
  r = mopeka("0872460040e0f509f0d8", TankMedium::Gas);
  CHECK_EQ(r.m.quality, 1);
  CHECK(!isnan(r.m.distanceMm));
  // Onbekend sensortype
  std::vector<uint8_t> mfg = {0x59, 0x00};
  auto d = hex("077043b6c3e0f509fae3");
  mfg.insert(mfg.end(), d.begin(), d.end());
  AdvData adv;
  adv.mopekaSvc = true;
  adv.mfg = mfg.data();
  adv.mfgLen = mfg.size();
  Reading x;
  CHECK_EQ((int)parseReading(SensorType::MopekaPro, adv, nullptr, x), (int)ParseResult::Unsupported);
}

TEST(mopeka_level) {
  CHECK_NEAR(mopekaLevelPct(250, 500, 0), 50, 0.001);
  CHECK_NEAR(mopekaLevelPct(600, 500, 0), 100, 0.001);  // begrensd
  CHECK_NEAR(mopekaLevelPct(100, 50, 450), 87.5, 0.001);  // bovenop de tank: vol = kleine afstand
  CHECK(isnan(mopekaLevelPct(NAN, 500, 0)));
  CHECK(isnan(mopekaLevelPct(100, 200, 200)));
}
