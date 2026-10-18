#include "datalog.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "i18n.h"
#include "sensors.h"

namespace {

constexpr uint32_t BLE_FRESH_MS = 300000;  // Bluetooth: metingen van de laatste 5 minuten
constexpr uint32_t N2K_FRESH_MS = 30000;   // NMEA 2000: van de laatste 30 seconden

bool fresh(uint32_t ms, uint32_t now, uint32_t maxAge) { return ms && now - ms < maxAge; }

// Puntkomma en aanhalingsteken horen niet in een kolomkop (scheidingsteken van de CSV)
void clean(char *s) {
  for (; *s; s++)
    if (*s == ';' || *s == '"' || *s == '\n' || *s == '\r') *s = ' ';
}

struct Out {
  Out(DlValue *o, int m) : out(o), max(m) {}
  DlValue *out;
  int max, n = 0;
  void add(const char *who, const char *what, float v, uint8_t dec) {
    if (isnan(v) || isinf(v) || n >= max) return;
    DlValue &d = out[n++];
    snprintf(d.name, sizeof(d.name), "%s %s", who, what);
    clean(d.name);
    d.v = v;
    d.decimals = dec;
  }
};

constexpr float KN = 1.943844f;  // m/s naar knopen

}  // namespace

int datalog_collect(const DeviceState *devs, int nDevs, const N2kState &s, uint32_t now, DlValue *out, int max,
                    float baroHpa) {
  Out o(out, max);
  o.add(T("Barometer"), "hPa", baroHpa, 1);
  for (int i = 0; i < nDevs; i++) {
    const DeviceState &d = devs[i];
    if (!d.hasData || !fresh(d.lastSeenMs, now, BLE_FRESH_MS)) continue;
    const Metrics &m = d.reading.m;
    const char *who = d.cfg.name;
    o.add(who, "V", m.volts, 2);
    o.add(who, "A", m.amps, 2);
    o.add(who, "W", m.watts, 0);
    o.add(who, "SoC %", m.soc, 1);
    o.add(who, "PV W", m.pvWatts, 0);
    o.add(who, T("kWh vandaag"), m.yieldKwh, 2);
    o.add(who, T("load A"), m.loadAmps, 2);
    o.add(who, "°C", m.tempC, 1);
    o.add(who, T("vocht %"), m.humidity, 0);
    if (m.ttgMin >= 0) o.add(who, T("resterend min"), (float)m.ttgMin, 0);
    if (d.cfg.type == SensorType::MopekaPro) {
      o.add(who, T("afstand mm"), m.distanceMm, 0);
      o.add(who, T("niveau %"), mopekaLevelPct(m.distanceMm, d.cfg.tankFullMm, d.cfg.tankEmptyMm), 0);
      o.add(who, T("batterij %"), m.batteryPct, 0);
    }
  }

  const char *nav = "NMEA";
  if (fresh(s.depthMs, now, N2K_FRESH_MS)) o.add(nav, T("diepte m"), s.depth, 1);
  if (fresh(s.cogSogMs, now, N2K_FRESH_MS)) {
    o.add(nav, "SOG kn", s.sog * KN, 1);
    o.add(nav, "COG °", s.cog, 0);
  }
  if (fresh(s.stwMs, now, N2K_FRESH_MS)) o.add(nav, "STW kn", s.stw * KN, 1);
  if (fresh(s.headingMs, now, N2K_FRESH_MS)) o.add(nav, T("koers °"), s.heading, 0);
  if (fresh(s.appWindMs, now, N2K_FRESH_MS)) {
    o.add(nav, "AWS kn", s.aws * KN, 1);
    o.add(nav, "AWA °", s.awa, 0);
  }
  if (fresh(s.trueWindMs, now, N2K_FRESH_MS)) o.add(nav, "TWS kn", s.tws * KN, 1);
  if (fresh(s.posMs, now, N2K_FRESH_MS)) {
    o.add(nav, "lat", (float)s.lat, 5);
    o.add(nav, "lon", (float)s.lon, 5);
  }
  char label[32];
  for (const N2kEngine &e : s.engine) {
    if (!fresh(e.rapidMs, now, N2K_FRESH_MS) && !fresh(e.dynMs, now, N2K_FRESH_MS)) continue;
    n2kEngineLabel(s, e, label, sizeof(label));
    o.add(label, T("tpm"), e.rpm, 0);
    o.add(label, T("koelwater °C"), e.coolantC, 0);
    o.add(label, T("oliedruk bar"), e.oilBar, 1);
  }
  for (const N2kTank &t : s.tanks) {
    if (!fresh(t.ms, now, N2K_FRESH_MS)) continue;
    n2kTankLabel(s, t, label, sizeof(label));
    o.add(label, "%", t.levelPct, 0);
  }
  for (const N2kTemp &t : s.temps)
    if (fresh(t.ms, now, N2K_FRESH_MS) && t.source == 0) o.add(T("Zeewater"), "°C", t.c, 1);
  if (fresh(s.rudderMs, now, N2K_FRESH_MS)) o.add(nav, T("roer °"), s.rudder, 0);
  if (fresh(s.attitudeMs, now, N2K_FRESH_MS)) {
    o.add(nav, T("helling °"), s.roll, 0);
    o.add(nav, T("stampen °"), s.pitch, 0);
  }
  for (const N2kBattery &b : s.batts) {
    bool ok = fresh(b.ms, now, N2K_FRESH_MS), okDet = fresh(b.detailMs, now, N2K_FRESH_MS);
    if (!ok && !okDet) continue;
    snprintf(label, sizeof(label), T("Accu %u (NMEA 2000)"), b.instance);
    if (ok) {
      o.add(label, "V", b.volts, 2);
      o.add(label, "A", b.amps, 1);
    }
    if (okDet) o.add(label, "SoC %", b.soc, 0);
  }
  for (const N2kHumidity &h : s.humids)
    if (fresh(h.ms, now, N2K_FRESH_MS)) o.add(nav, h.source == 1 ? T("vocht buiten %") : T("vocht binnen %"), h.pct, 0);
  return o.n;
}

int DataLog::find(const char *name) const {
  for (int i = 0; i < count_; i++)
    if (strcmp(names_[i], name) == 0) return i;
  return -1;
}

void DataLog::loadHeader(const char *line) {
  count_ = 0;
  const char *p = strchr(line, ';');  // eerste kolom is de tijd
  while (p && count_ < DL_MAX_COLS) {
    p++;
    const char *e = p;
    while (*e && *e != ';' && *e != '\r' && *e != '\n') e++;
    size_t n = e - p;
    if (n >= DL_NAME_LEN) n = DL_NAME_LEN - 1;
    memcpy(names_[count_], p, n);
    names_[count_][n] = 0;
    count_++;
    p = *e == ';' ? e : nullptr;
  }
}

bool DataLog::addColumns(const DlValue *vals, int n) {
  bool added = false;
  for (int i = 0; i < n && count_ < DL_MAX_COLS; i++) {
    if (find(vals[i].name) >= 0) continue;
    strncpy(names_[count_], vals[i].name, DL_NAME_LEN - 1);
    names_[count_][DL_NAME_LEN - 1] = 0;
    count_++;
    added = true;
  }
  return added;
}

void DataLog::header(char *out, size_t len, const char *timeTitle) const {
  size_t n = snprintf(out, len, "%s", timeTitle);
  for (int i = 0; i < count_ && n < len; i++) n += snprintf(out + n, len - n, ";%s", names_[i]);
  if (n < len) snprintf(out + n, len - n, "\r\n");
}

void DataLog::row(char *out, size_t len, const char *timeText, const DlValue *vals, int n, bool decimalComma) const {
  size_t pos = snprintf(out, len, "%s", timeText);
  for (int c = 0; c < count_ && pos + 1 < len; c++) {
    out[pos++] = ';';
    out[pos] = 0;
    for (int i = 0; i < n; i++) {
      if (strcmp(vals[i].name, names_[c]) != 0) continue;
      char num[24];
      snprintf(num, sizeof(num), "%.*f", vals[i].decimals, vals[i].v);
      if (decimalComma)
        for (char *q = num; *q; q++)
          if (*q == '.') *q = ',';
      pos += snprintf(out + pos, len - pos, "%s", num);
      break;
    }
  }
  if (pos < len) snprintf(out + pos, len - pos, "\r\n");
}
