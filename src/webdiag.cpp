#include "webdiag.h"
#include "psram.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <stdarg.h>
#include <esp_ota_ops.h>
#include <string>
#include <vector>
#include "alarms.h"
#include "baro.h"
#include "board.h"
#include "clock.h"
#include "i18n.h"
#include "log.h"
#include "n2k.h"
#include "ota.h"
#include "quarantine.h"
#include "sdcard.h"
#include "store.h"
#include "webkeys.h"

namespace {

WebServer *srv;
bool (*isAllowed)();
bool (*isAuthOk)();

const char *BOARD = BOARD_NAME;

// ---------- hulpjes ----------

String hex(const uint8_t *b, size_t n) {
  static const char *H = "0123456789abcdef";
  String o;
  o.reserve(n * 2);
  for (size_t i = 0; i < n; i++) {
    o += H[b[i] >> 4];
    o += H[b[i] & 15];
  }
  return o;
}

int hexVal(char c) {
  return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

bool unhex(const String &s, std::vector<uint8_t> &out) {
  if (s.length() % 2) return false;
  out.resize(s.length() / 2);
  for (size_t i = 0; i < out.size(); i++) {
    int a = hexVal(s[2 * i]), b = hexVal(s[2 * i + 1]);
    if (a < 0 || b < 0) return false;
    out[i] = a << 4 | b;
  }
  return true;
}

String duration(uint32_t s) {
  char b[32];
  if (s >= 86400) snprintf(b, sizeof(b), T("%lu d %lu u"), (unsigned long)(s / 86400), (unsigned long)(s % 86400 / 3600));
  else if (s >= 3600) snprintf(b, sizeof(b), T("%lu u %lu min"), (unsigned long)(s / 3600), (unsigned long)(s % 3600 / 60));
  else if (s >= 60) snprintf(b, sizeof(b), "%lu min", (unsigned long)(s / 60));
  else snprintf(b, sizeof(b), "%lu s", (unsigned long)s);
  return b;
}

String ago(uint32_t ms) {
  if (!ms) return T("nooit");
  char b[64];
  snprintf(b, sizeof(b), T("%s geleden"), duration((millis() - ms) / 1000).c_str());
  return b;
}

// Opmaak met een (vertaalde) printf-tekst
String fmt(const char *f, ...) __attribute__((format(printf, 1, 2)));
String fmt(const char *f, ...) {
  char b[256];
  va_list a;
  va_start(a, f);
  vsnprintf(b, sizeof(b), f, a);
  va_end(a);
  return b;
}

String span(const char *cls, const char *text) { return String("<span class=") + cls + ">" + text + "</span>"; }

String row(const char *k, const String &v) { return String("<tr><td class=muted>") + k + "</td><td>" + v + "</td></tr>"; }

// ---------- diagnose ----------

void handleDiag() {
  if (!isAllowed()) return;
  String p = web_head();
  p += F("<style>table{width:100%;border-collapse:collapse;font-size:15px}td{padding:4px 6px;vertical-align:top}"
         "td:first-child{width:36%}"
         "pre{white-space:pre-wrap;font-size:12px;background:#06121c;padding:8px;border-radius:8px}</style>");
  p += String("<p><a href='/'>&larr; ") + T("Menu") + "</a> &middot; <a href='/diagnose'>" + T("Vernieuwen") +
       "</a> &middot; <a href='/logboek.txt'>" + T("Logboek downloaden") + "</a></p><h2>" + T("Diagnose") + "</h2>";

  // Board
  char buf[160];
  const esp_partition_t *run = esp_ota_get_running_partition();
  p += String("<div class=card><b>") + T("Board") + "</b><table>";
  p += row(T("Firmware"), String(__DATE__) + " " + __TIME__ + " (" + BOARD + ", " + (run ? run->label : "?") + ")");
  p += row(T("Draait"), duration(millis() / 1000));
  p += row(T("Laatste start"), web_esc(T(log_reset_reason())));
  p += row(T("Aantal starts"), String(log_boot_count()));
  if (log_fail_kind()) {
    char code[8];
    log_error_code(code, sizeof(code));
    p += row(T("Vorige start"),
             span("warn", fmt(T("mislukt: foutcode %s (%s tijdens %s), %lu keer na elkaar"), code,
                              log_fail_kind() == 2 ? T("vastgelopen") : T("crash"), T(log_stage_name(log_fail_stage())),
                              (unsigned long)log_failures()).c_str()));
  }
  snprintf(buf, sizeof(buf), T("%lu KB vrij (laagste %lu KB, grootste blok %lu KB)"), (unsigned long)(ESP.getFreeHeap() / 1024),
           (unsigned long)(ESP.getMinFreeHeap() / 1024), (unsigned long)(ESP.getMaxAllocHeap() / 1024));
  p += row(T("Geheugen"), buf);
  snprintf(buf, sizeof(buf), T("%lu KB vrij"), (unsigned long)(ESP.getFreePsram() / 1024));
  p += row("PSRAM", buf);
  size_t fsTotal = LittleFS.totalBytes();
  if (fsTotal) snprintf(buf, sizeof(buf), T("%lu van %lu KB gebruikt"), (unsigned long)(LittleFS.usedBytes() / 1024), (unsigned long)(fsTotal / 1024));
  else snprintf(buf, sizeof(buf), "%s", T("niet beschikbaar"));
  p += row(T("Opslag historie"), buf);
  {
    BaroInfo b = baro_info();
    if (!isnan(b.hPa))
      p += row(T("Barometer"), fmt(T("%.1f hPa (%s), %s"), b.hPa, b.sourceName, T(baro_tendency_text(b.tendency))));
    else
      p += row(T("Barometer"), T("geen (BME280 op I2C, NMEA 2000 of een Ruuvi met luchtdruk)"));
  }
  if (sd_supported()) {
    SdInfo si = sd_info();
    if (si.present)
      p += row(T("Micro-SD-kaart"), fmt(T("%.1f GB vrij, %lu metingen geschreven sinds de start"),
                                         (double)(si.totalBytes - si.usedBytes) / 1e9, (unsigned long)si.rowsWritten));
    else
      p += row(T("Micro-SD-kaart"), si.wasPresent ? span("warn", T(si.lastError)) : String(T("Geen kaart (niet nodig)")));
  }
  struct tm lt;
  if (clock_valid() && clock_local(clock_now(), lt))
    snprintf(buf, sizeof(buf), "%02d-%02d-%04d %02d:%02d", lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900, lt.tm_hour, lt.tm_min);
  else
    snprintf(buf, sizeof(buf), "%s", T("niet ingesteld"));
  String clk = buf;
  if (clock_valid()) {
    int32_t m = clock_utc_offset_min(clock_now());
    clk += " (UTC";
    if (m) {
      snprintf(buf, sizeof(buf), "%c%ld:%02ld", m < 0 ? '-' : '+', (long)(labs(m) / 60), (long)(labs(m) % 60));
      clk += buf;
    }
    if (clock_source() == ClockSource::Network) clk += String(", ") + T("van de plotter");
    clk += ")";
  }
  clk += " &middot; ";
  clk += clock_rtc_present() ? (clock_rtc_lost_power() ? span("warn", T("RTC-batterij leeg?")) : String("RTC ok"))
                             : span("warn", T("geen RTC"));
  p += row(T("Klok"), clk);
  if (WiFi.getMode() & WIFI_AP) p += row("Wifi", fmt(T("eigen wifipunt, %d verbonden"), (int)WiFi.softAPgetStationNum()));
  else if (WiFi.isConnected()) p += row("Wifi", fmt(T("verbonden, %d dBm"), (int)WiFi.RSSI()));
  p += F("</table></div>");

  // NMEA 2000
  p += F("<div class=card><b>NMEA 2000</b><table>");
  if (!n2k_available()) {
    p += row(T("Status"), T("dit board heeft geen CAN-aansluiting"));
  } else {
    N2kStats st = n2k_stats();
    p += row(T("Status"), st.running ? T("aan") : T("uit (instellingen)"));
    if (st.running) {
      p += row(T("Berichten"), fmt(T("%lu per seconde"), (unsigned long)st.framesPerSec) +
                                   (st.framesPerSec ? String() : " &middot; " + span("warn", T("geen verkeer: kabel?"))));
      p += row(T("Apparaten"), String(st.sources));
      p += row(T("Klok via GPS"), st.lastClockSync ? ago(st.lastClockSync) : String(T("nog niet")));
    }
  }
  p += F("</table></div>");

  // Meldingen
  static Alarm *al = psram_alloc<Alarm>(MAX_ALARMS);
  int na = al ? alarms_snapshot(al, MAX_ALARMS) : 0;
  p += String("<div class=card><b>") + T("Meldingen") + "</b><table>";
  if (!na) p += row("", span("ok", T("geen")));
  for (int i = 0; i < na; i++) {
    const char *cls = al[i].level == AlarmLevel::Alarm ? "err" : "warn";
    String t = span(cls, al[i].level == AlarmLevel::Alarm ? T("alarm") : T("waarschuwing"));
    if (al[i].level == AlarmLevel::Alarm) t += String(" (") + (al[i].acked ? T("bevestigd") : T("niet bevestigd")) + ")";
    p += row(t.c_str(), web_esc(al[i].text));
  }
  p += F("</table></div>");

  // Uitgeschakeld na crashes
  QEntry q[Q_MAX];
  int nq = quarantine_list(q, Q_MAX);
  if (nq) {
    p += String("<div class=card><b>") + T("Uitgeschakeld na crashes") + "</b><table>";
    for (int i = 0; i < nq; i++) {
      char d[64];
      quarantine_describe(q[i], d, sizeof(d));
      p += row(span("err", T("uit")).c_str(),
               web_esc(d) + "<br>" + span("muted", T("Weer inschakelen: op het scherm bij Instellingen")));
    }
    p += F("</table></div>");
  }

  // Bluetooth
  p += String("<div class=card><b>") + T("Bluetooth: toegevoegd") + "</b><table>";
  DeviceState d;
  int nd = 0;
  for (int i = 0; store_get(i, d); i++, nd++) {
    String st;
    if (d.last == ParseResult::BadKey) st = span("err", T("sleutel onjuist"));
    else if (d.last == ParseResult::NeedKey) st = span("warn", T("sleutel nodig"));
    else if (d.lastSeenMs && millis() - d.lastSeenMs < 120000) st = span("ok", "ok");
    else st = span("warn", T("geen verse meting"));
    String v = st + " &middot; " + d.rssi + " dBm<br><span class=muted>" +
               fmt(T("meting %s, gehoord %s"), ago(d.lastSeenMs).c_str(), ago(d.lastHeardMs).c_str()) + "<br>" +
               web_esc(T(typeName(d.cfg.type))) + " &middot; " + d.cfg.mac + "</span>";
    p += row(web_esc(d.cfg.name).c_str(), v);
  }
  if (!nd) p += row("", T("nog geen apparaten"));
  p += F("</table></div>");

  static Discovered found[MAX_DISCOVERED];
  int nf = store_discovered(found, MAX_DISCOVERED);
  p += String("<div class=card><b>") + T("Bluetooth: gevonden, niet toegevoegd") + "</b><table>";
  if (!nf) p += row("", T("geen"));
  for (int i = 0; i < nf; i++)
    p += row(web_esc(found[i].name[0] ? found[i].name : T("(zonder naam)")).c_str(),
             web_esc(T(typeName(found[i].type))) + " &middot; " + found[i].rssi + " dBm &middot; " + found[i].mac);
  p += F("</table></div>");

  // Logboek (ook van vóór een crash of watchdog-herstart)
  size_t cap = 6200;
  char *lg = (char *)malloc(cap);
  if (lg) {
    log_copy(lg, cap);
    p += String("<div class=card><b>") + T("Logboek") + "</b> " + span("muted", T("(nieuwste onderaan)")) + "<pre>";
    p += web_esc(lg);
    p += F("</pre></div>");
    free(lg);
  }
  p += F("</body></html>");
  srv->send(200, "text/html; charset=utf-8", p);
}

void handleLogTxt() {
  if (!isAllowed()) return;
  size_t cap = 6200;
  char *lg = (char *)malloc(cap);
  if (!lg) {
    srv->send(500, "text/plain; charset=utf-8", T("geen geheugen"));
    return;
  }
  log_copy(lg, cap);
  srv->sendHeader("Content-Disposition", "attachment; filename=bosun-logboek.txt");
  srv->send(200, "text/plain; charset=utf-8", lg);
  free(lg);
}

// ---------- backup ----------

void handleBackupPage() {
  if (!isAllowed()) return;
  String p = web_head();
  p += String("<p><a href='/'>&larr; ") + T("Menu") + "</a></p><h2>" + T("Backup") + "</h2>";
  p += String("<div class=card><b>") + T("Backup maken") + "</b><p class=muted>" +
       T("Alle apparaten met hun sleutels en grenzen, de instellingen en "
         "de wifi-instellingen in één bestand. De historie zit er niet in. De code van 8 cijfers op het scherm "
         "zit er niet in: die is elke keer nieuw.") +
       "</p><p class=warn>" +
       T("Het bestand bevat de Victron-sleutels en, als je dat hebt ingesteld, het wachtwoord van je "
         "eigen wifi-netwerk (bij Bestaand netwerk). Bewaar het veilig.") +
       "</p><p><a href='/backup.txt'><button type=button>" + T("Backup downloaden") + "</button></a></p></div>";
  p += String("<div class=card><b>") + T("Backup terugzetten") + "</b><p class=muted>" +
       T("Vervangt alle apparaten en instellingen op dit board. Daarna herstart het board.") + "</p>";
  p += F("<form method=POST action=/terugzetten enctype=multipart/form-data>"
         "<input type=file name=backup accept='.txt,text/plain'><br><br>"
         "<label><input type=checkbox name=zeker value=1 required> ");
  p += T("Ik wil alles op dit board vervangen");
  p += F("</label><br><br><button type=submit>");
  p += T("Terugzetten");
  p += F("</button></form></div></body></html>");
  srv->send(200, "text/html; charset=utf-8", p);
}

void handleBackupTxt() {
  if (!isAllowed()) return;
  char name[64] = "bosun-backup.txt";
  struct tm lt;
  if (clock_valid() && clock_local(clock_now(), lt))
    snprintf(name, sizeof(name), "bosun-backup-%04d-%02d-%02d.txt", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
  srv->sendHeader("Content-Disposition", String("attachment; filename=") + name);
  srv->send(200, "text/plain; charset=utf-8", backup_make());
  log_line("[web] backup gedownload");
}

String uploadBuf;
bool uploadOk = false, uploadTooBig = false;
constexpr size_t MAX_BACKUP = 16384;

void handleRestoreData() {
  HTTPUpload &u = srv->upload();
  if (u.status == UPLOAD_FILE_START) {
    uploadBuf = "";
    uploadTooBig = false;
    uploadOk = isAuthOk();
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (!uploadOk) return;
    if (uploadBuf.length() + u.currentSize > MAX_BACKUP) {
      uploadTooBig = true;
      return;
    }
    uploadBuf.concat((const char *)u.buf, u.currentSize);
  }
}

void restoreResult(bool ok, const String &msg);

void handleRestoreDone() {
  if (!isAllowed()) return;
  String msg;
  bool ok = false;
  if (!uploadOk) msg = T("Niet gelukt: geen toegang.");
  else if (uploadTooBig) msg = T("Niet gelukt: dit bestand is te groot voor een backup.");
  else if (srv->arg("zeker") != "1") msg = T("Niet gelukt: vink aan dat je alles wilt vervangen.");
  else if (ota_alarm_active()) msg = T("Niet gelukt: er is een alarm dat nog niet bevestigd is. Bevestig het eerst op het scherm.");
  else ok = backup_restore(uploadBuf, msg);
  uploadBuf = "";
  restoreResult(ok, msg);
}

void restoreResult(bool ok, const String &msg) {
  String p = web_head();
  p += String("<p><a href='/'>&larr; ") + T("Menu") + "</a></p><h2>" + T("Backup terugzetten") + "</h2><div class=msg>";
  p += web_esc(msg.c_str());
  p += F("</div>");
  if (ok)
    p += String("<p>") + T("Het board herstart nu. Gebruik je het eigen wifipunt, dan moet je daarna opnieuw verbinden.") + "</p>";
  p += F("</body></html>");
  srv->send(ok ? 200 : 400, "text/html; charset=utf-8", p);
  if (ok) ota_request_restart(3000);
}

// ---------- micro-SD-kaart ----------

String sizeText(uint32_t b) {
  char t[24];
  if (b >= 1048576) snprintf(t, sizeof(t), "%.1f MB", b / 1048576.0);
  else snprintf(t, sizeof(t), "%lu kB", (unsigned long)((b + 1023) / 1024));
  return t;
}

void fileList(String &p, const char *dir, const char *title, const char *intro, bool restore) {
  static SdFile files[62];  // niet op de stack van de servertaak
  int n = sd_list(dir, files, 62);
  p += String("<div class=card><b>") + title + "</b><p class=muted>" + intro + "</p>";
  if (!n) p += String("<p class=muted>") + T("Nog geen bestanden.") + "</p>";
  for (int i = 0; i < n; i++) {
    if (!sd_valid_path(files[i].path)) continue;  // bestanden die er niet door ons op zijn gezet
    const char *base = strrchr(files[i].path, '/') + 1;
    p += String("<p><a href='/kaart/bestand?p=") + web_esc(files[i].path) + "'>" + web_esc(base) + "</a> <span class=muted>" +
         sizeText(files[i].size) + "</span>";
    if (restore) {
      p += F("<form method=POST action=/kaart/terugzetten style='display:inline;margin-left:8px'>"
             "<input type=hidden name=p value='");
      p += web_esc(files[i].path);
      p += F("'><label class=muted><input type=checkbox name=zeker value=1 required> ");
      p += T("zeker");
      p += F("</label> <button type=submit style='font-size:14px;padding:4px 10px'>");
      p += T("Terugzetten");
      p += F("</button></form>");
    }
    p += F("</p>");
  }
  if (n == 62) p += String("<p class=muted>") + T("Oudere bestanden staan ook op de kaart: haal de kaart eruit om ze te lezen.") + "</p>";
  p += F("</div>");
}

void handleCard() {
  if (!isAllowed()) return;
  String p = web_head();
  p += String("<p><a href='/'>&larr; ") + T("Menu") + "</a></p><h2>" + T("Micro-SD-kaart") + "</h2>";
  SdInfo si = sd_info();
  if (!si.present) {
    p += String("<div class=msg>") + T("Geen kaart gevonden.") + " " + (si.lastError[0] ? T(si.lastError) : "") + "</div><p class=muted>" +
         T("Steek een micro-SD-kaart (FAT32, tot 32 GB) in het slot en herstart het board met de RESET-knop. De kaart wordt alleen bij het opstarten gezocht.") +
         "</p></body></html>";
    srv->send(200, "text/html; charset=utf-8", p);
    return;
  }
  p += String("<p class=muted>") +
       fmt(T("%.1f GB van %.1f GB vrij. Is de kaart bijna vol, dan verdwijnen de oudste metingen vanzelf."),
           (double)(si.totalBytes - si.usedBytes) / 1e9, (double)si.totalBytes / 1e9) +
       "</p>";
  fileList(p, "metingen", T("Metingen (CSV)"),
           T("Elke minuut alle waarden, één bestand per dag. Opent in Excel of Numbers."), false);
  fileList(p, "logboek", T("Logboek"), T("Per maand. Blijft ook na een stroomuitval bewaard."), false);
  fileList(p, "backup", T("Backups"),
           T("Elk uur gecontroleerd; bij een wijziging een nieuwe backup van die dag. Terugzetten vervangt alle "
             "apparaten en instellingen en herstart het board."),
           true);
  p += String("<p class=warn>") +
       T("De backups bevatten de Victron-sleutels en het wifi-wachtwoord. Wie de kaart heeft, heeft die ook.") +
       "</p></body></html>";
  srv->send(200, "text/html; charset=utf-8", p);
}

void handleCardFile() {
  if (!isAllowed()) return;
  String path = srv->arg("p");
  if (!sd_send_file(*srv, path.c_str())) srv->send(404, "text/plain; charset=utf-8", T("Bestand niet gevonden."));
}

void handleCardRestore() {
  if (!isAllowed()) return;
  String path = srv->arg("p"), text, msg;
  bool ok = false;
  if (srv->arg("zeker") != "1") msg = T("Niet gelukt: vink aan dat je alles wilt vervangen.");
  else if (ota_alarm_active()) msg = T("Niet gelukt: er is een alarm dat nog niet bevestigd is. Bevestig het eerst op het scherm.");
  else if (strncmp(path.c_str(), "backup/", 7) != 0 || !sd_read_text(path.c_str(), text, MAX_BACKUP))
    msg = T("Niet gelukt: de backup op de kaart is niet te lezen.");
  else ok = backup_restore(text, msg);
  if (ok) logf("[web] backup teruggezet van de kaart: %s\n", path.c_str());
  restoreResult(ok, msg);
}

}  // namespace

String backup_make() {
  String o = "# Bosun backup\n# Bevat de Victron-sleutels en het wachtwoord van het ingestelde wifi-netwerk: bewaar dit bestand veilig.\nformat 1\n";
  o += String("board ") + BOARD + "\n";
  struct tm lt;
  char b[40];
  if (clock_valid() && clock_local(clock_now(), lt)) {
    snprintf(b, sizeof(b), "datum %04d-%02d-%02d %02d:%02d\n", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);
    o += b;
  }
  Preferences p;
  p.begin("bootmon", true);
  o += String("store_ver ") + p.getUChar("ver", 0) + "\n";
  o += String("store_rsz ") + p.getUShort("rsz", 0) + "\n";
  size_t n = p.getBytesLength("devs");
  std::vector<uint8_t> raw(n);
  if (n) p.getBytes("devs", raw.data(), n);
  o += "devs " + hex(raw.data(), n) + "\n";
  n = p.getBytesLength("settings");
  raw.assign(n, 0);
  if (n) p.getBytes("settings", raw.data(), n);
  o += "settings " + hex(raw.data(), n) + "\n";
  p.end();
  p.begin("wifi", true);
  String ssid = p.getString("ssid", ""), pass = p.getString("pass", "");
  o += "wifi_ssid " + hex((const uint8_t *)ssid.c_str(), ssid.length()) + "\n";
  o += "wifi_pass " + hex((const uint8_t *)pass.c_str(), pass.length()) + "\n";
  o += String("wifi_ap ") + (p.getBool("ap", true) ? 1 : 0) + "\n";
  o += String("wifi_idle ") + p.getUChar("idle", 10) + "\n";
  p.end();
  p.begin("ui", true);
  uint8_t lang = p.getUChar("lang", 0xFF);
  p.end();
  if (lang != 0xFF) o += String("ui_lang ") + lang + "\n";
  return o;
}

bool backup_restore(const String &text, String &msg) {
  // Regels "sleutel waarde"; onbekende sleutels negeren (nieuwere versie)
  String format, storeVer, storeRsz, devs, settingsHex, ssid, pass, ap, idle, lang;
  bool haveDevs = false, haveSettings = false, haveWifi = false;
  int start = 0;
  while (start < (int)text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String line = text.substring(start, end);
    start = end + 1;
    line.trim();
    if (!line.length() || line[0] == '#') continue;
    int sp = line.indexOf(' ');
    String k = sp < 0 ? line : line.substring(0, sp);
    String v = sp < 0 ? String("") : line.substring(sp + 1);
    v.trim();
    if (k == "format") format = v;
    else if (k == "store_ver") storeVer = v;
    else if (k == "store_rsz") storeRsz = v;
    else if (k == "devs") devs = v, haveDevs = true;
    else if (k == "settings") settingsHex = v, haveSettings = v.length() > 0;  // leeg = nog nooit opgeslagen
    else if (k == "wifi_ssid") ssid = v, haveWifi = true;
    else if (k == "wifi_pass") pass = v;
    else if (k == "wifi_ap") ap = v;
    else if (k == "wifi_idle") idle = v;
    else if (k == "ui_lang") lang = v;
  }
  if (format != "1") {
    msg = T("Niet gelukt: dit is geen backup van Bosun.");
    return false;
  }
  std::vector<uint8_t> d, s, ws, wp;
  long ver = storeVer.toInt(), rsz = storeRsz.toInt();
  bool devsOk = haveDevs && unhex(devs, d) && ver >= 0 && ver <= 255;
  if (devsOk && !d.empty())  // zonder apparaten mogen versie en recordgrootte 0 zijn
    devsOk = ver >= 1 && rsz >= 60 && rsz <= 1024 && d.size() % rsz == 0 && d.size() / rsz <= (size_t)MAX_DEVICES;
  if (!devsOk) {
    msg = T("Niet gelukt: de lijst met apparaten in het bestand klopt niet.");
    return false;
  }
  if (haveSettings && (!unhex(settingsHex, s) || s.size() < 6 || s.size() > 1024)) {
    msg = T("Niet gelukt: de instellingen in het bestand kloppen niet.");
    return false;
  }
  if (haveWifi && (!unhex(ssid, ws) || !unhex(pass, wp) || ws.size() > 32 || wp.size() > 64)) {
    msg = T("Niet gelukt: de wifi-instellingen in het bestand kloppen niet.");
    return false;
  }

  Preferences p;
  p.begin("bootmon", false);
  p.putUChar("ver", (uint8_t)ver);
  p.putUShort("rsz", (uint16_t)rsz);
  p.putBytes("devs", d.data(), d.size());
  if (haveSettings) p.putBytes("settings", s.data(), s.size());
  p.end();
  if (haveWifi) {
    p.begin("wifi", false);
    p.putString("ssid", std::string(ws.begin(), ws.end()).c_str());
    p.putString("pass", std::string(wp.begin(), wp.end()).c_str());
    if (ap.length()) p.putBool("ap", ap.toInt() != 0);
    long im = idle.toInt();
    if (im >= 1 && im <= 120) p.putUChar("idle", (uint8_t)im);
    p.end();
  }
  long lg = lang.length() ? lang.toInt() : -1;
  if (lg >= 0 && lg < (long)Lang::COUNT) {  // taal (ontbreekt in oudere backups: dan blijft de huidige)
    p.begin("ui", false);
    p.putUChar("lang", (uint8_t)lg);
    p.end();
  }
  size_t count = rsz > 0 ? d.size() / rsz : 0;
  // Hele zinnen per geval: losse stukjes laten zich slecht vertalen
  const char *f = haveSettings ? (haveWifi ? T("Teruggezet: %u apparaten, instellingen en wifi.") : T("Teruggezet: %u apparaten, instellingen."))
                               : (haveWifi ? T("Teruggezet: %u apparaten en wifi.") : T("Teruggezet: %u apparaten."));
  msg = fmt(f, (unsigned)count);
  logf("[web] backup teruggezet: %u apparaten\n", (unsigned)count);
  return true;
}

void webdiag_register(WebServer &web, bool (*allowed)(), bool (*authOk)()) {
  srv = &web;
  isAllowed = allowed;
  isAuthOk = authOk;
  web.on("/diagnose", HTTP_GET, handleDiag);
  web.on("/logboek.txt", HTTP_GET, handleLogTxt);
  web.on("/backup", HTTP_GET, handleBackupPage);
  web.on("/backup.txt", HTTP_GET, handleBackupTxt);
  web.on("/terugzetten", HTTP_POST, handleRestoreDone, handleRestoreData);
  if (sd_supported()) {
    web.on("/kaart", HTTP_GET, handleCard);
    web.on("/kaart/bestand", HTTP_GET, handleCardFile);
    web.on("/kaart/terugzetten", HTTP_POST, handleCardRestore);
  }
}
