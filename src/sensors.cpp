#include "sensors.h"
#include "i18n.h"
#include <mbedtls/aes.h>
#include <math.h>
#include <stdarg.h>

namespace {

constexpr uint16_t VICTRON_ID = 0x02E1;
constexpr uint16_t RUUVI_ID = 0x0499;

uint16_t le16(const uint8_t *p) { return p[0] | (p[1] << 8); }
uint16_t be16(const uint8_t *p) { return (p[0] << 8) | p[1]; }

void add(Reading &r, const char *label, const char *fmt, ...) {
  if (r.count >= MAX_VALUES) return;
  Value &v = r.values[r.count++];
  strlcpy(v.label, label, sizeof(v.label));
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(v.text, sizeof(v.text), fmt, ap);
  va_end(ap);
}

void primary(Reading &r, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(r.primary, sizeof(r.primary), fmt, ap);
  va_end(ap);
}

// Leest bitvelden LSB-first, zoals Victron ze verpakt
struct BitReader {
  const uint8_t *d;
  size_t n;
  size_t pos = 0;
  BitReader(const uint8_t *data, size_t len) : d(data), n(len) {}
  uint32_t u(int bits) {
    uint32_t v = 0;
    for (int i = 0; i < bits; i++, pos++) {
      size_t byte = pos >> 3;
      if (byte < n && ((d[byte] >> (pos & 7)) & 1)) v |= 1UL << i;
    }
    return v;
  }
  int32_t s(int bits) {
    uint32_t v = u(bits);
    if (v & (1UL << (bits - 1))) v |= ~((1UL << bits) - 1);
    return (int32_t)v;
  }
};

}  // namespace

const char *victronStateName(uint8_t s) {
  switch (s) {
    case 0: return TN("Uit");
    case 1: return TN("Laag vermogen");
    case 2: return TN("Storing");
    case 3: return TN("Bulk");
    case 4: return TN("Absorptie");
    case 5: return TN("Float");
    case 6: return TN("Opslag");
    case 7: return TN("Egaliseren");
    case 8: return TN("Doorgeven");
    case 9: return TN("Omvormen");
    case 10: return TN("Ondersteunen");
    case 11: return TN("Voeding");
    case 245: return TN("Opstarten");
    case 247: return TN("Auto-egaliseren");
    case 248: return TN("Accu beschermd");
    case 249: return TN("Actief");
    case 252: return TN("Externe sturing");
    default: return TN("Onbekend");
  }
}

const char *victronAlarmName(uint16_t a) {
  static const char *names[] = {TN("Lage spanning"), TN("Hoge spanning"), TN("Lage SoC"), TN("Lage startaccuspanning"),
                                TN("Hoge startaccuspanning"), TN("Lage temperatuur"), TN("Hoge temperatuur"),
                                TN("Middenpunt-afwijking"), TN("Overbelasting"), TN("DC-rimpel"), TN("Lage AC-uitgang"),
                                TN("Hoge AC-uitgang"), TN("Kortsluiting"), TN("BMS-blokkade")};
  for (int i = 0; i < 14; i++)
    if (a & (1 << i)) return names[i];
  return TN("Onbekend alarm");
}

const char *victronErrorName(uint8_t c) {
  switch (c) {
    case 2: return TN("accuspanning te hoog");
    case 11: return TN("hoge rimpel");
    case 14: return TN("accutemperatuur te laag");
    case 17: return TN("lader te heet");
    case 18: return TN("overstroom");
    case 20: return TN("bulktijd overschreden");
    case 21: return TN("stroomsensor");
    case 26: return TN("oververhit");
    case 27: return TN("kortsluiting");
    case 29: return TN("overladen");
    case 33: return TN("PV-spanning te hoog");
    case 34: return TN("PV-stroom te hoog");
    case 38: case 39: return TN("ingang uitgeschakeld");
    case 67: return "BMS";
    case 116: return TN("kalibratie kwijt");
    case 119: return TN("instellingen");
    default: return TN("fout");
  }
}

namespace {

void formatMinutes(char *buf, size_t len, uint32_t mins) {
  if (mins >= 0xFFFF) snprintf(buf, len, "--");
  else if (mins >= 60 * 24 * 10) snprintf(buf, len, "%s", TN("> 10 dagen"));
  else snprintf(buf, len, "%lu h %02lu min", (unsigned long)(mins / 60), (unsigned long)(mins % 60));  // in elke taal leesbaar
}

ParseResult parseVictron(SensorType t, const AdvData &adv, const uint8_t *key, Reading &r) {
  // [0-1] company id, [2] 0x10, [3] recordlengte, [4-5] model, [6] recordtype,
  // [7-8] nonce, [9] key[0], [10..] versleutelde data
  if (adv.mfgLen < 11) return ParseResult::Unsupported;
  if (!key) return ParseResult::NeedKey;
  const uint8_t *m = adv.mfg;
  if (m[9] != key[0]) return ParseResult::BadKey;

  uint8_t plain[16] = {0};
  size_t len = min<size_t>(adv.mfgLen - 10, sizeof(plain));
  uint8_t nonce[16] = {m[7], m[8]};  // little-endian teller, rest nul
  uint8_t stream[16];
  size_t off = 0;
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, key, 128);
  mbedtls_aes_crypt_ctr(&aes, len, &off, nonce, stream, m + 10, plain);
  mbedtls_aes_free(&aes);

  BitReader b(plain, len);
  char buf[20];
  switch (t) {
    case SensorType::VictronBattery: {
      uint32_t ttg = b.u(16);
      int32_t mv = b.s(16);
      uint32_t alarm = b.u(16);
      uint32_t aux = b.u(16);
      uint32_t auxMode = b.u(2);
      int32_t ma = b.s(22);
      uint32_t consumed = b.u(20);
      uint32_t soc = b.u(10);
      bool haveAmps = ma != 0x1FFFFF;  // Battery Sense meet geen stroom
      float volts = mv == 0x7FFF ? NAN : mv * 0.01f, amps = ma * 0.001f;  // 0x7FFF = niet beschikbaar
      r.m.volts = volts;
      if (haveAmps) {
        r.m.amps = amps;
        r.m.watts = volts * amps;
      }
      if (soc != 0x3FF) r.m.soc = soc * 0.1f;
      if (ttg != 0xFFFF) r.m.ttgMin = ttg;
      if (auxMode == 2) r.m.tempC = aux * 0.01f - 273.15f;
      r.m.alarm = alarm;
      if (soc != 0x3FF) primary(r, "%.1f %%", soc * 0.1f);
      else if (!isnan(volts)) primary(r, "%.2f V", volts);
      else primary(r, "--");
      if (!isnan(volts)) add(r, TN("Spanning"), "%.2f V", volts);
      if (haveAmps) {
        add(r, TN("Stroom"), "%.2f A", amps);
        if (!isnan(volts)) add(r, TN("Vermogen"), "%.0f W", volts * amps);
      }
      if (ttg != 0xFFFF) {
        formatMinutes(buf, sizeof(buf), ttg);
        add(r, TN("Resterend"), "%s", buf);
      }
      if (consumed != 0xFFFFF) add(r, TN("Verbruikt"), "%.1f Ah", consumed * -0.1f);
      if (auxMode == 0) add(r, TN("Startaccu"), "%.2f V", (int16_t)aux * 0.01f);
      else if (auxMode == 1) add(r, TN("Middenpunt"), "%.2f V", aux * 0.01f);
      else if (auxMode == 2) add(r, TN("Temperatuur"), "%.1f °C", aux * 0.01f - 273.15f);
      if (alarm) add(r, TN("Alarm"), "%s", victronAlarmName(alarm));
      return ParseResult::Ok;
    }
    case SensorType::VictronSolar: {
      uint8_t state = b.u(8);
      uint8_t err = b.u(8);
      int32_t rawV = b.s(16), rawA = b.s(16);
      uint32_t yield = b.u(16);
      uint32_t pv = b.u(16);
      uint32_t load = b.u(9);
      // "Niet beschikbaar" volgens victron-ble: 0x7FFF bij spanning en stroom, 0xFFFF bij opbrengst en PV
      float volts = rawV == 0x7FFF ? NAN : rawV * 0.01f;
      float amps = rawA == 0x7FFF ? NAN : rawA * 0.1f;
      r.m.state = state;
      r.m.volts = volts;
      r.m.amps = amps;
      r.m.watts = volts * amps;  // NaN als een van beide ontbreekt
      r.m.pvWatts = pv == 0xFFFF ? NAN : (float)pv;
      r.m.error = err;
      r.m.yieldKwh = yield == 0xFFFF ? NAN : yield * 0.01f;
      if (load != 0x1FF) r.m.loadAmps = load * 0.1f;
      if (pv != 0xFFFF) primary(r, "%lu W", (unsigned long)pv);
      else primary(r, "--");
      add(r, TN("Status"), "%s", victronStateName(state));
      if (!isnan(volts)) add(r, TN("Accu"), "%.2f V", volts);
      if (!isnan(amps)) add(r, TN("Laadstroom"), "%.1f A", amps);
      if (yield != 0xFFFF) add(r, TN("Vandaag"), "%.2f kWh", yield * 0.01f);
      if (load != 0x1FF) add(r, TN("Load"), "%.1f A", load * 0.1f);
      if (err) add(r, TN("Fout"), "%u %s", err, victronErrorName(err));
      return ParseResult::Ok;
    }
    case SensorType::VictronDcDc: {
      uint8_t state = b.u(8);
      uint8_t err = b.u(8);
      float vin = b.u(16) * 0.01f;
      float vout = b.s(16) * 0.01f;
      r.m.state = state;
      r.m.error = err;
      r.m.volts = vout;
      primary(r, "%.2f V", vout);
      add(r, TN("Status"), "%s", victronStateName(state));
      add(r, TN("Ingang"), "%.2f V", vin);
      add(r, TN("Uitgang"), "%.2f V", vout);
      if (err) add(r, TN("Fout"), "%u %s", err, victronErrorName(err));
      return ParseResult::Ok;
    }
    case SensorType::VictronBatteryProtect: {
      // Velden volgens victron-ble (smart_battery_protect.py)
      uint32_t devState = b.u(8), outState = b.u(8), err = b.u(8);
      uint32_t alarm = b.u(16), warning = b.u(16);
      int32_t inV = b.s(16);
      uint32_t outV = b.u(16), offReason = b.u(32);
      if (inV != 0x7FFF) r.m.volts = inV * 0.01f;
      r.m.state = devState != 0xFF ? (uint8_t)devState : 0xFF;
      r.m.alarm = alarm;
      r.m.error = err;
      const char *out = outState == 1   ? TN("Aan")
                        : outState == 4 ? TN("Uit")
                        : outState == 0 ? TN("Uitgeschakeld")
                                        : TN("Onbekend");
      primary(r, "%s", out);
      if (!isnan(r.m.volts)) add(r, TN("Accu"), "%.2f V", r.m.volts);
      if (outV != 0xFFFF) add(r, TN("Uitgang"), "%.2f V", outV * 0.01f);
      add(r, TN("Status"), "%s", victronStateName(devState));
      if (warning) add(r, TN("Waarschuwing"), "0x%04lX", (unsigned long)warning);
      if (offReason) add(r, TN("Reden uit"), "0x%lX", (unsigned long)offReason);
      return ParseResult::Ok;
    }
    case SensorType::VictronLynx: {
      // Velden volgens victron-ble (lynx_smart_bms.py). Bits zonder documentatie worden niet als melding gebruikt.
      uint32_t errFlags = b.u(8), remaining = b.u(16);
      int32_t v = b.s(16), a = b.s(16);
      b.u(16);             // io-status: onbekend
      b.u(18);             // alarm-bits: onbekend, wel bewaard in de bibliotheek
      uint32_t soc = b.u(10), consumed = b.u(20), temp = b.u(7);
      if (v != 0x7FFF) r.m.volts = v * 0.01f;
      if (a != 0x7FFF) r.m.amps = a * 0.1f;
      if (!isnan(r.m.volts) && !isnan(r.m.amps)) r.m.watts = r.m.volts * r.m.amps;
      if (soc != 0x3FF) r.m.soc = soc * 0.1f;  // 0x3FF = ongeldig (10 bits)
      if (remaining != 0xFFFF) r.m.ttgMin = remaining;
      if (temp != 0x7F) r.m.tempC = (int)temp - 40;
      // errFlags niet in m.error: dat veld is voor laderfouten en geeft een verkeerde melding
      if (!isnan(r.m.soc)) primary(r, "%.1f %%", r.m.soc);
      else if (!isnan(r.m.volts)) primary(r, "%.2f V", r.m.volts);
      else primary(r, "--");
      if (!isnan(r.m.volts)) add(r, TN("Spanning"), "%.2f V", r.m.volts);
      if (!isnan(r.m.amps)) add(r, TN("Stroom"), "%.1f A", r.m.amps);
      if (!isnan(r.m.watts)) add(r, TN("Vermogen"), "%.0f W", r.m.watts);
      if (consumed != 0xFFFFF) add(r, TN("Verbruikt"), "%.1f Ah", consumed * 0.1f);  // Lynx: positief, anders dan de SmartShunt
      if (errFlags) add(r, TN("BMS-fout"), "0x%02lX", (unsigned long)errFlags);
      return ParseResult::Ok;
    }
    case SensorType::VictronLithium: {
      // Bitvelden volgens victron-ble (smart_lithium.py): flags 32, fouten 16, 8 cellen à 7 bits,
      // accuspanning 12, balancer 4, temperatuur 7
      b.u(32);
      uint32_t err = b.u(16);
      float lo = INFINITY, hi = -INFINITY;
      int valid = 0;
      for (int i = 0; i < 8; i++) {
        uint32_t c = b.u(7);
        // 0x00 = -inf, 0x7E = +inf, 0x7F = geen cel: alleen 0x01-0x7D is een echte spanning
        r.cellV[i] = (c == 0x00 || c == 0x7E || c == 0x7F) ? NAN : (260 + c) / 100.0f;
        if (!isnan(r.cellV[i])) {
          lo = fminf(lo, r.cellV[i]);
          hi = fmaxf(hi, r.cellV[i]);
          valid++;
        }
      }
      uint32_t bv = b.u(12), bal = b.u(4), temp = b.u(7);
      if (bv != 0x0FFF) r.m.volts = bv * 0.01f;
      if (temp != 0x7F) r.m.tempC = (int)temp - 40;
      if (!isnan(r.m.volts)) primary(r, "%.2f V", r.m.volts);
      else primary(r, "--");
      if (valid) add(r, TN("Cellen"), "%.2f-%.2f V", lo, hi);
      static const char *bal_names[] = {TN("onbekend"), TN("in balans"), TN("balanceert"), TN("onbalans")};
      if (bal >= 1 && bal <= 3) add(r, TN("Balancer"), "%s", bal_names[bal]);
      if (err) add(r, TN("Fout"), "0x%04lX", (unsigned long)err);
      return ParseResult::Ok;
    }
    case SensorType::VictronAcCharger: {
      uint8_t state = b.u(8);
      uint8_t err = b.u(8);
      float watts = 0, amps = 0, volts = NAN;
      for (int out = 0; out < 3; out++) {  // tot 3 uitgangen
        uint32_t v = b.u(13), a = b.u(11);
        if (v == 0x1FFF || a == 0x7FF) continue;
        if (isnan(volts)) volts = v * 0.01f;
        amps += a * 0.1f;
        watts += v * 0.01f * a * 0.1f;
      }
      uint32_t temp = b.u(7);
      uint32_t ac = b.u(9);
      r.m.state = state;
      r.m.volts = volts;
      r.m.amps = amps;
      r.m.watts = watts;
      r.m.error = err;
      if (temp != 0x7F) r.m.tempC = (int)temp - 40;
      primary(r, "%.0f W", watts);
      add(r, TN("Status"), "%s", victronStateName(state));
      if (!isnan(volts)) add(r, TN("Uitgang"), "%.2f V", volts);
      add(r, TN("Laadstroom"), "%.1f A", amps);
      if (temp != 0x7F) add(r, TN("Temperatuur"), "%d °C", (int)temp - 40);
      if (ac != 0x1FF) add(r, TN("AC-stroom"), "%.1f A", ac * 0.1f);
      if (err) add(r, TN("Fout"), "%u %s", err, victronErrorName(err));
      return ParseResult::Ok;
    }
    case SensorType::VictronInverter: {
      // Velden volgens victron-ble (inverter.py)
      uint32_t state = b.u(8), alarm = b.u(16);
      int32_t mv = b.s(16);
      uint32_t va = b.u(16), acV = b.u(15), acA = b.u(11);
      r.m.state = state;
      r.m.alarm = alarm;
      if (mv != 0x7FFF) r.m.volts = mv * 0.01f;
      if (va != 0xFFFF) r.m.watts = va;  // schijnbaar vermogen (VA), voor het overzicht als W
      if (va != 0xFFFF) primary(r, "%lu VA", (unsigned long)va);
      else primary(r, "--");
      add(r, TN("Status"), "%s", victronStateName(state));
      if (!isnan(r.m.volts)) add(r, TN("Accu"), "%.2f V", r.m.volts);
      if (acV != 0x7FFF) add(r, TN("AC-uitgang"), "%.0f V", acV * 0.01f);
      if (acA != 0x7FF) add(r, TN("AC-stroom"), "%.1f A", acA * 0.1f);
      if (alarm) add(r, TN("Alarm"), "%s", victronAlarmName(alarm));
      return ParseResult::Ok;
    }
    case SensorType::VictronVeBus: {
      // MultiPlus / Quattro (victron-ble vebus.py)
      uint32_t state = b.u(8), err = b.u(8);
      int32_t da = b.s(16);
      uint32_t mv = b.u(14), acIn = b.u(2);
      int32_t inW = b.s(19), outW = b.s(19);
      uint32_t alarm = b.u(2), temp = b.u(7), soc = b.u(7);
      r.m.state = state;
      if (mv != 0x3FFF) r.m.volts = mv * 0.01f;
      if (da != 0x7FFF) r.m.amps = da * 0.1f;
      if (!isnan(r.m.volts) && !isnan(r.m.amps)) r.m.watts = r.m.volts * r.m.amps;
      if (temp != 0x7F) r.m.tempC = (int)temp - 40;
      if (soc != 0x7F) r.m.soc = soc;
      r.m.acIn = acIn;
      r.m.acInW = inW != 0x3FFFF ? inW : NAN;
      r.m.acOutW = outW != 0x3FFFF ? outW : NAN;
      primary(r, "%s", victronStateName(state));
      if (!isnan(r.m.volts)) add(r, TN("Accu"), "%.2f V", r.m.volts);
      if (!isnan(r.m.amps)) add(r, TN("Accustroom"), "%+.1f A", r.m.amps);
      if (acIn != 3) add(r, TN("AC-ingang"), "%s", acIn == 2 ? TN("niet aangesloten") : acIn == 1 ? "AC2" : "AC1");
      if (!isnan(r.m.acInW)) add(r, TN("AC in"), "%.0f W", r.m.acInW);
      if (!isnan(r.m.acOutW)) add(r, TN("AC uit"), "%.0f W", r.m.acOutW);
      // Fout en alarm vóór temperatuur en SoC: er passen maar 8 regels op een tegel
      if (err && err != 0xFF) add(r, TN("Fout"), "VE.Bus %u", (unsigned)err);
      if (alarm == 2) add(r, TN("Alarm"), "%s", TN("alarm"));
      else if (alarm == 1) add(r, TN("Waarschuwing"), "%s", TN("waarschuwing"));
      if (temp != 0x7F) add(r, TN("Temperatuur"), "%d °C", (int)temp - 40);
      if (soc != 0x7F) add(r, "SoC", "%u %%", (unsigned)soc);
      return ParseResult::Ok;
    }
    case SensorType::VictronMultiRs: {
      // Multi RS (Solar) (victron-ble multirs.py): bytes, little-endian
      if (len < 14) return ParseResult::Unsupported;
      const uint8_t *p = plain;
      uint8_t state = p[0], err = p[1];
      int16_t da = (int16_t)le16(p + 2);
      uint16_t vin = le16(p + 4);
      int16_t inW = (int16_t)le16(p + 6), outW = (int16_t)le16(p + 8);
      uint16_t pv = le16(p + 10), yld = le16(p + 12);
      uint8_t acIn = vin >> 14;
      r.m.state = state;
      r.m.error = err != 0xFF ? err : 0;
      r.m.volts = (vin & 0x3FFF) * 0.01f;
      if (da != 0x7FFF) r.m.amps = da * 0.1f;
      if (!isnan(r.m.amps)) r.m.watts = r.m.volts * r.m.amps;
      r.m.acIn = acIn;
      r.m.acInW = inW != 0x7FFF ? inW : NAN;
      r.m.acOutW = outW != 0x7FFF ? outW : NAN;
      if (pv != 0xFFFF) r.m.pvWatts = pv;
      if (yld != 0xFFFF) r.m.yieldKwh = yld * 0.01f;
      primary(r, "%s", victronStateName(state));
      add(r, TN("Accu"), "%.2f V", r.m.volts);
      if (!isnan(r.m.amps)) add(r, TN("Accustroom"), "%+.1f A", r.m.amps);
      if (acIn != 3) add(r, TN("AC-ingang"), "%s", acIn == 2 ? TN("niet aangesloten") : acIn == 1 ? "AC2" : "AC1");
      if (!isnan(r.m.acInW)) add(r, TN("AC in"), "%.0f W", r.m.acInW);
      if (!isnan(r.m.acOutW)) add(r, TN("AC uit"), "%.0f W", r.m.acOutW);
      if (pv != 0xFFFF) add(r, "PV", "%u W", (unsigned)pv);
      if (yld != 0xFFFF) add(r, TN("Vandaag"), "%.2f kWh", yld * 0.01f);
      if (r.m.error) add(r, TN("Fout"), "%u %s", r.m.error, victronErrorName(r.m.error));
      return ParseResult::Ok;
    }
    case SensorType::VictronOrionXs: {
      // Orion XS (victron-ble orion_xs.py)
      uint32_t state = b.u(8), err = b.u(8), outV = b.u(16), outA = b.u(16), inV = b.u(16), inA = b.u(16);
      r.m.state = state;
      r.m.error = err != 0xFF ? err : 0;
      if (outV != 0xFFFF) r.m.volts = outV * 0.01f;
      if (outA != 0xFFFF) r.m.amps = outA * 0.1f;
      if (!isnan(r.m.volts) && !isnan(r.m.amps)) r.m.watts = r.m.volts * r.m.amps;
      if (!isnan(r.m.watts)) primary(r, "%.0f W", r.m.watts);
      else primary(r, "--");
      add(r, TN("Status"), "%s", victronStateName(state));
      if (inV != 0xFFFF) add(r, TN("Ingang"), "%.2f V", inV * 0.01f);
      if (inA != 0xFFFF) add(r, TN("Ingangsstroom"), "%.1f A", inA * 0.1f);
      if (!isnan(r.m.volts)) add(r, TN("Uitgang"), "%.2f V", r.m.volts);
      if (!isnan(r.m.amps)) add(r, TN("Laadstroom"), "%.1f A", r.m.amps);
      if (r.m.error) add(r, TN("Fout"), "%u %s", r.m.error, victronErrorName(r.m.error));
      return ParseResult::Ok;
    }
    case SensorType::VictronDcMeter: {
      // SmartShunt als DC-energiemeter (victron-ble dc_energy_meter.py)
      b.s(16);  // soort meter (lader, verbruiker, ...): niet getoond
      int32_t mv = b.s(16);
      uint32_t alarm = b.u(16), aux = b.u(16), auxMode = b.u(2);
      int32_t ma = b.s(22);
      if (mv != 0x7FFF) r.m.volts = mv * 0.01f;
      if (ma != 0x1FFFFF) r.m.amps = ma * 0.001f;  // 22 bits signed: "niet beschikbaar" = hoogste positieve
      if (!isnan(r.m.volts) && !isnan(r.m.amps)) r.m.watts = r.m.volts * r.m.amps;
      if (auxMode == 2 && aux != 0xFFFF) r.m.tempC = aux * 0.01f - 273.15f;
      r.m.alarm = alarm;
      if (!isnan(r.m.watts)) primary(r, "%.0f W", r.m.watts);
      else if (!isnan(r.m.volts)) primary(r, "%.2f V", r.m.volts);
      else primary(r, "--");
      if (!isnan(r.m.volts)) add(r, TN("Spanning"), "%.2f V", r.m.volts);
      if (!isnan(r.m.amps)) add(r, TN("Stroom"), "%.2f A", r.m.amps);
      if (auxMode == 0) add(r, TN("Startaccu"), "%.2f V", (int16_t)aux * 0.01f);
      else if (auxMode == 2 && aux != 0xFFFF) add(r, TN("Temperatuur"), "%.1f °C", aux * 0.01f - 273.15f);
      if (alarm) add(r, TN("Alarm"), "%s", victronAlarmName(alarm));
      return ParseResult::Ok;
    }
    default:
      primary(r, "--");
      add(r, TN("Recordtype"), "0x%02X", m[6]);
      return ParseResult::Ok;
  }
}

ParseResult parseRuuvi(const AdvData &adv, Reading &r) {
  if (adv.mfgLen < 2 + 18 || adv.mfg[2] != 5) return ParseResult::Unsupported;
  const uint8_t *d = adv.mfg + 2;
  int16_t t = (int16_t)be16(d + 1);
  uint16_t h = be16(d + 3);
  uint16_t p = be16(d + 5);
  uint16_t pwr = be16(d + 13);
  if (t != (int16_t)0x8000) r.m.tempC = t * 0.005f;
  if (h != 0xFFFF) r.m.humidity = h * 0.0025f;
  if (t != (int16_t)0x8000) primary(r, "%.1f °C", t * 0.005f);
  else primary(r, "--");
  if (h != 0xFFFF) add(r, TN("Vocht"), "%.0f %%", h * 0.0025f);
  if (p != 0xFFFF) {
    r.m.pressureHpa = (p + 50000) / 100.0f;
    add(r, TN("Luchtdruk"), "%.0f hPa", r.m.pressureHpa);
  }
  if ((pwr >> 5) != 0x7FF) add(r, TN("Batterij"), "%.2f V", ((pwr >> 5) + 1600) / 1000.0f);
  add(r, TN("Bewegingen"), "%u", d[15]);
  return ParseResult::Ok;
}

ParseResult parseAtc(const AdvData &adv, Reading &r) {
  const uint8_t *d = adv.svc181a;
  if (adv.svc181aLen == 13) {  // ATC1441: big-endian
    r.m.tempC = (int16_t)be16(d + 6) * 0.1f;
    r.m.humidity = d[8];
    primary(r, "%.1f °C", (int16_t)be16(d + 6) * 0.1f);
    add(r, TN("Vocht"), "%u %%", d[8]);
    add(r, TN("Batterij"), "%u %%", d[9]);
    add(r, TN("Batterijspanning"), "%.2f V", be16(d + 10) / 1000.0f);
    return ParseResult::Ok;
  }
  if (adv.svc181aLen >= 15) {  // pvvx custom: little-endian
    r.m.tempC = (int16_t)le16(d + 6) * 0.01f;
    r.m.humidity = le16(d + 8) * 0.01f;
    primary(r, "%.1f °C", (int16_t)le16(d + 6) * 0.01f);
    add(r, TN("Vocht"), "%.1f %%", le16(d + 8) * 0.01f);
    add(r, TN("Batterij"), "%u %%", d[12]);
    add(r, TN("Batterijspanning"), "%.2f V", le16(d + 10) / 1000.0f);
    return ParseResult::Ok;
  }
  return ParseResult::Unsupported;
}

struct BtHomeObj {
  uint8_t id;
  uint8_t len;
  bool isSigned;
  float factor;
  const char *label;
  const char *fmt;
};

const BtHomeObj BTHOME_OBJS[] = {
  {0x00, 1, false, 1, nullptr, nullptr},  // packet id
  {0x01, 1, false, 1, TN("Batterij"), "%.0f %%"},
  {0x02, 2, true, 0.01f, TN("Temperatuur"), "%.1f °C"},
  {0x03, 2, false, 0.01f, TN("Vocht"), "%.0f %%"},
  {0x04, 3, false, 0.01f, TN("Luchtdruk"), "%.0f hPa"},
  {0x05, 3, false, 0.01f, TN("Licht"), "%.0f lux"},
  {0x08, 2, true, 0.01f, TN("Dauwpunt"), "%.1f °C"},
  {0x0A, 3, false, 0.001f, TN("Energie"), "%.2f kWh"},
  {0x0B, 3, false, 0.01f, TN("Vermogen"), "%.0f W"},
  {0x0C, 2, false, 0.001f, TN("Spanning"), "%.2f V"},
  {0x14, 2, false, 0.01f, TN("Vochtigheid"), "%.0f %%"},
  {0x2E, 1, false, 1, TN("Vocht"), "%.0f %%"},
  {0x2F, 1, false, 1, TN("Vochtigheid"), "%.0f %%"},
  {0x3F, 2, true, 0.1f, TN("Rotatie"), "%.1f°"},
  {0x43, 2, false, 0.001f, TN("Stroom"), "%.2f A"},
  {0x45, 2, true, 0.1f, TN("Temperatuur"), "%.1f °C"},
  {0x4A, 2, false, 0.1f, TN("Spanning"), "%.1f V"},
};

ParseResult parseBtHome(const AdvData &adv, Reading &r) {
  const uint8_t *d = adv.bthome;
  size_t n = adv.bthomeLen;
  if (n < 1 || (d[0] & 0x01)) return ParseResult::Unsupported;  // versleuteld wordt niet ondersteund
  size_t i = 1;
  while (i < n) {
    uint8_t id = d[i++];
    const BtHomeObj *obj = nullptr;
    for (const auto &o : BTHOME_OBJS)
      if (o.id == id) obj = &o;
    if (!obj) {
      if (id == 0x12 || id == 0x13) { i += 2; continue; }  // CO2 en TVOC: 2 bytes, overslaan
      if (id >= 0x0F && id <= 0x2D) { i += 1; continue; }  // binaire sensoren: 1 byte, overslaan
      break;                                               // onbekend object: lengte onbekend
    }
    if (i + obj->len > n) break;
    int32_t raw = 0;
    for (int k = 0; k < obj->len; k++) raw |= (int32_t)d[i + k] << (8 * k);
    if (obj->isSigned && (raw & (1L << (8 * obj->len - 1)))) raw -= 1L << (8 * obj->len);
    i += obj->len;
    if (!obj->label) continue;
    float v = raw * obj->factor;
    if (id == 0x02 || id == 0x45) r.m.tempC = v;
    if (id == 0x03 || id == 0x2E) r.m.humidity = v;
    if (id == 0x0C || id == 0x4A) r.m.volts = v;
    if (id == 0x43) r.m.amps = v;
    if (id == 0x04) r.m.pressureHpa = v;
    if (r.primary[0] == 0) primary(r, obj->fmt, v);
    add(r, obj->label, obj->fmt, v);
  }
  return r.count ? ParseResult::Ok : ParseResult::Unsupported;
}

// ThermoBeacon (ORIA, Brifit, SensorBlue en andere merken met de app ThermoBeacon).
// Referentie: Home Assistant thermobeacon-ble (github.com/Bluetooth-Devices/thermobeacon-ble), MIT.
// Manufacturer data van 20 bytes; het "bedrijfs-id" is het apparaattype (0x10 = mini met lusje, ...).
// [10..11] batterij mV, [12..13] temperatuur /16 (signed), [14..15] vocht /16. Pakketten van 22 bytes
// bevatten geen meting (min/max-historie).
bool isThermoBeaconId(uint8_t id) {
  return id == 0x10 || id == 0x11 || id == 0x14 || id == 0x15 || id == 0x18 || id == 0x1B || id == 0x30;
}

ParseResult parseThermoBeacon(const AdvData &adv, Reading &r) {
  if (!adv.thermoBeaconSvc || adv.mfgLen != 20 || !isThermoBeaconId(adv.mfg[0]) || adv.mfg[1] != 0)
    return ParseResult::Unsupported;
  const uint8_t *d = adv.mfg;
  uint16_t mv = le16(d + 10);
  float t = (int16_t)le16(d + 12) / 16.0f;
  float h = le16(d + 14) / 16.0f;
  if (t > 100 || h > 100) return ParseResult::Unsupported;  // onzin (zoals de referentie)
  int pct = mv >= 3000 ? 100 : mv >= 2600 ? 60 + (mv - 2600) / 10 : mv >= 2500 ? 40 + (mv - 2500) / 5
          : mv >= 2450 ? 20 + (mv - 2450) * 2 / 5 : 0;
  r.m.tempC = t;
  r.m.humidity = h;
  r.m.batteryPct = pct;
  primary(r, "%.1f °C", t);
  add(r, TN("Vocht"), "%.0f %%", h);
  add(r, TN("Batterij"), "%d %%", pct);
  add(r, TN("Batterijspanning"), "%.2f V", mv / 1000.0f);
  return ParseResult::Ok;
}

// Mopeka Pro Check: referentie = ESPHome mopeka_pro_check (github.com/esphome/esphome)
// Data (10 bytes na het bedrijfs-id 0x0059): [0] sensortype, [1] batterij, [2] temperatuur + sync-bit,
// [3..4] afstand (14 bits) + kwaliteit (bits 6-7 van byte 4)
ParseResult parseMopeka(const AdvData &adv, Reading &r, uint8_t medium) {
  if (!adv.mopekaSvc || adv.mfgLen != 12) return ParseResult::Unsupported;
  const uint8_t *d = adv.mfg + 2;
  // Typen volgens Home Assistant mopeka-iot-ble. Bovenop de tank (meet door de lucht): Pro-200, TD40/TD200.
  uint8_t type = d[0];
  bool topDown = type == 0x04 || type == 0x0A || type == 0x0B || type == 0x12;
  if (!topDown && type != 0x03 && type != 0x05 && type != 0x06 && type != 0x08 && type != 0x09 && type != 0x0C)
    return ParseResult::Unsupported;

  float v = (d[1] & 0x7F) / 32.0f;  // spanning voor een CR2032
  r.m.batteryPct = fmaxf(0, fminf(100, (v - 2.2f) / 0.65f * 100));
  uint8_t tRaw = d[2] & 0x7F;
  r.m.tempC = (float)tRaw - 40;
  r.m.quality = d[4] >> 6;
  uint16_t raw = d[3] | (d[4] << 8);
  double level = raw & 0x3FFF;
  // Geluidssnelheid hangt af van wat er tussen sensor en oppervlak zit, en van de temperatuur
  // (coëfficiënten van Mopeka, via mopeka-iot-ble)
  static const double COEF[][3] = {
      {0.573045, -0.002822, -0.00000535},                  // gas (propaan/butaan)
      {0.600592, 0.003124, -0.00001368},                   // water (drinkwater, grijs-, vuilwater)
      {0.7373417462, -0.001978229885, 0.00000202162},      // diesel, benzine, olie
      {0.153096, 0.000327, -0.000000294},                  // lucht (sensor bovenop de tank)
  };
  int k = topDown ? 3
        : medium == (uint8_t)TankMedium::FreshWater || medium == (uint8_t)TankMedium::WasteWater ? 1
        : medium == (uint8_t)TankMedium::Fuel ? 2 : 0;
  const double *c = COEF[k];
  r.m.distanceMm = r.m.quality ? (float)(level * (c[0] + c[1] * tRaw + c[2] * tRaw * tRaw)) : NAN;
  if (!isnan(r.m.distanceMm)) primary(r, "%.0f mm", r.m.distanceMm);
  else primary(r, "--");
  add(r, TN("Inhoud"), "%s", tankMediumName((TankMedium)min<int>(medium, (int)TankMedium::Count)));
  add(r, TN("Batterij"), "%.0f %%", r.m.batteryPct);
  add(r, TN("Temperatuur"), "%.0f °C", r.m.tempC);
  return ParseResult::Ok;
}

}  // namespace

// Onder tegen de tank: vol = grote afstand. Bovenop de tank (TD40, Pro-200): vol = kleine afstand.
// Beide werken, zolang "vol" en "leeg" zijn afgelezen zoals de sensor ze meet.
float mopekaLevelPct(float d, float full, float empty) {
  if (isnan(d) || isnan(full) || isnan(empty) || fabsf(full - empty) < 1) return NAN;
  float pct = 100.0f * (d - empty) / (full - empty);
  return fmaxf(0, fminf(100, pct));
}

const char *tankMediumName(TankMedium m) {
  switch (m) {
    case TankMedium::FreshWater: return TN("drinkwater");
    case TankMedium::WasteWater: return TN("grijs-/vuilwater");
    case TankMedium::Fuel: return TN("diesel/benzine");
    case TankMedium::Gas: return TN("gas");
    default: return TN("gas");
  }
}

SensorType detectType(const AdvData &adv) {
  if (adv.mfgLen >= 7) {
    uint16_t company = le16(adv.mfg);
    if (company == VICTRON_ID && adv.mfg[2] == 0x10) {
      switch (adv.mfg[6]) {
        case 0x01: return SensorType::VictronSolar;
        case 0x02: return SensorType::VictronBattery;
        case 0x04: return SensorType::VictronDcDc;
        case 0x08: return SensorType::VictronAcCharger;
        case 0x05: return SensorType::VictronLithium;
        case 0x09: return SensorType::VictronBatteryProtect;
        case 0x0A: return SensorType::VictronLynx;
        case 0x03: return SensorType::VictronInverter;
        case 0x0B: return SensorType::VictronMultiRs;
        case 0x0C: return SensorType::VictronVeBus;
        case 0x0D: return SensorType::VictronDcMeter;
        case 0x0F: return SensorType::VictronOrionXs;
        default: return SensorType::VictronOther;
      }
    }
    if (company == RUUVI_ID && adv.mfg[2] == 5) return SensorType::Ruuvi;
  }
  if (adv.thermoBeaconSvc && adv.mfgLen == 20 && isThermoBeaconId(adv.mfg[0]) && adv.mfg[1] == 0)
    return SensorType::ThermoBeacon;
  if (adv.mopekaSvc && adv.mfgLen == 12 && adv.mfg[0] == 0x59 && adv.mfg[1] == 0x00) return SensorType::MopekaPro;
  if (adv.svc181a && (adv.svc181aLen == 13 || adv.svc181aLen >= 15)) return SensorType::AtcThermo;
  if (adv.bthome && adv.bthomeLen >= 2 && !(adv.bthome[0] & 0x01)) return SensorType::BTHome;
  return SensorType::Unknown;
}

bool parseVictronKey(const char *s, uint8_t out[16]) {
  int n = 0;
  for (; *s; s++) {
    char c = *s;
    if (c == ' ' || c == '-' || c == ':' || c == '\t' || c == '\r' || c == '\n') continue;
    int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    if (v < 0 || n >= 32) return false;
    if (n % 2 == 0) out[n / 2] = v << 4;
    else out[n / 2] |= v;
    n++;
  }
  return n == 32;
}

bool isVictron(SensorType t) {
  return t == SensorType::VictronBattery || t == SensorType::VictronSolar || t == SensorType::VictronLithium ||
         t == SensorType::VictronBatteryProtect || t == SensorType::VictronLynx ||
         t == SensorType::VictronDcDc || t == SensorType::VictronOther || t == SensorType::VictronAcCharger ||
         t == SensorType::VictronInverter || t == SensorType::VictronVeBus || t == SensorType::VictronMultiRs ||
         t == SensorType::VictronOrionXs || t == SensorType::VictronDcMeter;
}

const char *typeName(SensorType t) {
  switch (t) {
    case SensorType::VictronBattery: return TN("Victron accumonitor");
    case SensorType::VictronSolar: return TN("Victron zonnelader");
    case SensorType::VictronDcDc: return TN("Victron DC-DC lader");
    case SensorType::VictronOther: return TN("Victron (overig)");
    case SensorType::VictronAcCharger: return TN("Victron walstroomlader");
    case SensorType::VictronLithium: return "Victron Smart Lithium";
    case SensorType::VictronBatteryProtect: return "Victron BatteryProtect";
    case SensorType::VictronLynx: return "Victron Lynx Smart BMS";
    case SensorType::MopekaPro: return TN("Mopeka tanksensor");
    case SensorType::Ruuvi: return "RuuviTag";
    case SensorType::AtcThermo: return TN("Thermometer (ATC)");
    case SensorType::BTHome: return TN("BTHome-sensor");
    case SensorType::ThermoBeacon: return TN("Thermometer (ThermoBeacon)");
    case SensorType::VictronInverter: return TN("Victron omvormer");
    case SensorType::VictronVeBus: return "Victron MultiPlus/Quattro";
    case SensorType::VictronMultiRs: return "Victron Multi RS";
    case SensorType::VictronOrionXs: return "Victron Orion XS";
    case SensorType::VictronDcMeter: return TN("Victron DC-energiemeter");
    default: return TN("Onbekend");
  }
}

ParseResult parseReading(SensorType t, const AdvData &adv, const uint8_t *key, Reading &out, uint8_t medium) {
  memset(&out, 0, sizeof(out));
  out.m = {NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, -1, 0xFF, 0, 0, NAN, NAN, 0, NAN, NAN, NAN, 3};
  for (float &c : out.cellV) c = NAN;
  if (isVictron(t)) return parseVictron(t, adv, key, out);
  if (t == SensorType::MopekaPro) return parseMopeka(adv, out, medium);
  switch (t) {
    case SensorType::Ruuvi: return parseRuuvi(adv, out);
    case SensorType::AtcThermo: return parseAtc(adv, out);
    case SensorType::BTHome: return parseBtHome(adv, out);
    case SensorType::ThermoBeacon: return parseThermoBeacon(adv, out);
    default: return ParseResult::Unsupported;
  }
}

// Vertalen (i18n.cpp). Zonder i18n (bv. in de pc-tests) blijft de Nederlandse tekst staan.
__attribute__((weak)) const char *tr(const char *nl) { return nl; }

void reading_value_text(const Reading &r, int i, char *out, size_t len) {
  const Value &v = r.values[i];
  // Foutcode en alarm opnieuw opbouwen in de huidige taal (de tekst in Value is bij het ontvangen
  // gemaakt, en kan voor lange alarmnamen afgekapt zijn)
  if (!strcmp(v.label, "Fout") && r.m.error && strncmp(v.text, "0x", 2) != 0)
    snprintf(out, len, "%u %s", (unsigned)r.m.error, T(victronErrorName(r.m.error)));
  else if (!strcmp(v.label, "Alarm") && r.m.alarm)
    snprintf(out, len, "%s", T(victronAlarmName(r.m.alarm)));
  else
    snprintf(out, len, "%s", T(v.text));
}

