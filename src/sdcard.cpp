#include "sdcard.h"
#include <new>
#include "psram.h"
#include <Arduino.h>
#include "board.h"
#include "log.h"

#if !defined(SD_CLK)

// Board zonder kaartslot
bool sd_supported() { return false; }
void sd_begin() {}
void sd_tick() {}
SdInfo sd_info() { return SdInfo{}; }
int sd_list(const char *, SdFile *, int) { return 0; }
bool sd_valid_path(const char *) { return false; }
bool sd_read_text(const char *, String &, size_t) { return false; }
bool sd_send_file(WebServer &, const char *) { return false; }

#else

#include <SD_MMC.h>
#include <freertos/semphr.h>
#include <WebServer.h>
#include <algorithm>
#include <vector>
#include "baro.h"
#include "clock.h"
#include "datalog.h"
#include "i18n.h"
#include "n2k.h"
#include "store.h"
#include "webdiag.h"

namespace {

constexpr const char *ROOT = "/bosun";
constexpr uint32_t LOG_FLUSH_MS = 2000;   // logboek naar de kaart
constexpr uint32_t HOURLY_MS = 3600000;   // backup controleren en opruimen
constexpr uint32_t FIRST_BACKUP_MS = 120000;
constexpr int KEEP_BACKUPS = 60;
constexpr uint64_t MIN_FREE = 100ULL * 1024 * 1024;  // en minstens 5 % van de kaart vrij houden

// Kaart wordt gebruikt door de UI-lus (schrijven) en de webserver (lezen, downloaden)
SemaphoreHandle_t lockHandle() {
  static StaticSemaphore_t buf;
  static SemaphoreHandle_t h = xSemaphoreCreateMutexStatic(&buf);
  return h;
}
struct Lock {
  bool ok;
  explicit Lock(uint32_t waitMs) { ok = xSemaphoreTake(lockHandle(), pdMS_TO_TICKS(waitMs)) == pdTRUE; }
  ~Lock() {
    if (ok) xSemaphoreGive(lockHandle());
  }
};

bool mounted = false, everMounted = false;
uint32_t lastLogFlush = 0, lastHourly = 0, rowsWritten = 0;
bool firstHourlyDone = false;
const char *lastError = "";
uint32_t logPos = 0;

// Kolommen van het huidige bestand (± 6 KB, in PSRAM)
DataLog &dlog() {
  static DataLog *d = new (psram_alloc<DataLog>(1)) DataLog();
  return *d;
}
int curDay = 0;
char curFile[48];       // relatief pad van het huidige metingenbestand
bool curHasHeader = false;
int lastMinute = -1;
uint32_t lastBackupCrc = 0;

String full(const char *rel) { return String(ROOT) + "/" + rel; }

uint32_t crc32(const char *p, size_t n) {
  uint32_t c = 0xFFFFFFFF;
  while (n--) {
    c ^= (uint8_t)*p++;
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320 & -(c & 1));
  }
  return ~c;
}

void fail(const char *why) {
  if (mounted) logf("[sd] %s: kaart afgemeld\n", why);
  lastError = why;
  mounted = false;
  SD_MMC.end();  // pas bij de volgende start weer zoeken
}

bool mount() {
  SD_MMC.end();
  if (!SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0)) {
    lastError = TN("pinnen niet in te stellen");
    return false;
  }
  // 1-bit modus (er is maar één datalijn), niet formatteren: een kaart wissen doet de gebruiker zelf
  if (!SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5) || SD_MMC.cardType() == CARD_NONE) {
    SD_MMC.end();
    lastError = TN("Geen kaart, of niet FAT32");
    return false;
  }
  for (const char *d : {"", "/metingen", "/logboek", "/backup"}) SD_MMC.mkdir(String(ROOT) + d);
  mounted = true;
  everMounted = true;
  lastError = "";
  curDay = 0;  // bij het volgende meetpunt het bestand van vandaag opnieuw zoeken
  logf("[sd] kaart gevonden: %llu MB, %llu MB gebruikt\n", SD_MMC.totalBytes() >> 20, SD_MMC.usedBytes() >> 20);
  return true;
}

bool append(const char *rel, const char *data, size_t n) {
  File f = SD_MMC.open(full(rel), FILE_APPEND);
  if (!f) return false;
  size_t w = f.write((const uint8_t *)data, n);
  f.close();
  return w == n;
}

bool exists(const char *rel) { return SD_MMC.exists(full(rel)); }

bool firstLine(const char *rel, char *out, size_t len) {
  File f = SD_MMC.open(full(rel), FILE_READ);
  if (!f) return false;
  size_t n = f.readBytesUntil('\n', out, len - 1);
  f.close();
  out[n] = 0;
  return n > 0;
}

// Tekstpad voor deel k van een dag: "metingen/2026-10-13.csv", "metingen/2026-10-13_2.csv", ...
void partName(char *out, size_t len, const struct tm &t, int k) {
  if (k <= 1) snprintf(out, len, "metingen/%04d-%02d-%02d.csv", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  else snprintf(out, len, "metingen/%04d-%02d-%02d_%d.csv", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, k);
}

int curPart = 1;

// Nieuwe dag (of na een herstart): verder in het laatste deel van vandaag, met dezelfde kolommen
void openDay(const struct tm &t) {
  char name[48];
  int last = 0;
  for (int k = 1; k < 100; k++) {
    partName(name, sizeof(name), t, k);
    if (!exists(name)) break;
    last = k;
  }
  dlog().reset();
  curHasHeader = false;
  curPart = last ? last : 1;
  partName(curFile, sizeof(curFile), t, curPart);
  if (last) {
    constexpr size_t LINE_N = DL_MAX_COLS * DL_NAME_LEN;
    static char *line = psram_alloc<char>(LINE_N);
    if (line && firstLine(curFile, line, LINE_N)) {
      const char *p = line;
      if ((uint8_t)p[0] == 0xEF && (uint8_t)p[1] == 0xBB && (uint8_t)p[2] == 0xBF) p += 3;  // BOM
      dlog().loadHeader(p);
      curHasHeader = true;
    }
  }
}

void writeRow(const struct tm &t) {
  // Werkbuffers in PSRAM (samen ± 18 KB)
  constexpr size_t BUF_N = 4096;
  static DeviceState *devs = psram_alloc<DeviceState>(MAX_DEVICES);
  static DlValue *vals = psram_alloc<DlValue>(DL_MAX_COLS);
  static char *buf = psram_alloc<char>(BUF_N);
  static N2kState ns;
  if (!devs || !vals || !buf) return fail(TN("geen geheugen"));
  int nd = 0;
  while (nd < MAX_DEVICES && store_get(nd, devs[nd])) nd++;
  n2k_get(ns);
  int n = datalog_collect(devs, nd, ns, millis(), vals, DL_MAX_COLS, baro_info().hPa);

  int day = (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
  if (day != curDay) {
    curDay = day;
    openDay(t);
  }
  if (dlog().addColumns(vals, n) && curHasHeader) {
    // Nieuwe kolom: verder in een nieuw deel met de uitgebreide kop
    partName(curFile, sizeof(curFile), t, ++curPart);
    curHasHeader = false;
  }
  if (dlog().columns() == 0) return;  // nog niets te meten

  if (!curHasHeader) {
    buf[0] = (char)0xEF, buf[1] = (char)0xBB, buf[2] = (char)0xBF;  // UTF-8-markering: Excel toont dan ° en é goed
    dlog().header(buf + 3, BUF_N - 3, T("tijd"));
    if (!append(curFile, buf, strlen(buf))) return fail(TN("schrijven mislukt"));
    curHasHeader = true;
  }
  char when[24];
  snprintf(when, sizeof(when), "%04d-%02d-%02d %02d:%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
  dlog().row(buf, BUF_N, when, vals, n, i18n_lang() != Lang::EN);
  if (!append(curFile, buf, strlen(buf))) return fail(TN("schrijven mislukt"));
  rowsWritten++;
}

void flushLog() {
  static char buf[1024];
  char name[40];
  struct tm t;
  if (clock_valid() && clock_local(clock_now(), t))
    snprintf(name, sizeof(name), "logboek/%04d-%02d.txt", t.tm_year + 1900, t.tm_mon + 1);
  else
    snprintf(name, sizeof(name), "logboek/zonder-datum.txt");
  for (int i = 0; i < 8; i++) {
    uint32_t before = logPos;
    size_t n = log_read(logPos, buf, sizeof(buf));
    if (!n) return;
    if (!append(name, buf, n)) {
      logPos = before;  // later opnieuw
      return fail(TN("schrijven mislukt"));
    }
  }
}

// Backup alleen als er iets veranderd is (de datumregel telt niet mee)
void backupIfChanged() {
  struct tm t;
  if (!clock_valid() || !clock_local(clock_now(), t)) return;
  String b = backup_make();
  String cmp = b;
  int d = cmp.indexOf("\ndatum ");
  if (d >= 0) cmp.remove(d + 1, cmp.indexOf('\n', d + 1) - d);
  uint32_t c = crc32(cmp.c_str(), cmp.length());
  if (c == lastBackupCrc) return;
  char name[48];
  snprintf(name, sizeof(name), "backup/backup-%04d-%02d-%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  File f = SD_MMC.open(full(name), FILE_WRITE);
  if (!f) return fail(TN("schrijven mislukt"));
  size_t w = f.print(b);
  f.close();
  if (w != b.length()) return fail(TN("schrijven mislukt"));
  lastBackupCrc = c;
  logf("[sd] backup geschreven: %s\n", name);
}

// Namen in een map, oplopend (de datum zit in de naam: oudste eerst)
int names(const char *dir, std::vector<String> &out) {
  out.clear();
  File d = SD_MMC.open(full(dir));
  if (!d || !d.isDirectory()) return 0;
  for (File f = d.openNextFile(); f; f = d.openNextFile()) {
    if (!f.isDirectory()) {
      String n = f.name();
      int slash = n.lastIndexOf('/');
      out.push_back(slash >= 0 ? n.substring(slash + 1) : n);
    }
    f.close();
  }
  d.close();
  std::sort(out.begin(), out.end(), [](const String &a, const String &b) { return strcmp(a.c_str(), b.c_str()) < 0; });
  return out.size();
}

void cleanup() {
  std::vector<String> list;
  // Hooguit KEEP_BACKUPS backups
  names("backup", list);
  for (int i = 0; i + KEEP_BACKUPS < (int)list.size(); i++) SD_MMC.remove(full(("backup/" + list[i]).c_str()));
  // Kaart bijna vol: oudste metingen weg (en als dat niet genoeg is, oude logboeken)
  uint64_t total = SD_MMC.totalBytes();
  uint64_t need = std::max<uint64_t>(MIN_FREE, total / 20);
  for (const char *dir : {"metingen", "logboek"}) {
    names(dir, list);
    int removed = 0;
    for (size_t i = 0; i + 1 < list.size() && removed < 60; i++) {  // het nieuwste bestand blijft altijd
      if (total - SD_MMC.usedBytes() >= need) return;
      SD_MMC.remove(full((String(dir) + "/" + list[i]).c_str()));
      removed++;
    }
    if (removed) logf("[sd] kaart bijna vol: %d oude bestanden uit %s verwijderd\n", removed, dir);
  }
}

}  // namespace

bool sd_supported() { return true; }

void sd_begin() {
  Lock l(1000);
  // Vanaf het logboek van vóór een crash of vastloper: dat is wat je dan wilt bewaren
  logPos = log_fail_kind() ? 0 : log_boot_start();  // 0 = vanaf het oudste dat er nog is
  if (!mount()) logf("[sd] %s\n", lastError);
}

void sd_tick() {
  // Alleen bij het opstarten gezocht: een kaart die er halverwege in gaat, wordt pas na een herstart gebruikt
  if (!mounted) return;
  uint32_t now = millis();
  Lock l(0);  // bezig met een download: volgende keer
  if (!l.ok) return;
  if (now - lastLogFlush >= LOG_FLUSH_MS) {
    lastLogFlush = now;
    flushLog();
    if (!mounted) return;
  }
  struct tm t;
  if (clock_valid() && clock_local(clock_now(), t) && t.tm_min != lastMinute) {
    lastMinute = t.tm_min;
    writeRow(t);
    if (!mounted) return;
  }
  if ((!firstHourlyDone && now > FIRST_BACKUP_MS) || (firstHourlyDone && now - lastHourly >= HOURLY_MS)) {
    firstHourlyDone = true;
    lastHourly = now;
    backupIfChanged();
    if (mounted) cleanup();
  }
}

SdInfo sd_info() {
  SdInfo i{};
  Lock l(200);
  i.present = mounted;
  i.wasPresent = everMounted;
  if (mounted && l.ok) {
    i.totalBytes = SD_MMC.totalBytes();
    i.usedBytes = SD_MMC.usedBytes();
  }
  i.rowsWritten = rowsWritten;
  strlcpy(i.lastError, lastError, sizeof(i.lastError));
  return i;
}

bool sd_valid_path(const char *rel) {
  const char *dirs[] = {"metingen/", "logboek/", "backup/"};
  const char *rest = nullptr;
  for (const char *d : dirs)
    if (strncmp(rel, d, strlen(d)) == 0) rest = rel + strlen(d);
  if (!rest || !*rest || strlen(rel) >= sizeof(SdFile::path)) return false;
  if (strstr(rest, "..")) return false;
  for (const char *p = rest; *p; p++)
    if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_' && *p != '.') return false;
  return true;
}

int sd_list(const char *dir, SdFile *out, int max) {
  Lock l(3000);
  if (!l.ok || !mounted) return 0;
  std::vector<String> list;
  names(dir, list);
  int n = 0;
  for (int i = (int)list.size() - 1; i >= 0 && n < max; i--) {  // nieuwste eerst
    snprintf(out[n].path, sizeof(out[n].path), "%s/%s", dir, list[i].c_str());
    File f = SD_MMC.open(full(out[n].path));
    out[n].size = f ? f.size() : 0;
    if (f) f.close();
    n++;
  }
  return n;
}

bool sd_read_text(const char *rel, String &out, size_t maxLen) {
  if (!sd_valid_path(rel)) return false;
  Lock l(3000);
  if (!l.ok || !mounted) return false;
  File f = SD_MMC.open(full(rel), FILE_READ);
  if (!f || f.size() > maxLen) {
    if (f) f.close();
    return false;
  }
  out = f.readString();
  f.close();
  return true;
}

bool sd_send_file(WebServer &web, const char *rel) {
  if (!sd_valid_path(rel)) return false;
  Lock l(5000);
  if (!l.ok || !mounted) return false;
  File f = SD_MMC.open(full(rel), FILE_READ);
  if (!f) return false;
  const char *base = strrchr(rel, '/') + 1;
  web.sendHeader("Content-Disposition", String("attachment; filename=\"") + base + "\"");
  bool csv = strstr(base, ".csv") != nullptr;
  web.streamFile(f, csv ? "text/csv; charset=utf-8" : "text/plain; charset=utf-8");
  f.close();
  return true;
}

#endif
