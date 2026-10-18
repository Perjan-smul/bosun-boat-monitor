#include "log.h"
#include "clock.h"
#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/time.h>

namespace {

// Twee soorten klokchip, automatisch gevonden: een losse DS3231-module, of de PCF85063 die op de
// Waveshare-print zit. Beide bewaren UTC.
constexpr uint8_t DS3231 = 0x68, PCF85063 = 0x51;
uint8_t rtcAddr = 0;  // 0 = geen klokchip
constexpr uint32_t MIN_VALID = 1735689600;  // 2025-01-01: alles daarvoor is "niet ingesteld"
bool rtcPresent = false, rtcLost = false, valid = false;
// De tijd is de tijd die je zelf instelt. Basis is Midden-Europese tijd (met zomer-/wintertijd); wie in een
// andere zone vaart (bv. VK of Portugal) zet de klok gewoon op de eigen tijd. Staat de klok gelijk met de GPS,
// dan wordt het verschil met Midden-Europese tijd bewaard (in kwartieren), en houdt de GPS daarna alleen de
// minuten en seconden precies.
int32_t offsetMin = 0;
bool utcTrusted = false;  // UTC komt van de GPS (deze start)
// Lokale tijd van het netwerk (PGN 129033): laatst ontvangen verschil met UTC, en de keuze van de schipper
int32_t netOffset = 0;
bool netKnown = false, netThisBoot = false;
ClockSource source = ClockSource::Ask;
constexpr int32_t NET_NONE = INT32_MIN;

void saveInt(const char *key, int32_t v) {
  Preferences p;
  p.begin("klok", false);
  p.putInt(key, v);
  p.end();
}
void saveOffset() { saveInt("ofs", offsetMin); }

bool validZone(int32_t m) { return m >= -14 * 60 && m <= 14 * 60 && m % 15 == 0; }

uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }

// Dagen sinds 1970-01-01 voor een UTC-datum (Howard Hinnant, werkt zonder timegm)
int32_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

bool rtcRead(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(rtcAddr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(rtcAddr, len) != len) return false;
  for (int i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

void rtcWrite(uint8_t reg, const uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(rtcAddr);
  Wire.write(reg);
  Wire.write(buf, len);
  Wire.endTransmission();
}

void setSystemTime(uint32_t epoch) {
  timeval tv = {(time_t)epoch, 0};
  settimeofday(&tv, nullptr);
}

}  // namespace

void clock_begin() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // Midden-Europese tijd; eigen verschil in offsetMin
  tzset();
  Preferences p;
  p.begin("klok", true);
  offsetMin = p.getInt("ofs", 0);
  int32_t net = p.getInt("net", NET_NONE);
  int32_t src = p.getInt("bron", (int32_t)ClockSource::Ask);
  p.end();
  if (offsetMin < -14 * 60 || offsetMin > 14 * 60) offsetMin = 0;
  netKnown = net != NET_NONE && validZone(net);
  netOffset = netKnown ? net : 0;
  source = src >= 0 && src <= (int32_t)ClockSource::Own ? (ClockSource)src : ClockSource::Ask;
  utcTrusted = false;  // pas na een nieuwe GPS-tijd weer
  netThisBoot = false;

  rtcAddr = 0;
  valid = rtcLost = false;
  for (uint8_t a : {DS3231, PCF85063}) {  // een losse DS3231 gaat voor (nauwkeuriger)
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      rtcAddr = a;
      break;
    }
  }
  rtcPresent = rtcAddr != 0;
  if (!rtcPresent) {
    log_line("[klok] geen RTC gevonden");
    return;
  }
  // Tijdregisters: sec, min, uur, dag, maand, jaar (BCD). De volgorde verschilt per chip.
  uint8_t r[7];
  int sec, minute, hour, day, month, year;
  if (rtcAddr == DS3231) {
    uint8_t status;
    if (!rtcRead(0x00, r, 7) || !rtcRead(0x0F, &status, 1)) return;
    rtcLost = status & 0x80;  // OSF: oscillator heeft stilgestaan
    sec = bcd2dec(r[0] & 0x7F), minute = bcd2dec(r[1] & 0x7F), hour = bcd2dec(r[2] & 0x3F);
    day = bcd2dec(r[4] & 0x3F), month = bcd2dec(r[5] & 0x1F), year = 2000 + bcd2dec(r[6]);
  } else {
    if (!rtcRead(0x04, r, 7)) return;  // seconden, minuten, uren, dagen, weekdag, maanden, jaren
    rtcLost = r[0] & 0x80;             // OS: oscillator heeft stilgestaan
    sec = bcd2dec(r[0] & 0x7F), minute = bcd2dec(r[1] & 0x7F), hour = bcd2dec(r[2] & 0x3F);
    day = bcd2dec(r[3] & 0x3F), month = bcd2dec(r[5] & 0x1F), year = 2000 + bcd2dec(r[6]);
  }
  uint32_t epoch = daysFromCivil(year, month, day) * 86400UL + hour * 3600UL + minute * 60UL + sec;
  if (!rtcLost && epoch >= MIN_VALID) {
    setSystemTime(epoch);
    valid = true;
  }
  logf("[klok] %s gevonden, tijd %s\n", rtcAddr == DS3231 ? "DS3231" : "PCF85063", valid ? "geldig" : "niet ingesteld");
}

bool clock_valid() { return valid; }
uint32_t clock_now() { return valid ? (uint32_t)time(nullptr) : 0; }
bool clock_rtc_present() { return rtcPresent; }
bool clock_rtc_lost_power() { return rtcPresent && rtcLost; }

bool clock_local(uint32_t epoch, struct tm &out) {
  if (!epoch) return false;
  if (source == ClockSource::Network && netKnown) {
    // Het netwerk geeft het hele verschil met UTC (zomer-/wintertijd zit erin, als de plotter dat bijhoudt)
    time_t t = (time_t)epoch + netOffset * 60;
    gmtime_r(&t, &out);
    return true;
  }
  time_t t = (time_t)epoch + offsetMin * 60;
  localtime_r(&t, &out);
  return true;
}

int32_t clock_utc_offset_min(uint32_t epoch) {
  struct tm t;
  if (!clock_local(epoch, t)) return 0;
  int64_t shown = (int64_t)daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) * 86400 + t.tm_hour * 3600 +
                  t.tm_min * 60 + t.tm_sec;
  return (int32_t)((shown - (int64_t)epoch) / 60);
}

void clock_set_utc(uint32_t epoch) {
  setSystemTime(epoch);
  valid = true;
  if (!rtcPresent) return;
  time_t t = epoch;
  struct tm u;
  gmtime_r(&t, &u);
  if (rtcAddr == DS3231) {
    uint8_t r[7] = {dec2bcd(u.tm_sec),  dec2bcd(u.tm_min),     dec2bcd(u.tm_hour),       (uint8_t)(u.tm_wday + 1),
                    dec2bcd(u.tm_mday), dec2bcd(u.tm_mon + 1), dec2bcd(u.tm_year - 100)};
    rtcWrite(0x00, r, 7);
    uint8_t ctrl = 0x1C, status = 0x00;  // oscillator aan op batterij, OSF wissen
    rtcWrite(0x0E, &ctrl, 1);
    rtcWrite(0x0F, &status, 1);
  } else {
    uint8_t c1;
    if (rtcRead(0x00, &c1, 1) && (c1 & 0x20)) {  // STOP-bit: klok stond stil
      c1 &= ~0x20;
      rtcWrite(0x00, &c1, 1);
    }
    // Seconden met OS-bit 0: dat wist de melding "oscillator heeft stilgestaan"
    uint8_t r[7] = {dec2bcd(u.tm_sec),  dec2bcd(u.tm_min),         dec2bcd(u.tm_hour),     dec2bcd(u.tm_mday),
                    (uint8_t)u.tm_wday, dec2bcd(u.tm_mon + 1), dec2bcd(u.tm_year - 100)};
    rtcWrite(0x04, r, 7);
  }
  rtcLost = false;
}

void clock_set_local(int year, int month, int day, int hour, int minute) {
  struct tm lt = {};
  lt.tm_year = year - 1900;
  lt.tm_mon = month - 1;
  lt.tm_mday = day;
  lt.tm_hour = hour;
  lt.tm_min = minute;
  lt.tm_isdst = -1;  // zomer-/wintertijd zelf laten bepalen
  if (source == ClockSource::Network) {
    // Zelf instellen betekent: niet meer de plotter volgen
    source = ClockSource::Own;
    saveInt("bron", (int32_t)source);
    log_line("[klok] zelf ingesteld: tijd van het netwerk niet meer gevolgd");
  }
  int32_t asCet = (int32_t)mktime(&lt);  // de ingestelde tijd, gelezen als Midden-Europese tijd
  if (utcTrusted && valid) {
    // UTC klopt al (GPS): alleen het verschil met Midden-Europese tijd onthouden, in hele kwartieren
    int32_t diff = asCet - (int32_t)time(nullptr);
    int32_t q = (diff >= 0 ? diff + 450 : diff - 450) / 900;
    offsetMin = q * 15;
    if (offsetMin < -14 * 60) offsetMin = -14 * 60;
    if (offsetMin > 14 * 60) offsetMin = 14 * 60;
    saveOffset();
    logf("[klok] eigen tijd: %+ld min t.o.v. Midden-Europese tijd\n", (long)offsetMin);
    return;
  }
  clock_set_utc((uint32_t)(asCet - offsetMin * 60));  // zonder GPS: het eigen verschil blijft gelden
}

void clock_set_gps(uint32_t utc) {
  clock_set_utc(utc);
  utcTrusted = true;
}

int32_t clock_offset_min() { return offsetMin; }

void clock_network_offset(int32_t m) {
  if (!validZone(m)) return;
  netThisBoot = true;
  if (netKnown && m == netOffset) return;
  netOffset = m;
  netKnown = true;
  saveInt("net", m);
  logf("[klok] lokale tijd van het netwerk: UTC%+ld:%02ld\n", (long)(m / 60), (long)(labs(m) % 60));
}

bool clock_network_known() { return netKnown; }
int32_t clock_network_offset_min() { return netOffset; }
ClockSource clock_source() { return source; }

void clock_set_source(ClockSource src) {
  if (src == source) return;
  source = src;
  saveInt("bron", (int32_t)src);
  logf("[klok] tijd: %s\n", src == ClockSource::Network ? "van het netwerk" : src == ClockSource::Own ? "eigen tijd" : "nog kiezen");
}

bool clock_network_offer() {
  if (source != ClockSource::Ask || !netKnown || !netThisBoot || !valid) return false;
  uint32_t now = clock_now();
  return clock_utc_offset_min(now) != netOffset;  // geeft het netwerk dezelfde tijd, dan valt er niets te kiezen
}
