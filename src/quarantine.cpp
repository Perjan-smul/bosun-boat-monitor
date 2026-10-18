#include "quarantine.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_attr.h>
#include <string.h>
#include "i18n.h"
#include "log.h"
#include "store.h"

namespace {

constexpr uint32_t MAGIC = 0x0A2A1715;

// Wat er op dit moment verwerkt wordt, en hoe vaak een apparaat al bij een crash betrokken was.
// In RTC-geheugen: blijft bewaard bij een crash of waakhond-herstart (niet bij stroomuitval).
struct Rtc {
  uint32_t magic;
  QEntry busyBle;  // kind None = niets bezig
  QEntry busyN2k;
  QEntry strike[4];
  uint8_t strikes[4];
};
RTC_NOINIT_ATTR Rtc rtc;

// Uitgeschakelde apparaten: blijvend (NVS), en een kopie in RAM voor de snelle controle per pakket
QEntry blocked[Q_MAX];
volatile int blockedCount = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Omschrijving; translate = true: in de taal van het scherm, false: Nederlands (voor het logboek)
void describe(const QEntry &e, char *out, size_t len, bool translate) {
  if (e.kind == QKind::Ble) {
    // Naam van het apparaat als het is toegevoegd
    DeviceState d;
    for (int i = 0; store_get(i, d); i++)
      if (strcmp(d.cfg.mac, e.mac) == 0) {
        snprintf(out, len, "%s (%s)", d.cfg.name, e.mac);
        return;
      }
    const char *fmt = TN("Bluetooth-apparaat %s");
    snprintf(out, len, translate ? T(fmt) : fmt, e.mac);
  } else if (e.kind == QKind::N2k) {
    const char *fmt = TN("NMEA 2000 PGN %lu van adres %u");
    snprintf(out, len, translate ? T(fmt) : fmt, (unsigned long)e.pgn, e.src);
  } else {
    snprintf(out, len, "-");
  }
}

bool same(const QEntry &a, const QEntry &b) {
  if (a.kind != b.kind) return false;
  if (a.kind == QKind::Ble) return strcmp(a.mac, b.mac) == 0;
  return a.pgn == b.pgn && a.src == b.src;
}

void save() {
  Preferences p;
  p.begin("quar", false);
  p.putBytes("list", blocked, blockedCount * sizeof(QEntry));
  p.end();
}

void load() {
  Preferences p;
  p.begin("quar", true);
  size_t len = p.getBytesLength("list");
  if (len % sizeof(QEntry) == 0 && len <= sizeof(blocked)) {
    p.getBytes("list", blocked, len);
    blockedCount = len / sizeof(QEntry);
  }
  p.end();
}

void block(const QEntry &e) {
  for (int i = 0; i < blockedCount; i++)
    if (same(blocked[i], e)) return;
  portENTER_CRITICAL(&mux);
  if (blockedCount == Q_MAX) {  // vol: de oudste eruit
    memmove(blocked, blocked + 1, (Q_MAX - 1) * sizeof(QEntry));
    blockedCount--;
  }
  blocked[blockedCount++] = e;
  portEXIT_CRITICAL(&mux);
  save();
}

// Eén crash toeschrijven aan dit apparaat; bij de tweede keer uitschakelen (geeft true)
bool strike(const QEntry &e) {
  if (e.kind == QKind::None) return false;
  char d[64];
  describe(e, d, sizeof(d), false);
  int slot = -1;
  for (int i = 0; i < 4; i++)
    if (rtc.strikes[i] && same(rtc.strike[i], e)) slot = i;
  if (slot < 0) {
    for (int i = 0; i < 4 && slot < 0; i++)
      if (!rtc.strikes[i]) slot = i;
    if (slot < 0) slot = 0;
    rtc.strike[slot] = e;
    rtc.strikes[slot] = 0;
  }
  rtc.strikes[slot]++;
  logf("[uitschakelen] crash tijdens het verwerken van %s (%u keer)\n", d, rtc.strikes[slot]);
  if (rtc.strikes[slot] >= Q_STRIKES) {
    block(e);
    rtc.strikes[slot] = 0;
    logf("[uitschakelen] %s uitgeschakeld: het board crashte steeds bij dit apparaat\n", d);
    return true;
  }
  return false;
}

}  // namespace

bool quarantine_begin(bool crashed) {
  bool newBlock = false;
  if (rtc.magic != MAGIC) {
    memset(&rtc, 0, sizeof(rtc));
    rtc.magic = MAGIC;
  }
  load();
  if (crashed) {
    newBlock |= strike(rtc.busyBle);
    newBlock |= strike(rtc.busyN2k);
  } else {
    memset(rtc.strikes, 0, sizeof(rtc.strikes));  // gewone start: opnieuw tellen
  }
  rtc.busyBle.kind = QKind::None;
  rtc.busyN2k.kind = QKind::None;
  return newBlock;
}

void quarantine_enter_ble(const char *mac) {
  QEntry &b = rtc.busyBle;
  b.kind = QKind::None;  // eerst ongeldig: nooit een half geschreven MAC-adres de schuld geven
  strncpy(b.mac, mac, sizeof(b.mac) - 1);
  b.mac[sizeof(b.mac) - 1] = 0;
  __asm__ __volatile__("" ::: "memory");  // eerst het adres, dan pas "bezig"
  b.kind = QKind::Ble;
}
void quarantine_leave_ble() { rtc.busyBle.kind = QKind::None; }

void quarantine_enter_n2k(uint32_t pgn, uint8_t src) {
  QEntry &b = rtc.busyN2k;
  b.kind = QKind::None;
  b.pgn = pgn;
  b.src = src;
  __asm__ __volatile__("" ::: "memory");
  b.kind = QKind::N2k;
}
void quarantine_leave_n2k() { rtc.busyN2k.kind = QKind::None; }

bool quarantine_ble_blocked(const char *mac) {
  if (!blockedCount) return false;
  bool hit = false;
  portENTER_CRITICAL(&mux);
  for (int i = 0; i < blockedCount && !hit; i++)
    hit = blocked[i].kind == QKind::Ble && strcmp(blocked[i].mac, mac) == 0;
  portEXIT_CRITICAL(&mux);
  return hit;
}

bool quarantine_n2k_blocked(uint32_t pgn, uint8_t src) {
  if (!blockedCount) return false;
  bool hit = false;
  portENTER_CRITICAL(&mux);
  for (int i = 0; i < blockedCount && !hit; i++)
    hit = blocked[i].kind == QKind::N2k && blocked[i].pgn == pgn && blocked[i].src == src;
  portEXIT_CRITICAL(&mux);
  return hit;
}

int quarantine_list(QEntry *out, int max) {
  portENTER_CRITICAL(&mux);
  int n = blockedCount < max ? blockedCount : max;
  memcpy(out, blocked, n * sizeof(QEntry));
  portEXIT_CRITICAL(&mux);
  return n;
}

void quarantine_release(int index) {
  if (index < 0 || index >= blockedCount) return;
  char d[64];
  describe(blocked[index], d, sizeof(d), false);
  portENTER_CRITICAL(&mux);
  memmove(blocked + index, blocked + index + 1, (blockedCount - index - 1) * sizeof(QEntry));
  blockedCount--;
  portEXIT_CRITICAL(&mux);
  save();
  logf("[uitschakelen] %s weer ingeschakeld\n", d);
}

void quarantine_describe(const QEntry &e, char *out, size_t len) { describe(e, out, len, true); }
