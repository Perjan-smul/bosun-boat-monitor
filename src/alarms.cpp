#include "log.h"
#include "alarms.h"
#include "psram.h"
#include "baro.h"
#include <Arduino.h>
#include <math.h>
#include <stdarg.h>
#include "buzzer.h"
#include "display.h"
#include "i18n.h"
#include "n2k.h"
#include "quarantine.h"
#include "settings.h"
#include "store.h"

namespace {

// Meldingentabellen in PSRAM (2 x 64 x ± 0,1 KB)
Alarm *tbl() {
  static Alarm *p = psram_alloc<Alarm>(MAX_ALARMS);
  return p;
}
int count = 0;
// Kopie voor andere taken (diagnosepagina); de tabel zelf wordt alleen door de UI-lus gebruikt
Alarm *pub() {
  static Alarm *p = psram_alloc<Alarm>(MAX_ALARMS);
  return p;
}
int pubCount = 0;
portMUX_TYPE pubMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t buzzerStart = 0;
uint32_t testUntil = 0;

constexpr uint32_t FRESH_S = 120;

Alarm *find(const char *mac, AlarmKind k) {
  for (int i = 0; i < count; i++)
    if (tbl()[i].kind == k && strcmp(tbl()[i].mac, mac) == 0) return &tbl()[i];
  return nullptr;
}

AlarmLevel levelOf(const char *mac, AlarmKind k) {
  Alarm *a = find(mac, k);
  return a ? a->level : AlarmLevel::None;
}

// Bestaande melding laten staan (bv. als de gegevens even niet vers zijn)
void keep(const char *mac, AlarmKind k) {
  Alarm *a = find(mac, k);
  if (a) a->seen = true;
}

// Zet het niveau van een melding. Nieuw of opgeschaald naar rood = onbevestigd en scherm aan.
void setLevel(const char *mac, const char *name, AlarmKind k, AlarmLevel level, const char *fmt = nullptr, ...) {
  Alarm *a = find(mac, k);
  if (level == AlarmLevel::None) {
    if (a) a->level = AlarmLevel::None;
    return;
  }
  if (!a) {
    if (count < MAX_ALARMS) {
      a = &tbl()[count++];
    } else {
      // Vol: een opgeheven melding hergebruiken, anders mag rood een oranje melding verdringen
      for (int i = 0; i < count && !a; i++)
        if (tbl()[i].level == AlarmLevel::None) a = &tbl()[i];
      for (int i = 0; i < count && !a && level == AlarmLevel::Alarm; i++)
        if (tbl()[i].level == AlarmLevel::Warning) a = &tbl()[i];
      if (!a) return;
    }
    memset(a, 0, sizeof(Alarm));
    strlcpy(a->mac, mac, sizeof(a->mac));
    a->kind = k;
  }
  a->seen = true;
  if (level > a->level) {
    a->acked = level != AlarmLevel::Alarm;  // alleen rood moet bevestigd worden
    display_wake();
    logf("[alarm] %s (%s)\n", name, level == AlarmLevel::Alarm ? "alarm" : "waarschuwing");
  }
  a->level = level;
  char msg[48];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), T(fmt), ap);  // opmaak in de huidige taal (de tekst wordt elke seconde opnieuw gemaakt)
  va_end(ap);
  snprintf(a->text, sizeof(a->text), "%s: %s", name, msg);
}

// Grens met hysterese, zodat een melding niet blijft aan- en uitgaan rond de grens
bool below(float value, float limit, float hyst, bool wasOn) {
  return !isnan(limit) && !isnan(value) && value < (wasOn ? limit + hyst : limit);
}
bool above(float value, float limit, float hyst, bool wasOn) {
  return !isnan(limit) && !isnan(value) && value > (wasOn ? limit - hyst : limit);
}

// Twee grenzen (oranje en rood) voor één meetwaarde
template <typename Cmp>
void twoLevel(const char *mac, const char *name, AlarmKind k, float value, float warn, float alarm, float hyst, Cmp cmp,
              const char *fmt) {
  if (isnan(value)) {  // even geen meetwaarde (bv. "niet beschikbaar"): de melding blijft zoals hij was
    keep(mac, k);
    return;
  }
  AlarmLevel was = levelOf(mac, k);
  AlarmLevel lvl = cmp(value, alarm, hyst, was == AlarmLevel::Alarm)  ? AlarmLevel::Alarm
                   : cmp(value, warn, hyst, was != AlarmLevel::None) ? AlarmLevel::Warning
                                                                      : AlarmLevel::None;
  setLevel(mac, name, k, lvl, fmt, value);
}

void evaluate(const DeviceState &d) {
  const Settings &s = settings();
  const Metrics &m = d.reading.m;
  const DeviceConfig &c = d.cfg;
  const char *mac = c.mac, *name = c.name;
  uint32_t age = d.lastSeenMs ? (millis() - d.lastSeenMs) / 1000 : UINT32_MAX;

  // Geen signaal: oranje
  bool silent = s.noSignalMin && millis() / 1000 > s.noSignalMin * 60U && age > s.noSignalMin * 60U;
  if (!silent) setLevel(mac, name, AlarmKind::NoSignal, AlarmLevel::None);
  else if (d.last == ParseResult::BadKey) setLevel(mac, name, AlarmKind::NoSignal, AlarmLevel::Warning, TN("sleutel onjuist"));
  else if (d.lastSeenMs) setLevel(mac, name, AlarmKind::NoSignal, AlarmLevel::Warning, TN("geen signaal (%lu min)"), (unsigned long)(age / 60));
  else setLevel(mac, name, AlarmKind::NoSignal, AlarmLevel::Warning, TN("nog nooit ontvangen"));

  // Meetwaarden alleen met verse gegevens; anders blijft de vorige toestand staan
  if (!d.hasData || age > FRESH_S) {
    for (AlarmKind k : {AlarmKind::SocLow, AlarmKind::VoltLow, AlarmKind::TempHigh, AlarmKind::TempLow, AlarmKind::Victron,
                        AlarmKind::ChargerError, AlarmKind::MopekaTank, AlarmKind::MopekaBatt})
      keep(mac, k);
    return;
  }

  if (d.cfg.type == SensorType::MopekaPro) {
    // Batterij van de sensor: altijd beoordeeld
    twoLevel(mac, name, AlarmKind::MopekaBatt, m.batteryPct, c.battWarn, c.battAlarm, 2, below, TN("batterij laag (%.0f%%)"));
    // Tankniveau: alleen na kalibratie. Zonder meting (kwaliteit 0) blijft de melding staan.
    float level = mopekaLevelPct(m.distanceMm, c.tankFullMm, c.tankEmptyMm);
    if (isnan(level)) keep(mac, AlarmKind::MopekaTank);
    else if (tankAlarmWhenFull(c.tankMedium))  // vuilwater: melding als hij vol raakt
      twoLevel(mac, name, AlarmKind::MopekaTank, level, c.tankWarn, c.tankAlarm, 2, above, TN("tank bijna vol (%.0f%%)"));
    else twoLevel(mac, name, AlarmKind::MopekaTank, level, c.tankWarn, c.tankAlarm, 2, below, TN("tank bijna leeg (%.0f%%)"));
    return;  // Mopeka heeft geen Victron- of temperatuurmeldingen
  }

  twoLevel(mac, name, AlarmKind::SocLow, m.soc, c.socWarn, c.socLow, 2, below, TN("laadtoestand laag (%.0f%%)"));
  twoLevel(mac, name, AlarmKind::VoltLow, m.volts, c.voltWarn, c.voltLow, 0.2f, below, TN("spanning laag (%.2f V)"));
  twoLevel(mac, name, AlarmKind::TempHigh, m.tempC, c.tempHighWarn, c.tempHigh, 0.5f, above, TN("te warm (%.1f °C)"));
  twoLevel(mac, name, AlarmKind::TempLow, m.tempC, c.tempLowWarn, c.tempLow, 0.5f, below, TN("te koud (%.1f °C)"));

  // Wat Victron zelf meldt: alarmbits = rood, laderfout = oranje
  setLevel(mac, name, AlarmKind::Victron, s.victronAlarms && m.alarm ? AlarmLevel::Alarm : AlarmLevel::None, "%s",
           T(victronAlarmName(m.alarm)));
  setLevel(mac, name, AlarmKind::ChargerError, s.victronAlarms && m.error ? AlarmLevel::Warning : AlarmLevel::None,
           TN("laderfout %u (%s)"), m.error, T(victronErrorName(m.error)));
}

// NMEA 2000: diepte, tanks en motor. Alleen als de hoofdschakelaar in de instellingen aan staat.
void evaluateN2k() {
  const Settings &s = settings();
  if (!s.n2kAlarms) return;  // niet beoordeeld = bestaande meldingen worden opgeruimd
  static N2kState n;         // ± 1,5 KB, niet op de stack
  n2k_get(n);
  uint32_t now = millis();
  auto fresh = [&](uint32_t ms, uint32_t maxAge) { return ms && now - ms < maxAge; };
  char mac[18], name[32];

  if (fresh(n.depthMs, 10000))
    twoLevel("n2k:diepte", T("Diepte"), AlarmKind::Depth, n.depth, s.depthWarn, s.depthAlarm, 0.2f, below, TN("ondiep (%.1f m)"));
  else keep("n2k:diepte", AlarmKind::Depth);

  for (auto &t : n.tanks) {
    if (!t.ms) continue;
    snprintf(mac, sizeof(mac), "n2k:tank:%u:%u", t.type, t.instance);
    n2kTankLabel(n, t, name, sizeof(name));
    bool waste = t.type == 2 || t.type == 5;
    AlarmKind k = waste ? AlarmKind::TankHigh : AlarmKind::TankLow;
    if (!fresh(t.ms, 30000) || (t.type != 0 && t.type != 1 && !waste)) {
      keep(mac, k);
      continue;
    }
    if (t.type == 0) twoLevel(mac, name, k, t.levelPct, s.fuelWarn, s.fuelAlarm, 2, below, TN("bijna leeg (%.0f%%)"));
    else if (t.type == 1) twoLevel(mac, name, k, t.levelPct, s.waterWarn, s.waterAlarm, 2, below, TN("bijna leeg (%.0f%%)"));
    else twoLevel(mac, name, k, t.levelPct, s.wasteWarn, s.wasteAlarm, 2, above, TN("bijna vol (%.0f%%)"));
  }

  for (auto &e : n.engine) {
    if (!e.rapidMs && !e.dynMs) continue;
    snprintf(mac, sizeof(mac), "n2k:motor:%u", e.instance);
    n2kEngineLabel(n, e, name, sizeof(name));
    if (fresh(e.dynMs, 10000)) {
      twoLevel(mac, name, AlarmKind::Coolant, e.coolantC, s.coolantWarn, s.coolantAlarm, 2, above, TN("koelwater te heet (%.0f °C)"));
      // Oliedruk alleen als de motor draait; uit = melding weg
      bool running = fresh(e.rapidMs, 10000) && e.rpm > 400;
      if (running) twoLevel(mac, name, AlarmKind::OilPressure, e.oilBar, s.oilWarn, s.oilAlarm, 0.1f, below, TN("oliedruk laag (%.1f bar)"));
      else setLevel(mac, name, AlarmKind::OilPressure, AlarmLevel::None);
    } else {
      keep(mac, AlarmKind::Coolant);
      keep(mac, AlarmKind::OilPressure);
    }
  }
}

// Twee korte piepjes, dan stilte; herhaalt tot het alarm bevestigd is
void updateBuzzer() {
  uint32_t now = millis();
  bool want = (alarms_unacked() > 0 && settings().buzzer) || now < testUntil;
  if (!want) {
    buzzerStart = 0;
    buzzer_set(false);
    return;
  }
  if (!buzzerStart) buzzerStart = now;
  uint32_t t = (now - buzzerStart) % 2500;
  buzzer_set(t < 150 || (t >= 300 && t < 450));
}

// Stormwaarschuwing: de luchtdruk is in 3 uur meer dan de grens gedaald
void evaluateBaro() {
  const Settings &s = settings();
  BaroInfo b = baro_info();
  float drop = isnan(b.diff3h) ? NAN : -b.diff3h;
  twoLevel("baro", T("Barometer"), AlarmKind::Storm, drop, s.stormWarn, s.stormAlarm, 1.0f, above,
           TN("storm? %.1f hPa gedaald in 3 uur"));
}

}  // namespace

void alarms_tick() {
  static uint32_t lastEval = 0;
  if (millis() - lastEval >= 1000) {
    lastEval = millis();
    for (int k = 0; k < count; k++) tbl()[k].seen = false;
    int n = store_count();
    for (int i = 0; i < n; i++) {
      DeviceState d;
      if (store_get(i, d)) evaluate(d);
    }
    evaluateN2k();
    evaluateBaro();
    // Apparaten die uitgeschakeld zijn omdat het board erdoor crashte: oranje, tot je ze weer inschakelt
    QEntry q[Q_MAX];
    int nq = quarantine_list(q, Q_MAX);
    for (int i = 0; i < nq; i++) {
      char key[18], desc[64];
      if (q[i].kind == QKind::Ble) strlcpy(key, q[i].mac, sizeof(key));
      else snprintf(key, sizeof(key), "q:%lu:%u", (unsigned long)q[i].pgn, q[i].src);
      quarantine_describe(q[i], desc, sizeof(desc));
      setLevel(key, desc, AlarmKind::Disabled, AlarmLevel::Warning, TN("uitgeschakeld na crashes (Instellingen)"));
    }
    // Opgeloste meldingen, verwijderde apparaten en uitgeschakelde alarmen opruimen
    for (int k = count - 1; k >= 0; k--) {
      if (tbl()[k].seen && tbl()[k].level != AlarmLevel::None) continue;
      memmove(&tbl()[k], &tbl()[k + 1], (count - k - 1) * sizeof(Alarm));
      count--;
    }
  }
  display_keep_awake(alarms_unacked() > 0);
  updateBuzzer();
  portENTER_CRITICAL(&pubMux);
  memcpy(pub(), tbl(), count * sizeof(Alarm));
  pubCount = count;
  portEXIT_CRITICAL(&pubMux);
}

int alarms_snapshot(Alarm *out, int max) {
  portENTER_CRITICAL(&pubMux);
  int n = 0;
  for (AlarmLevel lvl : {AlarmLevel::Alarm, AlarmLevel::Warning})
    for (int i = 0; i < pubCount && n < max; i++)
      if (pub()[i].level == lvl) out[n++] = pub()[i];
  portEXIT_CRITICAL(&pubMux);
  return n;
}

int alarms_active(Alarm *out, int max) {
  int n = 0;
  for (AlarmLevel lvl : {AlarmLevel::Alarm, AlarmLevel::Warning})
    for (int i = 0; i < count && n < max; i++)
      if (tbl()[i].level == lvl) out[n++] = tbl()[i];
  return n;
}

int alarms_unacked() {
  int n = 0;
  for (int i = 0; i < count; i++)
    if (tbl()[i].level == AlarmLevel::Alarm && !tbl()[i].acked) n++;
  return n;
}

AlarmLevel alarms_highest() {
  AlarmLevel h = AlarmLevel::None;
  for (int i = 0; i < count; i++)
    if (tbl()[i].level > h) h = tbl()[i].level;
  return h;
}

AlarmLevel alarms_for_device(const char *mac) {
  AlarmLevel h = AlarmLevel::None;
  for (int i = 0; i < count; i++)
    if (tbl()[i].level > h && strcmp(tbl()[i].mac, mac) == 0) h = tbl()[i].level;
  return h;
}

void alarms_ack_all() {
  for (int i = 0; i < count; i++) tbl()[i].acked = true;
}

void alarms_test_buzzer() { testUntil = millis() + 2500; }
