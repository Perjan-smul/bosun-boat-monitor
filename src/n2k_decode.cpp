#include "n2k_decode.h"
#include "i18n.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr float RAD2DEG = 57.29577951f;
constexpr float KELVIN = 273.15f;

uint16_t u16(const uint8_t *p) { return p[0] | (p[1] << 8); }
int16_t i16(const uint8_t *p) { return (int16_t)u16(p); }
uint32_t u24(const uint8_t *p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16); }
uint32_t u32(const uint8_t *p) { return u24(p) | ((uint32_t)p[3] << 24); }
int32_t i32(const uint8_t *p) { return (int32_t)u32(p); }
int64_t i64(const uint8_t *p) { return (int64_t)((uint64_t)u32(p) | ((uint64_t)u32(p + 4) << 32)); }

// "Niet beschikbaar"-waarden (de bovenste waarden van elk bereik zijn gereserveerd)
float fu16(const uint8_t *p, float res) { uint16_t v = u16(p); return v >= 0xFFFD ? NAN : v * res; }
float fi16(const uint8_t *p, float res) { int16_t v = i16(p); return v >= 0x7FFD ? NAN : v * res; }
float fu32(const uint8_t *p, float res) { uint32_t v = u32(p); return v >= 0xFFFFFFFD ? NAN : v * res; }

float degNorm(float d) { return isnan(d) ? NAN : fmodf(d + 360.0f, 360.0f); }

// Positie binnen het bereik (een verminkte boodschap kan bv. lengte -198 geven)
bool validPos(double lat, double lon) { return fabs(lat) <= 90 && fabs(lon) <= 180; }

}  // namespace

uint32_t n2kPgnFromId(uint32_t id) {
  uint32_t pgn = (id >> 8) & 0x3FFFF;
  uint8_t pf = (pgn >> 8) & 0xFF;
  if (pf < 240) pgn &= 0x3FF00;  // PDU1: onderste byte is het bestemmingsadres
  return pgn;
}

bool n2kIsFastPacket(uint32_t pgn) { return pgn == 127489 || pgn == 129029 || pgn == 127506; }

N2kDecoder::N2kDecoder() { reset(); }

// Plek voor een accu op instance; nieuwe accu's in een lege plek (vol = niet bijhouden)
N2kBattery *N2kDecoder::battSlot(uint8_t instance, uint32_t nowMs) {
  (void)nowMs;
  for (auto &b : state.batts)
    if ((b.ms || b.detailMs) && b.instance == instance) return &b;
  for (auto &b : state.batts)
    if (!b.ms && !b.detailMs) {
      b.instance = instance;
      return &b;
    }
  return nullptr;
}

void N2kDecoder::reset() {
  memset(&state, 0, sizeof(state));
  state.lat = state.lon = NAN;
  state.sog = state.cog = state.heading = state.stw = state.depth = state.depthOffset = NAN;
  state.aws = state.awa = state.tws = state.twa = NAN;
  state.pressureHpa = NAN;
  state.rudder = state.roll = state.pitch = NAN;
  for (auto &b : state.batts) {
    b.volts = b.amps = b.tempC = b.soc = NAN;
    b.ttgMin = -1;
  }
  for (auto &e : state.engine) {
    e.rpm = e.coolantC = e.oilTempC = e.oilBar = e.altV = e.fuelLph = e.hours = NAN;
    e.loadPct = -128;
  }
  memset(fp_, 0, sizeof(fp_));
  memset(seenSrc_, 0, sizeof(seenSrc_));
  offsetCand_ = 0;
  offsetCandN_ = 0;
}

void N2kDecoder::onFrame(uint32_t id, const uint8_t *data, uint8_t len, uint32_t nowMs) {
  uint32_t pgn = n2kPgnFromId(id);
  uint8_t src = id & 0xFF;
  state.frames++;
  if (!(seenSrc_[src >> 3] & (1 << (src & 7)))) {
    seenSrc_[src >> 3] |= 1 << (src & 7);
    state.sources++;
  }
  if (n2kIsFastPacket(pgn)) onFastFrame(pgn, src, data, len, nowMs);
  else onMessage(pgn, src, data, len, nowMs);
}

// Fast packet: frame 0 = [volgnr|0][totale lengte][6 bytes], frame n = [volgnr|n][7 bytes]
void N2kDecoder::onFastFrame(uint32_t pgn, uint8_t src, const uint8_t *d, uint8_t len, uint32_t nowMs) {
  if (len < 2) return;
  uint8_t seq = d[0] >> 5, frame = d[0] & 0x1F;
  FastPacket *p = nullptr;
  for (auto &f : fp_)
    if (f.used && f.pgn == pgn && f.src == src && f.seq == seq) p = &f;

  if (frame == 0) {
    if (!p) {  // vrij of oudste slot
      p = &fp_[0];
      for (auto &f : fp_) {
        if (!f.used) { p = &f; break; }
        if (f.ms < p->ms) p = &f;
      }
    }
    p->used = true;
    p->pgn = pgn;
    p->src = src;
    p->seq = seq;
    p->total = d[1];
    p->received = 0;
    p->next = 1;
    p->ms = nowMs;
    for (int i = 2; i < len; i++) {
      if (p->received < sizeof(p->buf)) p->buf[p->received] = d[i];
      p->received++;
    }
  } else {
    if (!p || frame != p->next) {  // frame gemist: deze boodschap weggooien
      if (p) p->used = false;
      return;
    }
    p->next++;
    p->ms = nowMs;
    for (int i = 1; i < len; i++) {
      if (p->received < sizeof(p->buf)) p->buf[p->received] = d[i];
      p->received++;
    }
  }
  if (p->received >= p->total) {
    int n = p->total < sizeof(p->buf) ? p->total : sizeof(p->buf);
    p->used = false;
    onMessage(pgn, src, p->buf, n, nowMs);
  }
}

// Motoren krijgen een plek op volgorde van binnenkomst, ongeacht hun nummer
N2kEngine *N2kDecoder::engineSlot(uint8_t instance, uint32_t) {
  for (auto &e : state.engine)
    if ((e.rapidMs || e.dynMs) && e.instance == instance) return &e;
  for (auto &e : state.engine)
    if (!e.rapidMs && !e.dynMs) {
      e.instance = instance;
      return &e;
    }
  return nullptr;
}

void N2kDecoder::onMessage(uint32_t pgn, uint8_t src, const uint8_t *d, int len, uint32_t now) {
  N2kState &s = state;
  switch (pgn) {
    case 126992: {  // systeemtijd
      if (len < 8 || u16(d + 2) >= 0xFFFD || u32(d + 4) >= 0xFFFFFFFD) break;
      s.gpsEpoch = u16(d + 2) * 86400UL + u32(d + 4) / 10000;
      s.gpsTimeMs = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 130310:    // omgeving (oud): luchtdruk in bytes 5-6, per 100 Pa
    case 130311: {  // omgeving: luchtdruk in bytes 6-7, per 100 Pa
      int at = pgn == 130310 ? 5 : 6;
      if (len < at + 2 || u16(d + at) >= 0xFFFD) break;
      if (s.pressurePgn == 130314 && now - s.pressureMs < 60000) break;  // fijnere bron aanwezig
      s.pressureHpa = u16(d + at);  // 100 Pa = 1 hPa
      s.pressureMs = now;
      s.pressurePgn = pgn;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 130314: {  // werkelijke druk; bron 0 = atmosferisch
      if (len < 7 || d[2] != 0 || i32(d + 3) >= 0x7FFFFFFD) break;
      s.pressureHpa = i32(d + 3) * 0.001f;  // 0,1 Pa -> hPa
      s.pressureMs = now;
      s.pressurePgn = pgn;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 129033: {  // tijd en datum met het lokale verschil met UTC (meestal van de kaartplotter)
      // Systemen verschillen: niet elk apparaat vult het verschil in, sommige sturen het zonder geldige tijd,
      // en er kunnen meerdere afzenders zijn. Daarom streng: alleen met geldige datum en tijd, een verschil
      // in hele kwartieren binnen ±14 uur, en twee keer dezelfde waarde na elkaar.
      if (len < 8 || u16(d) >= 0xFFFD || u32(d + 2) >= 0xFFFFFFFD) break;
      int16_t ofs = i16(d + 6);
      if (ofs >= 0x7FFD || ofs < -14 * 60 || ofs > 14 * 60 || ofs % 15) break;
      // Meerdere afzenders: bij de huidige blijven zolang die zendt, anders wisselt de tijd heen en weer
      if (s.localOffsetMs && src != s.localOffsetSrc && now - s.localOffsetMs < 30000) break;
      if (offsetCandN_ == 0 || ofs != offsetCand_) {
        offsetCand_ = ofs;
        offsetCandN_ = 1;
        break;
      }
      s.localOffsetMin = ofs;
      s.localOffsetSrc = src;
      s.localOffsetMs = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 129029: {  // GNSS-positie (fast packet): datum, tijd, positie
      if (len < 23) break;
      if (u16(d + 1) < 0xFFFD && u32(d + 3) < 0xFFFFFFFD) {
        s.gpsEpoch = u16(d + 1) * 86400UL + u32(d + 3) / 10000;
        s.gpsTimeMs = now;
        lastDecoded = pgn;
        decoded++;
      }
      int64_t la = i64(d + 7), lo = i64(d + 15);
      if (la != INT64_MAX && lo != INT64_MAX && validPos(la * 1e-16, lo * 1e-16)) {
        s.lat = la * 1e-16;
        s.lon = lo * 1e-16;
        s.posMs = now;
        lastDecoded = pgn;
        decoded++;
      }
      break;
    }
    case 129025: {  // positie, snel
      if (len < 8 || i32(d) == 0x7FFFFFFF || i32(d + 4) == 0x7FFFFFFF) break;
      if (!validPos(i32(d) * 1e-7, i32(d + 4) * 1e-7)) break;
      s.lat = i32(d) * 1e-7;
      s.lon = i32(d + 4) * 1e-7;
      s.posMs = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 129026: {  // COG en SOG
      if (len < 6) break;
      s.cogRef = d[1] & 0x03;
      s.cog = degNorm(fu16(d + 2, 1e-4f) * RAD2DEG);
      s.sog = fu16(d + 4, 0.01f);
      s.cogSogMs = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 127250: {  // koers (kompas)
      if (len < 8) break;
      s.heading = degNorm(fu16(d + 1, 1e-4f) * RAD2DEG);
      s.headingRef = d[7] & 0x03;
      if (!isnan(s.heading)) s.headingMs = now, lastDecoded = pgn, decoded++;
      break;
    }
    case 128259: {  // snelheid door het water
      if (len < 3) break;
      s.stw = fu16(d + 1, 0.01f);
      if (!isnan(s.stw)) s.stwMs = now, lastDecoded = pgn, decoded++;
      break;
    }
    case 128267: {  // diepte
      if (len < 7) break;
      s.depth = fu32(d + 1, 0.01f);
      s.depthOffset = fi16(d + 5, 0.001f);
      if (!isnan(s.depth)) s.depthMs = now, lastDecoded = pgn, decoded++;
      break;
    }
    case 130306: {  // wind
      if (len < 6) break;
      uint8_t ref = d[5] & 0x07;
      float spd = fu16(d + 1, 0.01f), ang = degNorm(fu16(d + 3, 1e-4f) * RAD2DEG);
      if (ref == 2) {
        s.aws = spd;
        s.awa = ang;
        s.appWindMs = now;
        lastDecoded = pgn;
        decoded++;
      } else {
        s.tws = spd;
        s.twa = ang;
        s.trueRef = ref;
        s.trueWindMs = now;
        lastDecoded = pgn;
        decoded++;
      }
      break;
    }
    case 127488: {  // motor, snel: toerental
      N2kEngine *ep = len >= 3 ? engineSlot(d[0], now) : nullptr;
      if (!ep) break;
      N2kEngine &e = *ep;
      e.rpm = fu16(d + 1, 0.25f);
      e.rapidMs = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 127489: {  // motor, dynamisch (fast packet)
      N2kEngine *ep = len >= 26 ? engineSlot(d[0], now) : nullptr;
      if (!ep) break;
      N2kEngine &e = *ep;
      e.oilBar = fu16(d + 1, 100) / 1e5f;
      e.oilTempC = fu16(d + 3, 0.1f) - KELVIN;
      e.coolantC = fu16(d + 5, 0.01f) - KELVIN;
      e.altV = fi16(d + 7, 0.01f);
      e.fuelLph = fi16(d + 9, 0.1f);
      e.hours = fu32(d + 11, 1) / 3600.0f;
      e.loadPct = d[24] == 0x7F ? -128 : (int8_t)d[24];
      e.dynMs = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 127505: {  // tankniveau
      if (len < 7) break;
      uint8_t inst = d[0] & 0x0F, type = d[0] >> 4;
      float level = fi16(d + 1, 0.004f);
      if (isnan(level)) break;
      N2kTank *t = nullptr;
      for (auto &x : s.tanks)
        if (x.ms && x.type == type && x.instance == inst) t = &x;
      for (auto &x : s.tanks)
        if (!t && !x.ms) t = &x;
      if (!t) break;
      t->type = type;
      t->instance = inst;
      t->levelPct = level;
      t->capacityL = fu32(d + 3, 0.1f);
      t->ms = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 130312:    // temperatuur
    case 130316: {  // temperatuur, uitgebreid bereik
      if (len < 6) break;
      uint8_t inst = d[1], source = d[2];
      float c;
      if (pgn == 130312) c = fu16(d + 3, 0.01f) - KELVIN;
      else c = u24(d + 3) >= 0xFFFFFD ? NAN : u24(d + 3) * 0.001f - KELVIN;
      if (isnan(c) || c < 200 - KELVIN) break;  // 0 K (bv. NAC-3 zonder watertemperatuursensor) = niet gemeten
      N2kTemp *t = nullptr;
      for (auto &x : s.temps)
        if (x.ms && x.source == source && x.instance == inst) t = &x;
      for (auto &x : s.temps)
        if (!t && !x.ms) t = &x;
      if (!t) break;
      t->source = source;
      t->instance = inst;
      t->c = c;
      t->ms = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 127508: {  // accustatus: spanning, stroom, temperatuur
      if (len < 7) break;
      N2kBattery *b = battSlot(d[0], now);
      if (!b) break;
      b->volts = fi16(d + 1, 0.01f);
      b->amps = fi16(d + 3, 0.1f);
      float k = fu16(d + 5, 0.01f);
      b->tempC = isnan(k) || k < 200 ? NAN : k - KELVIN;  // 0 K (sommige apparaten) = niet gemeten
      b->ms = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 127506: {  // DC-status: laadtoestand en resterende tijd (fast packet)
      if (len < 7) break;
      N2kBattery *b = battSlot(d[1], now);
      if (!b) break;
      b->soc = d[3] <= 100 ? d[3] : NAN;
      b->ttgMin = u16(d + 5) >= 0xFFFD ? -1 : u16(d + 5);  // per minuut (60 s)
      b->detailMs = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 127245: {  // roer: positie in 0,0001 rad, + = stuurboord
      if (len < 6) break;
      float r = fi16(d + 4, 0.0001f);
      if (isnan(r)) break;
      // Meerdere roeren (of een stuurautomaat met eigen bericht): bij het eerste blijven zolang dat zendt
      if (s.rudderMs && d[0] != s.rudderInstance && now - s.rudderMs < 5000) break;
      s.rudderInstance = d[0];
      s.rudder = r * RAD2DEG;
      s.rudderMs = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 127257: {  // scheepsstand: stampen en rollen (helling)
      if (len < 7) break;
      float p = fi16(d + 3, 0.0001f), r = fi16(d + 5, 0.0001f);
      if (isnan(p) && isnan(r)) break;
      s.pitch = isnan(p) ? NAN : p * RAD2DEG;
      s.roll = isnan(r) ? NAN : r * RAD2DEG;
      s.attitudeMs = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    case 130313: {  // luchtvochtigheid in 0,004 %
      if (len < 5) break;
      float pct = fi16(d + 3, 0.004f);
      if (isnan(pct) || pct < 0 || pct > 100) break;
      uint8_t inst = d[1], source = d[2];
      N2kHumidity *h = nullptr;
      for (auto &x : s.humids)
        if (x.ms && x.source == source && x.instance == inst) h = &x;
      for (auto &x : s.humids)
        if (!h && !x.ms) h = &x;
      if (!h) break;
      h->source = source;
      h->instance = inst;
      h->pct = pct;
      h->ms = now;
      lastDecoded = pgn;
      decoded++;
      break;
    }
    default: break;
  }
  (void)src;
}

// Vertalen (i18n.cpp). Zonder i18n (bv. in de pc-tests) blijft de Nederlandse tekst staan.
__attribute__((weak)) const char *tr(const char *nl) { return nl; }

const char *n2kTankTypeName(uint8_t t) {
  static const char *n[] = {TN("Diesel"), TN("Water"), TN("Grijs water"), TN("Aaskom"), TN("Olie"), TN("Vuilwater")};
  return T(t < 6 ? n[t] : TN("Tank"));
}

void n2kTankLabel(const N2kState &s, const N2kTank &t, char *buf, unsigned len) {
  int sameType = 0;
  for (auto &o : s.tanks) sameType += o.ms && o.type == t.type;
  if (sameType > 1) snprintf(buf, len, "%s %u", n2kTankTypeName(t.type), t.instance + 1);
  else snprintf(buf, len, "%s", n2kTankTypeName(t.type));
}

void n2kEngineLabel(const N2kState &s, const N2kEngine &e, char *buf, unsigned len) {
  int count = 0;
  for (auto &o : s.engine) count += (o.rapidMs || o.dynMs) ? 1 : 0;
  if (count < 2) snprintf(buf, len, "%s", T("Motor"));
  else if (e.instance == 0) snprintf(buf, len, "%s", T("Bakboordmotor"));
  else if (e.instance == 1) snprintf(buf, len, "%s", T("Stuurboordmotor"));
  else snprintf(buf, len, T("Motor %u"), e.instance);
}
