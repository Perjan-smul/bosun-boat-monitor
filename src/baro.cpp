#include "baro.h"
#include <math.h>
#include <string.h>
#include <initializer_list>
#include "i18n.h"

// ---------- tendens ----------

BaroTendency baro_tendency(float d) {
  if (isnan(d)) return BaroTendency::Unknown;
  float a = fabsf(d);
  if (a < 0.5f) return BaroTendency::Steady;
  bool up = d > 0;
  if (a < 1.6f) return up ? BaroTendency::RisingSlow : BaroTendency::FallingSlow;
  if (a < 3.6f) return up ? BaroTendency::Rising : BaroTendency::Falling;
  if (a < 6.1f) return up ? BaroTendency::RisingFast : BaroTendency::FallingFast;
  return up ? BaroTendency::RisingVeryFast : BaroTendency::FallingVeryFast;
}

const char *baro_tendency_text(BaroTendency t) {
  switch (t) {
    case BaroTendency::Steady: return TN("stabiel");
    case BaroTendency::RisingSlow: return TN("stijgt langzaam");
    case BaroTendency::Rising: return TN("stijgt");
    case BaroTendency::RisingFast: return TN("stijgt snel");
    case BaroTendency::RisingVeryFast: return TN("stijgt zeer snel");
    case BaroTendency::FallingSlow: return TN("daalt langzaam");
    case BaroTendency::Falling: return TN("daalt");
    case BaroTendency::FallingFast: return TN("daalt snel");
    case BaroTendency::FallingVeryFast: return TN("daalt zeer snel");
    default: return TN("trend na 3 uur meten");
  }
}

// ---------- historie ----------

namespace {
constexpr uint32_t MAGIC = 0xBA200001;
constexpr int SLOTS_3H = 3 * 3600 / BARO_SLOT_S;  // 18
}  // namespace

void BaroHistory::clear() {
  for (auto &x : v_) x = BARO_NONE;
  head_ = BARO_N - 1;
  slot_ = 0;
  sum_ = 0;
  cnt_ = 0;
}

int16_t BaroHistory::at(int ago) const {
  if (ago < 0 || ago >= BARO_N) return BARO_NONE;
  return v_[(head_ - ago + BARO_N) % BARO_N];
}

void BaroHistory::add(uint32_t epoch, float hPa) {
  if (isnan(hPa)) return;
  uint32_t s = epoch / BARO_SLOT_S;
  if (slot_ && s + 1 < slot_) clear();  // klok een stuk terug gezet: opnieuw beginnen
  if (!slot_ || s < slot_) {
    if (!slot_) {
      slot_ = s;
      sum_ = 0;
      cnt_ = 0;
    }
    if (s < slot_) return;  // één tijdvak terug (klok iets verzet): overslaan
  }
  if (s > slot_) {
    // Lopend tijdvak afsluiten, ontbrekende tijdvakken leeg laten
    head_ = (head_ + 1) % BARO_N;
    v_[head_] = cnt_ ? (int16_t)lround(sum_ / cnt_ * 10) : BARO_NONE;
    uint32_t gap = s - slot_ - 1;
    for (uint32_t k = 0; k < gap && k < (uint32_t)BARO_N; k++) {
      head_ = (head_ + 1) % BARO_N;
      v_[head_] = BARO_NONE;
    }
    slot_ = s;
    sum_ = 0;
    cnt_ = 0;
  }
  sum_ += hPa;
  cnt_++;
}

float BaroHistory::current() const {
  if (cnt_) return (float)(sum_ / cnt_);
  int16_t x = at(0);
  return x == BARO_NONE ? NAN : x / 10.0f;
}

float BaroHistory::diff3h() const {
  float now = current();
  if (isnan(now)) return NAN;
  // 3 uur terug = 18 tijdvakken voor het lopende; at(0) is het vorige tijdvak. Een gat van één tijdvak opvangen.
  for (int ago : {SLOTS_3H - 1, SLOTS_3H - 2, SLOTS_3H}) {
    int16_t x = at(ago);
    if (x != BARO_NONE) return now - x / 10.0f;
  }
  return NAN;
}

int BaroHistory::series(int16_t *out, int max) const {
  int n = max < BARO_N ? max : BARO_N;
  // Laatste punt = het lopende tijdvak (zo loopt de grafiek tot nu)
  float cur = cnt_ ? (float)(sum_ / cnt_) : NAN;
  for (int i = 0; i < n; i++) {
    int ago = n - 2 - i;  // i = n-1 is het lopende tijdvak
    out[i] = ago >= 0 ? at(ago) : (isnan(cur) ? BARO_NONE : (int16_t)lround(cur * 10));
  }
  return n;
}

void BaroHistory::save(Saved &out, uint8_t source) const {
  out.magic = MAGIC;
  out.slot = slot_;
  out.source = source;
  for (int i = 0; i < BARO_N; i++) out.v[i] = at(BARO_N - 1 - i);  // oudste eerst
}

bool BaroHistory::load(const Saved &in) {
  if (in.magic != MAGIC || !in.slot) return false;
  for (int i = 0; i < BARO_N; i++) v_[i] = in.v[i];
  head_ = BARO_N - 1;  // at(0) = in.v[N-1]
  slot_ = in.slot;
  sum_ = 0;
  cnt_ = 0;
  return true;
}

// ---------- hardware en bronkeuze (niet in de pc-test van de historie) ----------
#ifndef BARO_HISTORY_ONLY

#include <Arduino.h>
#include <Preferences.h>
#include "bme280.h"
#include "clock.h"
#include "log.h"
#include "n2k.h"
#include "store.h"

namespace {

constexpr uint32_t READ_MS = 30000;          // elke 30 s een meting
constexpr uint32_t SAVE_MS = 1800000;        // historie elke 30 min bewaren
constexpr uint32_t BLE_FRESH_MS = 600000;    // Bluetooth-sensor: meting van de laatste 10 min
constexpr uint32_t N2K_FRESH_MS = 60000;
constexpr uint32_t BME_FRESH_MS = 120000;

Bme280 bme;
BaroHistory hist;
BaroSource src = BaroSource::None, histSrc = BaroSource::None;
char srcName[28] = "";
uint32_t lastRead = 0, lastSave = 0, lastBmeOk = 0, version = 0, lastSlot = 0;
bool available = false;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
BaroInfo info{BaroSource::None, "", NAN, NAN, BaroTendency::Unknown};

bool sane(float hPa) { return !isnan(hPa) && hPa > 870 && hPa < 1085; }  // records op aarde: 870 en 1084 hPa

void save() {
  static BaroHistory::Saved s;
  hist.save(s, (uint8_t)histSrc);
  Preferences p;
  p.begin("baro", false);
  p.putBytes("hist", &s, sizeof(s));
  p.end();
}

}  // namespace

void baro_begin() {
  hist.clear();
  if (bme.begin()) log_line(bme.isBmp() ? "[baro] BMP280 gevonden" : "[baro] BME280 gevonden");
  static BaroHistory::Saved s;
  Preferences p;
  p.begin("baro", true);
  if (p.getBytesLength("hist") == sizeof(s) && p.getBytes("hist", &s, sizeof(s)) == sizeof(s) && hist.load(s)) {
    histSrc = (BaroSource)s.source;
    for (int16_t x : s.v) available |= x != BARO_NONE;
  }
  p.end();
}

void baro_tick() {
  uint32_t now = millis();
  if (lastRead && now - lastRead < READ_MS) return;
  lastRead = now;

  // Mogelijke bronnen
  float vBme = NAN, vN2k = NAN, vBle = NAN, t;
  char bleName[28] = "";
  if (bme.present() && bme.read(vBme, t) && sane(vBme)) lastBmeOk = now;
  else vBme = NAN;
  if (!bme.present() || now - lastBmeOk > BME_FRESH_MS) vBme = NAN;
  static N2kState ns;
  n2k_get(ns);
  if (ns.pressureMs && now - ns.pressureMs < N2K_FRESH_MS && sane(ns.pressureHpa)) vN2k = ns.pressureHpa;
  DeviceState d;
  for (int i = 0; store_get(i, d); i++) {
    if (d.hasData && d.lastSeenMs && now - d.lastSeenMs < BLE_FRESH_MS && sane(d.reading.m.pressureHpa)) {
      vBle = d.reading.m.pressureHpa;
      strlcpy(bleName, d.cfg.name, sizeof(bleName));
      break;
    }
  }
  auto valueOf = [&](BaroSource s) {
    return s == BaroSource::Bme280 ? vBme : s == BaroSource::N2k ? vN2k : s == BaroSource::Ble ? vBle : NAN;
  };

  // Bij de huidige bron blijven zolang die meet; anders de eerste die er is
  BaroSource pick = src;
  if (isnan(valueOf(pick))) {
    pick = BaroSource::None;
    for (BaroSource s : {BaroSource::Bme280, BaroSource::N2k, BaroSource::Ble})
      if (pick == BaroSource::None && !isnan(valueOf(s))) pick = s;
  }
  float v = valueOf(pick);
  if (pick != src && pick != BaroSource::None)
    logf("[baro] bron: %s\n", pick == BaroSource::Bme280 ? "BME280" : pick == BaroSource::N2k ? "NMEA 2000" : bleName);
  if (pick != BaroSource::None) src = pick;

  if (!isnan(v)) {
    available = true;
    // Andere bron dan de opgeslagen historie: sensoren verschillen onderling een paar hPa, dus opnieuw beginnen
    if (histSrc != BaroSource::None && histSrc != pick) {
      log_line("[baro] andere bron: historie opnieuw begonnen");
      hist.clear();
    }
    histSrc = pick;
    if (clock_valid()) hist.add(clock_now(), v);
  }

  BaroInfo n{};
  n.source = isnan(v) ? BaroSource::None : pick;
  strlcpy(n.sourceName, pick == BaroSource::Bme280 ? (bme.isBmp() ? "BMP280" : "BME280") : pick == BaroSource::N2k ? "NMEA 2000" : bleName,
          sizeof(n.sourceName));
  n.hPa = isnan(v) ? NAN : (clock_valid() ? hist.current() : v);
  n.diff3h = isnan(v) ? NAN : hist.diff3h();
  n.tendency = baro_tendency(n.diff3h);
  portENTER_CRITICAL(&mux);
  info = n;
  portEXIT_CRITICAL(&mux);

  if (hist.lastSlot() != lastSlot) {
    lastSlot = hist.lastSlot();
    version++;
  }
  if (clock_valid() && hist.lastSlot() && (!lastSave || now - lastSave >= SAVE_MS)) {
    lastSave = now;
    save();
  }
}

BaroInfo baro_info() {
  portENTER_CRITICAL(&mux);
  BaroInfo i = info;
  portEXIT_CRITICAL(&mux);
  return i;
}

int baro_series(int16_t *out, int max) { return hist.series(out, max); }
uint32_t baro_version() { return version; }
bool baro_available() { return available; }

#endif
