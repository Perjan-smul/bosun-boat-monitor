#include "log.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <freertos/semphr.h>
#include <stdarg.h>
#include "clock.h"
#include "i18n.h"

namespace {

constexpr uint32_t MAGIC = 0xB007106A;
constexpr size_t SIZE = 6144;

// Overleeft een herstart (niet een stroomuitval)
struct Ring {
  uint32_t magic;
  uint32_t head;  // volgende schrijfpositie
  uint32_t used;  // aantal geldige bytes (max SIZE)
  char buf[SIZE];
};
RTC_NOINIT_ATTR Ring ring;

// Mislukte starts na elkaar (crash of waakhond) en in welke fase het board was: ook in RTC-geheugen,
// zodat een herstartlus herkend wordt en het herstelscherm kan zeggen waar het misging
struct Boot {
  uint32_t magic;
  uint32_t failures;  // crashes/vastlopers direct na elkaar
  uint8_t stage;      // fase van deze start (bijgewerkt met log_stage)
  uint8_t failStage;  // fase waarin de vorige start misging
  uint8_t failKind;   // 1 = crash, 2 = waakhond
};
RTC_NOINIT_ATTR Boot boot;
uint32_t failuresAtStart = 0;  // de telling bij deze start (de RTC-waarde gaat na een tijd goed draaien terug naar 0)

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
const char *resetReason = TN("onbekend");  // Nederlandse sleutel: vertalen bij het tonen
uint32_t boots = 0;
bool atLineStart = true;
// Doorlopende positie: alles wat ooit in de buffer is geschreven (ook van vóór een herstart die de buffer bewaarde).
// Zo kan de kopie op de SD-kaart steeds alleen het nieuwe stuk ophalen.
uint32_t total = 0, bootStart = 0;

// Meerdere taken loggen (scherm, Bluetooth, NMEA 2000, wifi): één regel tegelijk
SemaphoreHandle_t writeLock() {
  static StaticSemaphore_t b;
  static SemaphoreHandle_t h = xSemaphoreCreateMutexStatic(&b);
  return h;
}

void put(const char *s, size_t n) {
  portENTER_CRITICAL(&mux);
  for (size_t i = 0; i < n; i++) {
    ring.buf[ring.head] = s[i];
    ring.head = (ring.head + 1) % SIZE;
  }
  ring.used = ring.used + n > SIZE ? SIZE : ring.used + n;
  total += n;
  portEXIT_CRITICAL(&mux);
}

// Tijd voor elke regel: klok als die bekend is, anders seconden sinds de start
void stamp() {
  char t[24];
  struct tm lt;
  if (clock_valid() && clock_local(clock_now(), lt))
    snprintf(t, sizeof(t), "%02d-%02d %02d:%02d:%02d ", lt.tm_mday, lt.tm_mon + 1, lt.tm_hour, lt.tm_min, lt.tm_sec);
  else
    snprintf(t, sizeof(t), "+%lus ", (unsigned long)(millis() / 1000));
  put(t, strlen(t));
}

void write(const char *s, size_t n) {
  xSemaphoreTake(writeLock(), portMAX_DELAY);
  Serial.write((const uint8_t *)s, n);
  // Per regel een tijdstempel
  size_t start = 0;
  for (size_t i = 0; i < n; i++) {
    if (atLineStart && s[i] != '\n') {
      put(s + start, i - start);
      start = i;
      stamp();
      atLineStart = false;
    }
    if (s[i] == '\n') atLineStart = true;
  }
  put(s + start, n - start);
  xSemaphoreGive(writeLock());
}

// Nederlandse sleutel (het logboek blijft Nederlands); vertalen bij het tonen met T()
const char *reasonText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return TN("stroom aan");
    case ESP_RST_SW: return TN("herstart door de software (bv. na een update)");
    case ESP_RST_PANIC: return TN("CRASH (programmafout)");
    case ESP_RST_INT_WDT: return TN("WATCHDOG (interrupt)");
    case ESP_RST_TASK_WDT: return TN("WATCHDOG (programma liep vast)");
    case ESP_RST_WDT: return TN("WATCHDOG");
    case ESP_RST_BROWNOUT: return TN("spanning te laag (brown-out)");
    case ESP_RST_DEEPSLEEP: return TN("uit slaapstand");
    case ESP_RST_EXT: return TN("resetknop");
    default: return TN("onbekend");
  }
}

}  // namespace

void log_begin() {
  esp_reset_reason_t r = esp_reset_reason();
  resetReason = reasonText(r);
  // Na stroom aan (of als de buffer onzin bevat) opnieuw beginnen
  if (r == ESP_RST_POWERON || r == ESP_RST_BROWNOUT || ring.magic != MAGIC || ring.head >= SIZE || ring.used > SIZE) {
    ring.magic = MAGIC;
    ring.head = 0;
    ring.used = 0;
  }
  // Alleen crashes en vastlopers tellen; elke andere start (stroom aan, update, resetknop,
  // te lage spanning bij het starten van de motor) begint opnieuw bij 0
  bool wdt = r == ESP_RST_TASK_WDT || r == ESP_RST_INT_WDT || r == ESP_RST_WDT;
  bool failed = wdt || r == ESP_RST_PANIC;
  if (boot.magic != MAGIC || !failed) {
    boot.magic = MAGIC;
    boot.failures = 0;
    boot.failKind = 0;
    boot.failStage = 0;
  } else {
    if (boot.failures < 1000) boot.failures++;
    boot.failKind = wdt ? 2 : 1;
    boot.failStage = boot.stage < STAGE_COUNT ? boot.stage : 0;
  }
  boot.stage = 0;
  failuresAtStart = boot.failures;
  total = bootStart = ring.used;  // wat er nog van vorige starts in staat
  Preferences p;
  p.begin("diag", false);
  boots = p.getUInt("boots", 0) + 1;
  p.putUInt("boots", boots);
  p.end();
  atLineStart = true;
  logf("==== start #%lu, reden: %s ====\n", (unsigned long)boots, resetReason);
  if (boot.failKind)
    logf("[start] vorige start mislukt: %s tijdens %s (%lu keer na elkaar)\n", boot.failKind == 2 ? "vastgelopen" : "crash",
         log_stage_name(boot.failStage), (unsigned long)boot.failures);
}

void logf(const char *fmt, ...) {
  char b[192];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(b, sizeof(b), fmt, ap);
  va_end(ap);
  if (n < 0) return;
  write(b, (size_t)n < sizeof(b) ? (size_t)n : sizeof(b) - 1);
}

void log_line(const char *text) { logf("%s\n", text); }

size_t log_copy(char *out, size_t max) {
  if (!max) return 0;
  portENTER_CRITICAL(&mux);
  size_t n = ring.used < max - 1 ? ring.used : max - 1;
  size_t start = (ring.head + SIZE - n) % SIZE;
  for (size_t i = 0; i < n; i++) {
    char c = ring.buf[(start + i) % SIZE];
    out[i] = (c == '\n' || (c >= 32 && c < 127) || (uint8_t)c >= 0x80) ? c : '?';
  }
  portEXIT_CRITICAL(&mux);
  out[n] = 0;
  return n;
}

size_t log_read(uint32_t &pos, char *out, size_t max) {
  portENTER_CRITICAL(&mux);
  uint32_t oldest = total - ring.used;
  if (pos < oldest || pos > total) pos = oldest;  // overschreven, of een positie van een vorige start
  size_t n = total - pos;
  if (n > max) n = max;
  size_t start = (ring.head + SIZE - (total - pos)) % SIZE;
  for (size_t i = 0; i < n; i++) {
    char c = ring.buf[(start + i) % SIZE];
    out[i] = (c == '\n' || (c >= 32 && c < 127) || (uint8_t)c >= 0x80) ? c : '?';
  }
  pos += n;
  portEXIT_CRITICAL(&mux);
  return n;
}

uint32_t log_position() {
  portENTER_CRITICAL(&mux);
  uint32_t t = total;
  portEXIT_CRITICAL(&mux);
  return t;
}

uint32_t log_boot_start() { return bootStart; }

const char *log_reset_reason() { return resetReason; }
uint32_t log_failures() { return failuresAtStart; }
void log_running_ok() { boot.failures = 0; }
void log_stage(uint8_t s) { boot.stage = s; }
uint8_t log_fail_stage() { return boot.failStage; }
uint8_t log_fail_kind() { return boot.failKind; }

// Nederlandse sleutel (het logboek blijft Nederlands); vertalen bij het tonen met T()
const char *log_stage_name(uint8_t s) {
  static const char *N[STAGE_COUNT] = {TN("opstarten"), TN("instellingen"),      TN("firmware-update"), TN("apparatenlijst"),
                                       TN("scherm"),    TN("klok"),              TN("historie (opslag)"), TN("schermopbouw"),
                                       "Bluetooth",     "NMEA 2000",             TN("in bedrijf"),        TN("SD-kaart")};
  return s < STAGE_COUNT ? N[s] : TN("onbekend");
}

void log_error_code(char *out, size_t len) {
  // Bv. "C6" = crash tijdens de historie, "W10" = vastgelopen tijdens gebruik
  if (boot.failKind) snprintf(out, len, "%c%u", boot.failKind == 2 ? 'W' : 'C', (unsigned)boot.failStage);
  else snprintf(out, len, "-");
}
uint32_t log_boot_count() { return boots; }
