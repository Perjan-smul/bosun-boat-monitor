#include "webkeys.h"
#include <Arduino.h>
#include "i18n.h"
#include "log.h"
#include "store.h"

// Victron-sleutels invoeren via de wifi-pagina: kopiëren uit VictronConnect op de telefoon en
// hier plakken. Draait in de taak van de webserver; de opslag is thread-safe.

namespace {

WebServer *srv;
bool (*isAllowed)();

// Namen komen uit Bluetooth-pakketten: nooit ongefilterd in de pagina zetten
String esc(const char *s) {
  String o;
  for (; *s; s++) {
    switch (*s) {
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '&': o += "&amp;"; break;
      case '"': o += "&quot;"; break;
      case '\'': o += "&#39;"; break;
      default: o += *s;
    }
  }
  return o;
}

// Begin van elke pagina; de taal (<html lang=..>) wordt er bij web_head() voor gezet
const char HEAD[] PROGMEM =
    "<head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Bosun</title><style>"
    "body{font-family:sans-serif;background:#0b1c2c;color:#e8f1f8;padding:12px;max-width:640px;margin:auto}"
    "a{color:#2ec4b6}h2{margin:8px 0 12px}"
    ".card{background:#13293d;border-radius:10px;padding:12px;margin:10px 0}"
    ".muted{color:#8aa4b8;font-size:14px}.ok{color:#2ec4b6}.warn{color:#f4a259}.err{color:#e5534b}"
    "input[type=text]{width:100%;box-sizing:border-box;font-size:17px;padding:8px;margin:4px 0 8px;"
    "border-radius:6px;border:1px solid #3a5670;background:#0b1c2c;color:#e8f1f8}"
    "button{font-size:17px;padding:10px 18px;background:#2ec4b6;border:0;border-radius:8px;color:#06121c}"
    ".msg{padding:10px;border-radius:8px;margin:10px 0;background:#13293d}"
    "</style></head><body>";

const char *statusOf(const DeviceState &d, const char *&cls) {
  if (d.last == ParseResult::BadKey) {
    cls = "err";
    return T("Sleutel onjuist");
  }
  if (!d.cfg.hasKey) {
    cls = "warn";
    return T("Sleutel nodig");
  }
  if (d.lastSeenMs && d.hasData && millis() - d.lastSeenMs < 120000) {
    cls = "ok";
    return T("Ontvangt metingen");
  }
  cls = "muted";
  return d.lastHeardMs ? T("Wacht op een meting") : T("Nog niet ontvangen");
}

String keyField() {
  // Geen autocorrectie of hoofdletters: de telefoon mag de sleutel niet "verbeteren"
  String f = F("<input type=text name=key autocomplete=off autocorrect=off autocapitalize=off spellcheck=false "
               "placeholder='");
  f += esc(T("plak hier de 32 tekens"));  // vertaling kan een ' bevatten
  f += F("'>");
  return f;
}

void handleList() {
  if (!isAllowed()) return;
  String p = web_head();
  p += F("<p><a href='/'>&larr; ");
  p += T("Menu");
  p += F("</a></p><h2>");
  p += T("Victron-sleutels");
  p += F("</h2>");
  String msg = srv->arg("m");
  if (msg.length()) p += "<div class=msg>" + esc(msg.c_str()) + "</div>";
  p += F("<p class=muted>");
  p += T("In VictronConnect: apparaat &rarr; &#9881; &rarr; Productinfo &rarr; Instant readout via "
         "Bluetooth aan &rarr; Show bij Instant readout details. Kopieer de Encryption key en plak hem hier.");
  p += F("</p>");

  // Toegevoegde Victron-apparaten
  bool any = false;
  DeviceState d;
  for (int i = 0; store_get(i, d); i++) {
    if (!isVictron(d.cfg.type)) continue;
    if (!any) p += String("<h3>") + T("Toegevoegd") + "</h3>";
    any = true;
    const char *cls;
    const char *st = statusOf(d, cls);
    p += F("<div class=card><form method=POST action=/sleutel>");
    p += "<b>" + esc(d.cfg.name) + "</b> <span class=" + cls + ">" + st + "</span><br>";
    p += "<span class=muted>" + esc(T(typeName(d.cfg.type))) + " &middot; " + d.cfg.mac + "</span><br>";
    p += "<input type=hidden name=mac value='" + String(d.cfg.mac) + "'>";
    p += String("<label class=muted>") + T("Naam") + "</label>";
    p += "<input type=text name=name maxlength=23 value='" + esc(d.cfg.name) + "'>";
    p += String("<label class=muted>") + (d.cfg.hasKey ? T("Nieuwe sleutel (leeg laten = huidige houden)") : T("Sleutel")) +
         "</label>";
    p += keyField();
    p += String("<button type=submit>") + T("Opslaan") + "</button></form></div>";
  }

  // Gevonden, nog niet toegevoegd
  static Discovered found[MAX_DISCOVERED];  // niet op de stack van de servertaak
  int n = store_discovered(found, MAX_DISCOVERED);
  bool anyNew = false;
  for (int i = 0; i < n; i++) {
    if (!isVictron(found[i].type)) continue;
    if (!anyNew) p += String("<h3>") + T("Gevonden, nog niet toegevoegd") + "</h3>";
    anyNew = true;
    const char *name = found[i].name[0] ? found[i].name : T(typeName(found[i].type));
    p += F("<div class=card><form method=POST action=/sleutel>");
    p += "<b>" + esc(name) + "</b> <span class=muted>" + String(found[i].rssi) + " dBm</span><br>";
    p += "<span class=muted>" + esc(T(typeName(found[i].type))) + " &middot; " + found[i].mac + "</span><br>";
    p += "<input type=hidden name=mac value='" + String(found[i].mac) + "'><input type=hidden name=new value=1>";
    p += String("<label class=muted>") + T("Naam") + "</label>";
    p += "<input type=text name=name maxlength=23 value='" + esc(name) + "'>";
    p += String("<label class=muted>") + T("Sleutel") + "</label>";
    p += keyField();
    p += String("<button type=submit>") + T("Toevoegen") + "</button></form></div>";
  }
  if (!any && !anyNew)
    p += String("<p>") + T("Nog geen Victron-apparaten gevonden. Staat Instant readout aan, en is het apparaat binnen bereik?") +
         "</p>";
  p += String("<p><a href='/sleutels'>") + T("Vernieuwen") + "</a></p></body></html>";
  srv->send(200, "text/html; charset=utf-8", p);
}

void back(const String &msg) {
  String url = "/sleutels?m=";
  // Eenvoudige URL-codering van de melding
  for (char c : msg) {
    if (isalnum((unsigned char)c)) url += c;
    else {
      char b[4];
      snprintf(b, sizeof(b), "%%%02X", (uint8_t)c);
      url += b;
    }
  }
  srv->sendHeader("Location", url);
  srv->send(303, "text/plain", "");
}

void handleSave() {
  if (!isAllowed()) return;
  String mac = srv->arg("mac"), name = srv->arg("name"), keyText = srv->arg("key");
  name.trim();
  bool isNew = srv->arg("new") == "1";
  uint8_t key[16];
  bool haveKey = keyText.length() > 0;
  if (haveKey && !parseVictronKey(keyText.c_str(), key)) {
    back(T("Niet opgeslagen: de sleutel moet 32 tekens zijn (0-9 en a-f)."));
    return;
  }
  if (isNew) {
    Discovered f;
    if (!store_find_discovered(mac.c_str(), f) || !isVictron(f.type)) {
      back(T("Niet toegevoegd: het apparaat is niet meer in de lijst. Probeer opnieuw."));
      return;
    }
    if (!haveKey) {
      back(T("Niet toegevoegd: vul de sleutel in."));
      return;
    }
    DeviceConfig c;
    memset(&c, 0, sizeof(c));
    strlcpy(c.mac, f.mac, sizeof(c.mac));
    strlcpy(c.name, name.length() ? name.c_str() : (f.name[0] ? f.name : T(typeName(f.type))), sizeof(c.name));
    c.type = f.type;
    deviceConfigDefaults(c);
    memcpy(c.key, key, sizeof(c.key));
    c.hasKey = true;
    if (!store_add(c)) {
      back(T("Niet toegevoegd: de lijst is vol (maximaal 24 apparaten) of het apparaat bestaat al."));
      return;
    }
    logf("[web] apparaat toegevoegd: %s (%s)\n", c.name, c.mac);
    char m[256];
    snprintf(m, sizeof(m), T("Toegevoegd: %s. Na een paar seconden staan de metingen op het scherm."), c.name);
    back(m);
    return;
  }
  if (!store_set_name_key(mac.c_str(), name.c_str(), haveKey ? key : nullptr)) {
    back(T("Niet opgeslagen: het apparaat bestaat niet meer."));
    return;
  }
  logf("[web] %s gewijzigd voor %s\n", haveKey ? "sleutel" : "naam", mac.c_str());
  back(haveKey ? T("Sleutel opgeslagen. Vernieuw over een paar seconden om de status te zien.") : T("Naam opgeslagen."));
}

}  // namespace

String web_esc(const char *s) { return esc(s); }
// Met de taalcode van de gekozen taal; opnieuw gemaakt als de taal wisselt (alleen de servertaak roept dit aan)
const char *web_head() {
  static const char *const C[] = {"nl", "en", "fr", "de", "es", "it", "pt", "sv", "nb", "da", "fi"};
  static String head;
  static int forLang = -1;
  int l = (int)i18n_lang();
  if (l != forLang) {
    head = F("<!doctype html><html lang=");
    head += l < (int)(sizeof(C) / sizeof(C[0])) ? C[l] : "nl";
    head += ">";
    head += FPSTR(HEAD);
    forLang = l;
  }
  return head.c_str();
}

void webkeys_register(WebServer &web, bool (*allowed)()) {
  srv = &web;
  isAllowed = allowed;
  web.on("/sleutels", HTTP_GET, handleList);
  web.on("/sleutel", HTTP_POST, handleSave);
}
