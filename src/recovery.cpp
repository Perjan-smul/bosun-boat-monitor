#include "recovery.h"
#include "psram.h"
#include <Arduino.h>
#include <lvgl.h>
#include "display.h"
#include "i18n.h"
#include "log.h"
#include "ota.h"

// Herstelscherm: na een paar mislukte starts na elkaar start alleen dit. Het gebruikt bewust niets van
// de gewone schermcode (ui.cpp), Bluetooth, NMEA 2000 of de historie: daar kan de fout in zitten.

namespace {

const lv_color_t BG = lv_color_hex(0x0B1C2C), CARD = lv_color_hex(0x13293D), TEXT = lv_color_hex(0xE8F1F8),
                 MUTED = lv_color_hex(0x8AA4B8), RED = lv_color_hex(0xE5534B), ACCENT = lv_color_hex(0x2EC4B6);

bool haveDisplay = false, haveWifi = false;
lv_obj_t *wifiLbl, *wifiBtnLbl, *qr, *installRow;
char qrText[80];

lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  lv_label_set_text(l, text);
  return l;
}

lv_obj_t *button(lv_obj_t *parent, const char *text, lv_color_t bg, lv_event_cb_t cb, lv_obj_t **lbl = nullptr) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_style_bg_color(b, bg, 0);
  lv_obj_set_height(b, 48);
  lv_obj_set_flex_grow(b, 1);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *l = label(b, &lv_font_montserrat_16, TEXT, text);
  lv_obj_center(l);
  if (lbl) *lbl = l;
  return b;
}

lv_obj_t *row(lv_obj_t *parent) {
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_pad_all(r, 0, 0);
  lv_obj_set_style_pad_column(r, 10, 0);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  return r;
}

void refreshWifi() {
  if (!haveDisplay) return;
  char buf[200];
  OtaPhase ph = ota_phase();
  switch (ph) {
    case OtaPhase::Off: snprintf(buf, sizeof(buf), "%s", T("Wifi uit. Zet de wifi-pagina aan om het logboek te downloaden of een nieuwe firmware te installeren.")); break;
    case OtaPhase::Connecting: snprintf(buf, sizeof(buf), "%s", T("Verbinden met het wifi-netwerk...")); break;
    case OtaPhase::Uploading: snprintf(buf, sizeof(buf), "%s", T("Firmware wordt geupload. Niet de stroom afhalen.")); break;
    case OtaPhase::Staged: snprintf(buf, sizeof(buf), "%s", T("Nieuwe firmware ontvangen. Installeren?")); break;
    default:
      if (ota_ap_name()[0])
        snprintf(buf, sizeof(buf), T("Wifi %s\nwachtwoord %s\nOpen http://%s\n(Diagnose = logboek)"), ota_ap_name(), ota_code(), ota_address());
      else
        snprintf(buf, sizeof(buf), T("Open http://%s\ngebruiker boot, wachtwoord %s\n(Diagnose = logboek)"), ota_address(), ota_code());
  }
  lv_label_set_text(wifiLbl, buf);
  lv_label_set_text(wifiBtnLbl, ph == OtaPhase::Off ? T(LV_SYMBOL_WIFI "  Wifi-pagina aan") : T(LV_SYMBOL_WIFI "  Wifi-pagina uit"));

  char q[80] = "";
  if (ph == OtaPhase::Serving && ota_ap_name()[0]) snprintf(q, sizeof(q), "WIFI:T:WPA;S:%s;P:%s;;", ota_ap_name(), ota_code());
  if (q[0] && strcmp(q, qrText) != 0) lv_qrcode_update(qr, q, strlen(q));
  if (q[0]) lv_obj_clear_flag(qr, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(qr, LV_OBJ_FLAG_HIDDEN);
  strlcpy(qrText, q, sizeof(qrText));

  if (ph == OtaPhase::Staged) lv_obj_clear_flag(installRow, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(installRow, LV_OBJ_FLAG_HIDDEN);
}

void buildScreen() {
  lv_obj_t *scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, BG, 0);
  lv_obj_set_style_pad_all(scr, 12, 0);
  lv_obj_set_style_pad_row(scr, 8, 0);
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);

  // Kop met foutcode
  lv_obj_t *head = row(scr);
  label(head, &lv_font_montserrat_20, RED, T(LV_SYMBOL_WARNING "  Bosun kan niet starten"));
  char code[8], buf[256];
  log_error_code(code, sizeof(code));
  lv_obj_t *c = row(scr);
  snprintf(buf, sizeof(buf), T("Foutcode %s"), code);
  label(c, &lv_font_montserrat_28, TEXT, buf);

  snprintf(buf, sizeof(buf),
           T("%s %lu keer na elkaar, tijdens: %s.\nOm een herstartlus te voorkomen draait nu alleen dit "
             "herstelscherm: geen meldingen, Bluetooth of NMEA 2000."),
           log_fail_kind() == 2 ? T("Vastgelopen") : T("Gecrasht"), (unsigned long)log_failures(),
           T(log_stage_name(log_fail_stage())));
  lv_obj_t *expl = label(scr, &lv_font_montserrat_14, MUTED, buf);
  lv_label_set_long_mode(expl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(expl, lv_pct(100));

  // Laatste regels van het logboek
  lv_obj_t *box = lv_obj_create(scr);
  lv_obj_set_size(box, lv_pct(100), 120);
  lv_obj_set_style_bg_color(box, lv_color_hex(0x06121C), 0);
  lv_obj_set_style_border_width(box, 0, 0);
  lv_obj_set_style_pad_all(box, 6, 0);
  constexpr size_t LOGBUF_N = 6200;
  static char *logBuf = psram_alloc<char>(LOGBUF_N);
  size_t n = logBuf ? log_copy(logBuf, LOGBUF_N) : 0;
  if (!logBuf) logBuf = (char *)"";
  const char *tail = n > 900 ? logBuf + n - 900 : logBuf;
  if (tail != logBuf) {
    const char *nl = strchr(tail, '\n');  // vanaf een hele regel
    if (nl) tail = nl + 1;
  }
  lv_obj_t *lg = label(box, &lv_font_montserrat_14, TEXT, tail);
  lv_label_set_long_mode(lg, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lg, lv_pct(100));
  lv_obj_update_layout(box);
  lv_obj_scroll_to_y(box, LV_COORD_MAX, LV_ANIM_OFF);  // nieuwste regels in beeld

  // Wifi: logboek downloaden, backup terugzetten of firmware installeren
  lv_obj_t *w = row(scr);
  wifiLbl = label(w, &lv_font_montserrat_14, TEXT, "");
  lv_label_set_long_mode(wifiLbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_flex_grow(wifiLbl, 1);
  qr = lv_qrcode_create(w, 100, lv_color_black(), lv_color_white());
  lv_obj_set_style_border_color(qr, lv_color_white(), 0);
  lv_obj_set_style_border_width(qr, 4, 0);
  lv_obj_add_flag(qr, LV_OBJ_FLAG_HIDDEN);

  installRow = row(scr);
  button(installRow, T(LV_SYMBOL_OK "  Installeer"), ACCENT, [](lv_event_t *) { ota_confirm(true); });
  button(installRow, T("Afwijzen"), CARD, [](lv_event_t *) { ota_confirm(false); });
  lv_obj_add_flag(installRow, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *btns = row(scr);
  button(btns, T(LV_SYMBOL_REFRESH "  Opnieuw proberen"), CARD, [](lv_event_t *) {
    log_line("[herstel] opnieuw proberen (gekozen op het scherm)");
    delay(100);
    esp_restart();  // een gewone herstart: de telling begint opnieuw
  });
  lv_obj_t *wb = button(btns, "", CARD, [](lv_event_t *) {
    if (ota_phase() == OtaPhase::Off) ota_start();
    else ota_stop();
    refreshWifi();
  }, &wifiBtnLbl);
  if (!haveWifi) lv_obj_add_flag(wb, LV_OBJ_FLAG_HIDDEN);

  lv_scr_load(scr);
  refreshWifi();
}

}  // namespace

void recovery_begin() {
  i18n_begin();  // taal voor het herstelscherm (alleen een NVS-waarde; mag ook al eerder gebeurd zijn)
  logf("[herstel] %lu mislukte starts na elkaar (%s tijdens %s): herstelscherm\n", (unsigned long)log_failures(),
       log_fail_kind() == 2 ? "vastgelopen" : "crash", log_stage_name(log_fail_stage()));
  // Ging het mis bij het scherm of de wifi zelf, dan die overslaan: zo blijft er iets over om te gebruiken
  haveWifi = log_fail_stage() != STAGE_OTA;
  haveDisplay = log_fail_stage() != STAGE_DISPLAY;
  if (haveWifi) {
    log_stage(STAGE_OTA);
    feedLoopWDT();
    ota_begin();
  }
  if (haveDisplay) {
    log_stage(STAGE_DISPLAY);
    feedLoopWDT();
    display_begin();
    display_keep_awake(true);
    buildScreen();
  }
  log_stage(STAGE_RUNNING);
  // Zonder scherm meteen de wifi aan: anders is er geen enkele manier om bij het logboek te komen
  if (haveWifi && !haveDisplay) ota_start();
}

void recovery_loop() {
  static uint32_t last = 0;
  ota_set_alarm_active(false);
  ota_tick();
  if (haveDisplay) {
    lv_timer_handler();
    if (millis() - last > 500) {
      last = millis();
      refreshWifi();
    }
  }
  delay(5);
}
