#include "log.h"
#include "ota.h"
#include <Arduino.h>
#include <Preferences.h>
#include <Update.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "board.h"
#include "sdcard.h"
#include "i18n.h"
#include "ota_guard.h"
#include "webdiag.h"
#include "webkeys.h"

namespace {

constexpr uint32_t HEALTHY_MS = 60000;     // zo lang moet het programma draaien om als gezond te gelden
constexpr uint32_t CONNECT_MS = 30000;     // opgeven als het wifi-netwerk niet bereikbaar is
constexpr uint32_t CONFIRM_MS = 60000;     // na deze tijd zonder antwoord: niet installeren

uint32_t idleOffMs = 10 * 60000UL;  // wifi uit na zoveel tijd zonder gebruik (instelling, standaard 10 min)
OtaRecord rec{false, 0};
bool bootedFromUpdate = false;
volatile OtaPhase phase = OtaPhase::Off;
volatile bool alarmActive = false;
volatile uint32_t restartAt = 0;  // herstart gevraagd (bv. na terugzetten van een backup)
char address[20] = "";
char code[9] = "";  // 8 cijfers: ook het wifi-wachtwoord van het eigen wifipunt (WPA2 vraagt er minstens 8)
char apName[24] = "";
volatile bool apActive = false;  // eigen wifipunt aan (anders: op een bestaand netwerk)
DNSServer dns;  // eigen wifipunt: elke naam wijst naar het board, zo opent de telefoon de pagina vanzelf
uint32_t phaseSince = 0, lastActivity = 0, stagedAt = 0;
WebServer web(80);
volatile bool serverUp = false;
volatile bool stopServer = false;  // de servertaak stopt de webserver zelf, niet midden in een verzoek
const esp_partition_t *stagedPart = nullptr;  // waar de nieuwe firmware staat, tot de bevestiging
const char *rejectReason = "";  // Nederlandse tekst (de sleutel); vertalen bij het tonen
bool reqRejected = false;  // dit verzoek is geweigerd; de fase blijft zoals hij was

// Elke build bevat de naam van zijn eigen board. Een upload zonder deze tekst is voor een ander
// board (of geen firmware van Bosun) en wordt geweigerd: anders blijft het scherm zwart.
const char BOARD_MARK[] = "@BMB=" BOARD_NAME ";";
size_t markLen() { return sizeof(BOARD_MARK) - 1; }
size_t markPos = 0;
bool markFound = false;

// '@' komt maar één keer voor in de tekst, dus bij een verkeerd teken opnieuw beginnen is genoeg
void scanForMark(const uint8_t *b, size_t n) {
  for (size_t i = 0; i < n && !markFound; i++) {
    if (b[i] == (uint8_t)BOARD_MARK[markPos]) {
      if (++markPos == markLen()) markFound = true;
    } else {
      markPos = b[i] == (uint8_t)BOARD_MARK[0] ? 1 : 0;
    }
  }
}

void saveRecord() {
  Preferences p;
  p.begin("ota", false);
  p.putBool("pending", rec.pending);
  p.putUChar("tries", rec.tries);
  p.end();
}

// Welke slot de nieuwe firmware kreeg. Zo weten we na een terugval door de bootloader
// dat we op de oude versie draaien, en vermijden we dat we de kapotte versie weer kiezen.
uint8_t stagedSubtype() {
  Preferences p;
  p.begin("ota", true);
  uint8_t v = p.getUChar("sub", 0xFF);
  p.end();
  return v;
}

void saveStagedSubtype(uint8_t sub) {
  Preferences p;
  p.begin("ota", false);
  p.putUChar("sub", sub);
  p.end();
}

void loadRecord() {
  Preferences p;
  p.begin("ota", true);
  rec.pending = p.getBool("pending", false);
  rec.tries = p.getUChar("tries", 0);
  p.end();
}

void setPhase(OtaPhase p) {
  phase = p;
  phaseSince = millis();
}

// ---------- webpagina ----------

// Alleen opmaak; de teksten komen er bij het maken van de pagina bij (vertaald)
const char PAGE_CSS[] PROGMEM =
    "body{font-family:sans-serif;background:#0b1c2c;color:#e8f1f8;padding:16px}"
    "button{font-size:18px;padding:12px 20px;background:#2ec4b6;border:0;border-radius:8px;color:#06121c}"
    "a{color:#2ec4b6}input{font-size:18px;padding:8px}";

// Taalcode voor <html lang=..>, in de volgorde van Lang
const char *langCode() {
  static const char *const C[] = {"nl", "en", "fr", "de", "es"};
  uint8_t l = (uint8_t)i18n_lang();
  return l < sizeof(C) / sizeof(C[0]) ? C[l] : "nl";
}

String pageStart(const char *title, const char *css) {
  String p = F("<!doctype html><html lang=");
  p += langCode();
  p += F("><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>");
  p += title;
  p += F("</title><style>");
  p += FPSTR(css);
  p += F("</style></head><body>");
  return p;
}

void touch() { lastActivity = millis(); }

// Eigen wifipunt: wie het wifi-wachtwoord van het scherm heeft, mag erin. Op een bestaand netwerk
// zitten ook anderen; daar vraagt de pagina de code.
bool authOk() { return apActive || web.authenticate("boot", code); }

bool allowed() {
  touch();
  if (authOk()) return true;
  web.requestAuthentication();
  return false;
}

const char MENU_CSS[] PROGMEM =
    "body{font-family:sans-serif;background:#0b1c2c;color:#e8f1f8;padding:16px}"
    "a{display:block;font-size:20px;padding:16px;margin:12px 0;background:#13293d;border-radius:10px;color:#2ec4b6;"
    "text-decoration:none}";

void menuLink(String &p, const char *href, const char *text) {
  p += F("<a href='");
  p += href;
  p += F("'>");
  p += text;
  p += F("</a>");
}

void handleRoot() {
  if (!allowed()) return;
  String p = pageStart("Bosun", MENU_CSS);
  p += F("<h2>Bosun</h2>");
  menuLink(p, "/sleutels", T("Victron-sleutels invoeren"));
  menuLink(p, "/diagnose", T("Diagnose en logboek"));
  menuLink(p, "/backup", T("Backup van de instellingen"));
  if (sd_supported()) menuLink(p, "/kaart", T("Micro-SD-kaart: metingen, logboek, backups"));
  menuLink(p, "/firmware", T("Firmware-update"));
  p += F("</body></html>");
  web.send(200, "text/html; charset=utf-8", p);
}

void handleFirmware() {
  if (!allowed()) return;
  String p = pageStart(T("Bosun update"), PAGE_CSS);
  p += F("<p><a href='/'>&larr; ");
  p += T("Menu");
  p += F("</a></p><h2>");
  p += T("Firmware-update");
  p += F("</h2><p>");
  p += T("Kies het .bin-bestand van de nieuwe versie. Daarna bevestig je de installatie op het scherm van de T-Panel.");
  p += F("</p><form method=POST action=/update enctype=multipart/form-data>"
         "<input type=file name=firmware accept=.bin><br><br><button type=submit>");
  p += T("Uploaden");
  p += F("</button></form></body></html>");
  web.send(200, "text/html; charset=utf-8", p);
}

void handleUploadDone() {
  if (!allowed()) return;
  if (reqRejected || phase == OtaPhase::Rejected) {
    // rejectReason is de Nederlandse tekst (ook voor het logboek); hier pas vertalen
    char msg[256];
    snprintf(msg, sizeof(msg), T("Niet gelukt: %s"), T(rejectReason));
    web.send(409, "text/plain; charset=utf-8", msg);
  } else if (phase == OtaPhase::Staged) {
    web.send(200, "text/plain; charset=utf-8", T("Ontvangen. Bevestig de installatie op het scherm van de T-Panel."));
  } else {
    web.send(500, "text/plain; charset=utf-8", T("Onbekende fout."));
  }
}

void reject(const char *why) {
  rejectReason = why;
  logf("[ota] upload geweigerd: %s\n", why);
  setPhase(OtaPhase::Rejected);
}

// Upload in stukken: schrijven gaat in de achtergrondtaak, niet in het scherm
void handleUploadData() {
  HTTPUpload &u = web.upload();
  if (u.status == UPLOAD_FILE_START) {
    touch();
    // Een geweigerd verzoek verandert de fase niet: een klaargezette update blijft wachten op het scherm
    reqRejected = true;
    if (!authOk()) {  // al bij het begin controleren: vóór de eerste byte
      rejectReason = TN("verkeerde code.");
      return;
    }
    if (phase != OtaPhase::Serving && phase != OtaPhase::Rejected) {
      rejectReason = TN("er staat al een update klaar of wordt geüpload.");
      return;
    }
    reqRejected = false;
    if (alarmActive) {
      reject(TN("er is een alarm actief. Bevestig het alarm eerst."));
      return;
    }
    markPos = 0;
    markFound = false;
    setPhase(OtaPhase::Uploading);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) reject(TN("kan het flashgeheugen niet voorbereiden."));
  } else if (u.status == UPLOAD_FILE_WRITE) {
    touch();
    if (reqRejected || phase != OtaPhase::Uploading) return;
    scanForMark(u.buf, u.currentSize);
    if (Update.write(u.buf, u.currentSize) != u.currentSize) {
      Update.abort();
      reject(TN("schrijven naar flash mislukt."));
    }
  } else if (u.status == UPLOAD_FILE_END) {
    touch();
    if (reqRejected || phase != OtaPhase::Uploading) return;
    if (!markFound) {
      Update.abort();
      reject(TN("dit bestand is geen Bosun-firmware voor dit board."));
    } else if (Update.end(true)) {
      // Update.end heeft de nieuwe slot als opstartslot gezet. Meteen terugzetten: zo start na
      // een stroomonderbreking gewoon de huidige versie, en pas na de bevestiging de nieuwe.
      stagedPart = esp_ota_get_boot_partition();
      esp_ota_set_boot_partition(esp_ota_get_running_partition());
      stagedAt = millis();
      setPhase(OtaPhase::Staged);
      logf("[ota] nieuwe firmware ontvangen (%u bytes), wacht op bevestiging\n", (unsigned)u.totalSize);
    } else {
      Update.abort();
      reject(TN("het bestand is beschadigd of te groot."));
    }
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    if (reqRejected || phase != OtaPhase::Uploading) return;
    Update.abort();
    reject(TN("de upload is afgebroken."));
  }
}

// Eigen taak, zodat het schrijven naar flash het scherm niet ophoudt
void serverTask(void *) {
  for (;;) {
    if (stopServer) {
      if (serverUp) web.stop();
      dns.stop();
      serverUp = false;
      stopServer = false;
    }
    if (serverUp) {
      if (apActive) dns.processNextRequest();
      web.handleClient();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void randomCode() {
  // Zes cijfers; zonder seed is de reeks bij elke start gelijk, dus de hardware-generator gebruiken
  snprintf(code, sizeof(code), "%08lu", (unsigned long)(esp_random() % 100000000UL));
}

void stopWifi() {
  if (phase != OtaPhase::Off) log_line("[wifi] uit");
  if (serverUp) stopServer = true;  // de servertaak stopt hem tussen twee verzoeken door
  if (apActive) WiFi.softAPdisconnect(true);
  else WiFi.disconnect(true, false);
  apActive = false;
  WiFi.mode(WIFI_OFF);
  address[0] = 0;
  if (phase != OtaPhase::Staged && phase != OtaPhase::Uploading) setPhase(OtaPhase::Off);
}

}  // namespace

void ota_boot_check() {
  loadRecord();
  if (rec.pending) {
    // De bootloader kan zelf al teruggevallen zijn op de oude slot: dan is de update al weg
    if (esp_ota_get_running_partition()->subtype != stagedSubtype()) {
      otaMarkHealthy(rec);
      saveRecord();
      log_line("[ota] bootloader is teruggevallen op de vorige versie");
    }
  }
  BootAction a = otaOnBoot(rec);
  if (a == BootAction::Revert) {
    // De andere slot is de vorige versie: daarheen terug
    const esp_partition_t *other = esp_ota_get_next_update_partition(nullptr);
    if (other) esp_ota_set_boot_partition(other);
    saveRecord();
    log_line("[ota] nieuwe firmware start niet: terug naar de vorige versie");
    delay(500);
    esp_restart();
  }
  bootedFromUpdate = a == BootAction::WaitHealthy;
  if (a == BootAction::WaitHealthy) saveRecord();  // telling bewaren, ook als we nu crashen
}

void ota_begin() {
  web.on("/", HTTP_GET, handleRoot);
  web.on("/firmware", HTTP_GET, handleFirmware);
  webkeys_register(web, allowed);
  webdiag_register(web, allowed, authOk);
  web.on("/update", HTTP_POST, handleUploadDone, handleUploadData);
  web.onNotFound([]() {
    // Eigen wifipunt: de telefoon test zelf of er internet is (bv. /generate_204). Doorsturen naar
    // het menu, dan toont de telefoon "Aanmelden bij netwerk" en opent hij de pagina vanzelf.
    if (apActive) {
      web.sendHeader("Location", "http://192.168.4.1/");
      web.send(302, "text/plain", "");
    } else {
      web.send(404, "text/plain; charset=utf-8", T("niet gevonden"));
    }
  });
  xTaskCreatePinnedToCore(serverTask, "ota", 8192, nullptr, 2, nullptr, 0);
  randomCode();
  logf("[ota] klaar, %s\n", bootedFromUpdate ? "nieuwe versie wordt gecontroleerd" : "normaal opstarten");
}

void ota_start() {
  if (phase != OtaPhase::Off || serverUp || stopServer) return;
  idleOffMs = wifi_idle_min() * 60000UL;
  randomCode();
  if (wifi_ap_mode()) {
    // Eigen wifipunt: naam met een stukje van het MAC-adres, zodat twee boten elkaar niet storen
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(apName, sizeof(apName), "Bosun-%02X%02X", mac[4], mac[5]);
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(apName, code)) {
      WiFi.mode(WIFI_OFF);
      return;
    }
    IPAddress ip = WiFi.softAPIP();  // 192.168.4.1
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(53, "*", ip);
    apActive = true;
    web.begin();
    serverUp = true;
    snprintf(address, sizeof(address), "%s", ip.toString().c_str());
    setPhase(OtaPhase::Serving);
    touch();
    logf("[wifi] eigen wifipunt %s aan, adres %s\n", apName, address);
    return;
  }
  Preferences p;
  p.begin("wifi", true);
  String ssid = p.getString("ssid", "");
  String pass = p.getString("pass", "");
  p.end();
  if (!ssid.length()) return;  // eerst instellen
  apActive = false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  setPhase(OtaPhase::Connecting);
  touch();
}

void ota_stop() {
  if (phase == OtaPhase::Uploading || phase == OtaPhase::Staged) return;  // niet midden in een upload of beslissing
  stopWifi();
  setPhase(OtaPhase::Off);
}

void ota_set_alarm_active(bool active) { alarmActive = active; }

void ota_confirm(bool install) {
  if (phase != OtaPhase::Staged) return;
  if (install && alarmActive) return;  // niet herstarten met een alarm dat nog niet gezien is
  if (install) {
    if (!stagedPart || esp_ota_set_boot_partition(stagedPart) != ESP_OK) {
      reject(TN("de nieuwe versie kan niet worden gestart."));
      return;
    }
    otaStage(rec);  // de nieuwe versie start met de telling op 0
    saveRecord();
    saveStagedSubtype(stagedPart->subtype);
    log_line("[ota] installeren: herstart");
    delay(300);
    esp_restart();
  }
  // Afwijzen: de opstartslot staat al op de huidige versie
  log_line("[ota] nieuwe firmware afgewezen");
  stagedPart = nullptr;
  setPhase(OtaPhase::Serving);
}

void ota_tick() {
  uint32_t now = millis();

  if (restartAt && (int32_t)(now - restartAt) >= 0) {
    log_line("[ota] herstart gevraagd via de wifi-pagina");
    delay(200);
    esp_restart();
  }

  if (phase == OtaPhase::Connecting) {
    if (WiFi.status() == WL_CONNECTED && !serverUp && !stopServer) {
      web.begin();
      serverUp = true;
      snprintf(address, sizeof(address), "%s", WiFi.localIP().toString().c_str());
      setPhase(OtaPhase::Serving);
      touch();
      logf("[wifi] verbonden met het netwerk, adres %s\n", address);
    } else if (now - phaseSince > CONNECT_MS) {
      log_line("[wifi] verbinden met het netwerk mislukt (naam of wachtwoord fout, of buiten bereik)");
      stopWifi();
    }
  }

  if ((phase == OtaPhase::Serving || phase == OtaPhase::Rejected) && now - lastActivity > idleOffMs) stopWifi();

  // Staged maar niet beslist binnen de tijd: niet installeren
  if (phase == OtaPhase::Staged && now - stagedAt > CONFIRM_MS) ota_confirm(false);

  // Gezond: de nieuwe versie draait en blijft draaien
  if (bootedFromUpdate && now > HEALTHY_MS) {
    bootedFromUpdate = false;
    otaMarkHealthy(rec);
    saveRecord();
    esp_ota_mark_app_valid_cancel_rollback();
    log_line("[ota] nieuwe versie gezond");
  }
}

const char *ota_address() { return address; }
const char *ota_code() { return code; }
OtaPhase ota_phase() { return phase; }
const char *ota_reject_reason() { return rejectReason; }
bool ota_alarm_active() { return alarmActive; }
void ota_request_restart(uint32_t afterMs) { restartAt = (millis() + afterMs) | 1; }
const char *ota_ap_name() { return apActive ? apName : ""; }

bool wifi_ap_mode() {
  Preferences p;
  p.begin("wifi", true);
  bool ap = p.getBool("ap", true);  // standaard: eigen wifipunt
  p.end();
  return ap;
}

uint8_t wifi_idle_min() {
  Preferences p;
  p.begin("wifi", true);
  uint8_t m = p.getUChar("idle", 10);
  p.end();
  return m >= 1 && m <= 120 ? m : 10;
}

void wifi_set_idle_min(uint8_t minutes) {
  Preferences p;
  p.begin("wifi", false);
  p.putUChar("idle", minutes);
  p.end();
  idleOffMs = minutes * 60000UL;  // geldt ook meteen als de wifi al aan staat
  touch();
}

void wifi_set_ap_mode(bool ap) {
  Preferences p;
  p.begin("wifi", false);
  p.putBool("ap", ap);
  p.end();
}

bool wifi_credentials(char *ssid, unsigned ssidLen, char *pass, unsigned passLen) {
  Preferences p;
  p.begin("wifi", true);
  String s = p.getString("ssid", "");
  String w = p.getString("pass", "");
  p.end();
  snprintf(ssid, ssidLen, "%s", s.c_str());
  snprintf(pass, passLen, "%s", w.c_str());
  return s.length() > 0;
}

void wifi_save_credentials(const char *ssid, const char *pass) {
  Preferences p;
  p.begin("wifi", false);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.end();
}
