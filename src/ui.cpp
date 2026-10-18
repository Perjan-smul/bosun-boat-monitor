#include "ui.h"
#include <lvgl.h>
#include <math.h>
#include <stdarg.h>
#include "alarms.h"
#include "baro.h"
#include "clock.h"
#include "display.h"
#include "energy.h"
#include "history.h"
#include "n2k.h"
#include "ota.h"
#include "quarantine.h"
#include "sdcard.h"
#include "buzzer.h"
#include "settings.h"
#include "psram.h"
#include "theme.h"
#include <Preferences.h>
#include "store.h"
#include "i18n.h"
#include "log.h"
#include "version.h"

// Tekst aan een buffer toevoegen. Anders dan "n += snprintf(buf + n, size - n, ...)" kan dit niet
// over het einde schrijven als de tekst niet past (bv. een langere vertaling): n blijft binnen de buffer.
int appendf(char *buf, size_t size, int n, const char *fmt, ...) {
  if (n < 0) n = 0;
  if (!size || (size_t)n >= size - 1) return size ? (int)size - 1 : 0;
  va_list ap;
  va_start(ap, fmt);
  int r = vsnprintf(buf + n, size - n, fmt, ap);
  va_end(ap);
  if (r > 0) n += r;
  return (size_t)n >= size ? (int)size - 1 : n;
}

// Gloeiende stipjes (src/glow.c, C-bestand)
extern "C" {
LV_IMG_DECLARE(glow_star);
LV_IMG_DECLARE(glow_core);
LV_IMG_DECLARE(glow_dot);
// Opstartscherm (src/splash.c)
LV_IMG_DECLARE(splash_mark);
LV_IMG_DECLARE(splash_mark_glow);
LV_IMG_DECLARE(splash_word);
LV_IMG_DECLARE(splash_word_glow);
LV_IMG_DECLARE(splash_sub);
}

namespace {

// ---------- Stijl ----------
#define COL_BG lv_color_hex(theme().bg)
#define COL_HEADER lv_color_hex(theme().header)
#define COL_CARD lv_color_hex(theme().card)
#define COL_ACCENT lv_color_hex(theme().accent)
#define COL_TEXT lv_color_hex(theme().text)
#define COL_MUTED lv_color_hex(theme().muted)
#define COL_OK lv_color_hex(theme().ok)
#define COL_WARN lv_color_hex(theme().warn)
#define COL_ERR lv_color_hex(theme().err)
#define COL_INK lv_color_hex(theme().ink)  // tekst op een gekleurd vlak

constexpr int W = 480;
constexpr int HEADER_H = 56;
constexpr uint32_t STALE_WARN_S = 30;
constexpr uint32_t STALE_ERR_S = 120;

enum class Page { Dashboard, Scan, Edit, Detail, Settings, ClockSet, N2kLimits, Ota, WifiSet, Language };
Page page;

// ---------- Hulpfuncties ----------

lv_obj_t *makeScreen() {
  lv_obj_t *scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, COL_BG, 0);
  lv_obj_set_style_text_color(scr, COL_TEXT, 0);  // alles erft de tekstkleur van het thema
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

void showScreen(lv_obj_t *scr, Page p) {
  page = p;
  lv_obj_t *old = lv_scr_act();
  lv_scr_load(scr);
  // Pas na afloop van het huidige event opruimen (we zitten vaak in een klik-callback)
  if (old && old != scr) lv_obj_del_async(old);
}

lv_obj_t *makeLabel(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text = "") {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  lv_label_set_text(l, text);
  return l;
}

lv_obj_t *makeButton(lv_obj_t *parent, const char *text, lv_color_t color, lv_event_cb_t cb, void *user = nullptr) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_style_bg_color(b, color, 0);
  lv_obj_t *l = makeLabel(b, &lv_font_montserrat_16, lv_color_hex(theme_text_on(lv_color_to32(color) & 0xFFFFFF)), text);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

// Kopbalk met optionele terugknop; geeft de titel-label terug
lv_obj_t *makeHeader(lv_obj_t *scr, const char *title, lv_event_cb_t backCb) {
  lv_obj_t *h = lv_obj_create(scr);
  lv_obj_set_size(h, W, HEADER_H);
  lv_obj_set_pos(h, 0, 0);
  lv_obj_set_style_bg_color(h, COL_HEADER, 0);
  lv_obj_set_style_border_width(h, 0, 0);
  lv_obj_set_style_radius(h, 0, 0);
  lv_obj_set_style_pad_all(h, 0, 0);
  lv_obj_clear_flag(h, LV_OBJ_FLAG_SCROLLABLE);

  int x = 16;
  if (backCb) {
    lv_obj_t *b = lv_btn_create(h);
    lv_obj_set_size(b, 48, 40);
    lv_obj_align(b, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t *l = makeLabel(b, &lv_font_montserrat_20, COL_TEXT, LV_SYMBOL_LEFT);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, backCb, LV_EVENT_CLICKED, nullptr);
    x = 60;
  }
  lv_obj_t *t = makeLabel(h, &lv_font_montserrat_20, COL_TEXT, title);
  lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
  lv_obj_set_width(t, W - x - 140);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, x, 0);
  return t;
}

lv_obj_t *headerOf(lv_obj_t *scr) { return lv_obj_get_child(scr, 0); }

const char *WEEKDAYS[] = {TN("zo"), TN("ma"), TN("di"), TN("wo"), TN("do"), TN("vr"), TN("za")};
const char *MONTHS[] = {TN("jan"), TN("feb"), TN("mrt"), TN("apr"), TN("mei"), TN("jun"), TN("jul"), TN("aug"), TN("sep"), TN("okt"), TN("nov"), TN("dec")};

// "15:04" van een UTC-tijdstip in de eigen tijd van de klok
void formatHM(char *buf, size_t len, uint32_t epoch) {
  struct tm t;
  if (clock_local(epoch, t)) snprintf(buf, len, "%02d:%02d", t.tm_hour, t.tm_min);
  else snprintf(buf, len, "--:--");
}

const char *weekdayOf(uint32_t epoch) {
  struct tm t;
  return clock_local(epoch, t) ? T(WEEKDAYS[t.tm_wday]) : "";
}

// Status-tekst + kleur op basis van leeftijd van de laatste ontvangst
void deviceStatus(const DeviceState &d, char *buf, size_t len, lv_color_t &color) {
  if (quarantine_ble_blocked(d.cfg.mac)) {
    snprintf(buf, len, T(LV_SYMBOL_CLOSE " Uitgeschakeld (crashes)"));
    color = COL_ERR;
    return;
  }
  if (d.last == ParseResult::BadKey) {
    snprintf(buf, len, T(LV_SYMBOL_WARNING " Sleutel onjuist"));
    color = COL_ERR;
    return;
  }
  if (d.last == ParseResult::NeedKey) {
    snprintf(buf, len, T(LV_SYMBOL_WARNING " Sleutel nodig"));
    color = COL_WARN;
    return;
  }
  if (!d.lastSeenMs) {
    snprintf(buf, len, T("Wachten op signaal..."));
    color = COL_MUTED;
    return;
  }
  uint32_t age = (millis() - d.lastSeenMs) / 1000;
  if (age >= STALE_ERR_S) {
    snprintf(buf, len, T(LV_SYMBOL_WARNING " Geen signaal (%lu min)"), (unsigned long)(age / 60));
    color = COL_ERR;
  } else {
    snprintf(buf, len, LV_SYMBOL_BLUETOOTH " %lus  %d dBm", (unsigned long)age, d.rssi);
    color = age >= STALE_WARN_S ? COL_WARN : COL_OK;
  }
}

void buildDashboard();
void buildScan();
bool scanFromSettings = false;  // Apparaat toevoegen geopend vanuit de instellingen (terugknop)
void buildEdit(bool isNew, int index, const char *mac);
void buildDetail(int index);
void buildSettings();
void buildClockSet();
void buildN2kLimits();
void buildOta();
void buildWifiSet();
void buildLanguage(bool firstStart);
void themeSwitch(lv_obj_t *sw);
void themeDropdown(lv_obj_t *dd);

// ================= Dashboard met swipebare pagina's =================

#define COL_SUN lv_color_hex(theme().sun)
#define COL_LINE lv_color_hex(theme().line)

constexpr int DOTS_H = 28;
constexpr int PAGE_H = 480 - HEADER_H - DOTS_H;
constexpr int MAX_TILES = 64;  // elk apparaat staat op zijn soortpagina en op Apparaten

enum class PageId : uint8_t { Overview, Navigation, Solar, Battery, Engine, Tanks, TankSensors, Climate, Barometer, Chargers, History, Devices, Count };

const char *pageTitle(PageId p) {
  switch (p) {
    case PageId::Overview: return T("Overzicht");
    case PageId::Solar: return T("Zonnepanelen");
    case PageId::Battery: return T("Accu's");
    case PageId::Climate: return T("Klimaat");
    case PageId::Barometer: return T("Barometer");
    case PageId::Chargers: return T("Laders & overig");
    case PageId::Navigation: return T("Navigatie");
    case PageId::Engine: return T("Motor");
    case PageId::Tanks: return T("Tanks");
    case PageId::TankSensors: return T("Tanksensoren");
    case PageId::History: return T("Historie");
    default: return T("Apparaten");
  }
}

enum class Category : uint8_t { Battery, Solar, Charger, Climate, Tank };

Category categoryOf(SensorType t) {
  switch (t) {
    case SensorType::VictronBattery:
    case SensorType::VictronLynx:
    case SensorType::VictronLithium: return Category::Battery;
    case SensorType::VictronBatteryProtect: return Category::Charger;
    case SensorType::VictronSolar: return Category::Solar;
    case SensorType::MopekaPro: return Category::Tank;
    case SensorType::VictronDcDc:
    case SensorType::VictronAcCharger:
    case SensorType::VictronInverter:
    case SensorType::VictronVeBus:
    case SensorType::VictronMultiRs:
    case SensorType::VictronOrionXs:
    case SensorType::VictronDcMeter:
    case SensorType::VictronOther: return Category::Charger;
    default: return Category::Climate;
  }
}

// Momentopname van alle apparaten, eens per seconde ververst
DeviceState *snap;  // in PSRAM
int snapCount;

void loadSnapshot() {
  if (!snap) snap = (DeviceState *)ps_malloc(MAX_DEVICES * sizeof(DeviceState));
  snapCount = 0;
  for (int i = 0; i < MAX_DEVICES && store_get(i, snap[i]); i++) snapCount++;
}

bool hasCategory(Category c) {
  for (int i = 0; i < snapCount; i++)
    if (categoryOf(snap[i].cfg.type) == c) return true;
  return false;
}

PageId currentPage = PageId::Overview;  // blijft bewaard bij terugkeren uit details
PageId pageIds[(int)PageId::Count];
int pageCount;
lv_obj_t *tileview, *pageDots[(int)PageId::Count], *pageTitleLbl, *clockLbl;
uint32_t dashVersion, dashN2kKey;

lv_color_t levelColor(AlarmLevel l) { return l == AlarmLevel::Alarm ? COL_ERR : COL_WARN; }

void markAlarm(lv_obj_t *card, const DeviceState &d) {
  AlarmLevel l = alarms_for_device(d.cfg.mac);
  lv_obj_set_style_border_width(card, l != AlarmLevel::None ? 2 : 0, 0);
  lv_obj_set_style_border_color(card, levelColor(l), 0);
}

void openDetail(lv_event_t *e) { buildDetail((intptr_t)lv_event_get_user_data(e)); }

lv_obj_t *makeCard(lv_obj_t *parent, int w, int h, int devIndex) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, w, h);
  lv_obj_set_style_bg_color(card, COL_CARD, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_radius(card, 12, 0);
  lv_obj_set_style_pad_all(card, 10, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  if (devIndex >= 0) {
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(card, COL_HEADER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(card, openDetail, LV_EVENT_CLICKED, (void *)(intptr_t)devIndex);
  }
  return card;
}

// Pagina-inhoud: verticaal scrollen, horizontaal swipen gaat naar de tileview
lv_obj_t *makePageContainer(lv_obj_t *tile, lv_flex_flow_t flow) {
  lv_obj_set_style_pad_all(tile, 8, 0);
  lv_obj_set_style_pad_row(tile, 8, 0);
  lv_obj_set_style_pad_column(tile, 8, 0);
  lv_obj_set_flex_flow(tile, flow);
  lv_obj_set_scroll_dir(tile, LV_DIR_VER);
  return tile;
}

void setPowerLabel(lv_obj_t *l, float watts) {
  if (isnan(watts)) {
    lv_label_set_text(l, "--");
    lv_obj_set_style_text_color(l, COL_MUTED, 0);
    return;
  }
  lv_label_set_text_fmt(l, "%+d W", (int)lroundf(watts));
  lv_obj_set_style_text_color(l, watts > 1 ? COL_OK : watts < -1 ? COL_WARN : COL_TEXT, 0);
}

void formatTtg(char *buf, size_t len, int32_t mins) {
  if (mins < 0) snprintf(buf, len, "--");
  else if (mins >= 60 * 24 * 10) snprintf(buf, len, T("> 10 dagen"));
  else snprintf(buf, len, T("%ldu %02ldm"), (long)(mins / 60), (long)(mins % 60));
}

// ---------- Generieke apparaattegels (Klimaat, Laders, Apparaten) ----------

struct Tile {
  int dev;
  lv_obj_t *card, *primary, *lines, *status;
};
Tile tiles[MAX_TILES];
int tileCount;

void makeDeviceTile(lv_obj_t *parent, int i) {
  if (tileCount >= MAX_TILES) return;
  lv_obj_t *card = makeCard(parent, 224, 176, i);
  lv_obj_t *name = makeLabel(card, &lv_font_montserrat_16, COL_ACCENT, snap[i].cfg.name);
  lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
  lv_obj_set_width(name, 204);

  Tile &t = tiles[tileCount++];
  t.dev = i;
  t.card = card;
  t.primary = makeLabel(card, &lv_font_montserrat_28, COL_TEXT, "--");
  lv_obj_align(t.primary, LV_ALIGN_TOP_LEFT, 0, 24);
  t.lines = makeLabel(card, &lv_font_montserrat_14, COL_MUTED);
  lv_label_set_long_mode(t.lines, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(t.lines, 204);
  lv_obj_align(t.lines, LV_ALIGN_TOP_LEFT, 0, 62);
  t.status = makeLabel(card, &lv_font_montserrat_14, COL_MUTED);
  lv_label_set_long_mode(t.status, LV_LABEL_LONG_DOT);
  lv_obj_set_width(t.status, 204);
  lv_obj_align(t.status, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void updateTiles() {
  for (int k = 0; k < tileCount; k++) {
    Tile &t = tiles[k];
    const DeviceState &d = snap[t.dev];
    markAlarm(t.card, d);
    lv_label_set_text(t.primary, d.hasData ? T(d.reading.primary) : "--");
    if (d.cfg.type == SensorType::MopekaPro && d.hasData) {  // gekalibreerd: niveau in %, anders de afstand in mm
      float lvl = mopekaLevelPct(d.reading.m.distanceMm, d.cfg.tankFullMm, d.cfg.tankEmptyMm);
      if (!isnan(lvl)) lv_label_set_text_fmt(t.primary, "%.0f %%", lvl);
    }

    char lines[128] = "";
    size_t n = 0;
    for (int v = 0, shown = 0; d.hasData && v < d.reading.count && shown < 3; v++) {
      if (strcmp(d.reading.values[v].text, d.reading.primary) == 0) continue;
      char vt[40];
      reading_value_text(d.reading, v, vt, sizeof(vt));
      n = appendf(lines, sizeof(lines), n, "%s%s: %s", shown ? "\n" : "", T(d.reading.values[v].label), vt);
      shown++;
    }
    lv_label_set_text(t.lines, lines);

    char st[48];
    lv_color_t c;
    deviceStatus(d, st, sizeof(st), c);
    lv_label_set_text(t.status, st);
    lv_obj_set_style_text_color(t.status, c, 0);
  }
}

void buildTilePage(lv_obj_t *tile, int filter /* -1 = alles */) {
  makePageContainer(tile, LV_FLEX_FLOW_ROW_WRAP);
  for (int i = 0; i < snapCount; i++)
    if (filter < 0 || (int)categoryOf(snap[i].cfg.type) == filter) makeDeviceTile(tile, i);
  if (filter < 0 && snapCount == 0) {
    lv_obj_t *l = makeLabel(tile, &lv_font_montserrat_20, COL_MUTED, T("Nog geen apparaten."));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_pad_top(l, 100, 0);
    lv_obj_t *b = makeButton(tile, T(LV_SYMBOL_PLUS "  Apparaat toevoegen"), COL_ACCENT, [](lv_event_t *) { buildScan(); });
    lv_obj_set_size(b, 280, 56);
    lv_obj_t *wrap = lv_obj_get_parent(b);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
  }
}

// ---------- Overzicht: energiestroom (Victron-inhoud, donkere ringmeter-stijl) ----------

#define COL_SHORE lv_color_hex(theme().shore)
#define COL_LOAD lv_color_hex(theme().load)
#define COL_BATTN lv_color_hex(theme().battery)

constexpr int RING_D = 104;

// Volle boog bij dit vermogen; groeit mee als er ooit meer gemeten wordt. Pas aan aan je installatie.
constexpr float SCALE_SUN_W = 400;
constexpr float SCALE_LOAD_W = 500;
constexpr float SCALE_SHORE_W = 1000;

// Pad van een knoop naar de hub: knoop -> hoek -> hub. Stipjes lopen heen of terug.
struct Path {
  lv_point_t p[3];
  lv_obj_t *line;
  lv_obj_t *dot[2];
  int speed;     // animatieduur in ms, 0 = stil
  bool reverse;  // true = van hub naar knoop
};

struct Node {
  lv_obj_t *root, *arc, *icon, *value, *inner, *chip, *sub;
  float peak;  // hoogste waarde sinds opstarten, voor de schaal van de boog
};

Node nodeSun, nodeShore, nodeBatt, nodeLoad;
Path pathSun, pathShore, pathBatt, pathLoad;
lv_obj_t *hub, *hubIcon, *hubText;

// Ringen iets omlaag: bovenaan staat de weerregel (temperatuur, vochtigheid, luchtdruk)
const lv_point_t POS_SUN = {90, 112}, POS_LOAD = {390, 112}, POS_BATT = {90, 296}, POS_SHORE = {390, 296},
                 POS_HUB = {240, 204};

// Weerregel: links temperatuur, midden vochtigheid, rechts luchtdruk (boven de kolommen van de ringen)
constexpr int WX_Y = 6, WX_H = 30;
#define SYM_THERMO "\xEF\x8B\x89"       // U+F2C9 thermometer-half
#define SYM_DROP "\xEF\x81\x83"         // U+F043 tint
#define SYM_SUN "\xEF\x86\x85"          // U+F185 sun
#define SYM_CLOUD_SUN "\xEF\x9B\x84"    // U+F6C4 cloud-sun
#define SYM_CLOUD "\xEF\x83\x82"        // U+F0C2 cloud
#define SYM_CLOUD_RAIN "\xEF\x9C\xBD"   // U+F73D cloud-rain
#define SYM_STORM "\xEF\x9D\x80"        // U+F740 cloud-showers-heavy
#define SYM_ARROW_RIGHT "\xEF\x81\xA1"  // U+F061
#define SYM_ARROW_UP "\xEF\x81\xA2"     // U+F062
#define SYM_ARROW_DOWN "\xEF\x81\xA3"   // U+F063
#define COL_DROP lv_color_hex(theme().drop)
#define COL_RAIN lv_color_hex(theme().rain)

struct WxCell {
  lv_obj_t *root, *icon, *value;
};
WxCell wxTemp, wxHum, wxBaro;

int pathLength(const Path &p) {
  return abs(p.p[1].x - p.p[0].x) + abs(p.p[1].y - p.p[0].y) + abs(p.p[2].x - p.p[1].x) + abs(p.p[2].y - p.p[1].y);
}

void dotAnim(void *var, int32_t v) {
  auto *dot = (lv_obj_t *)var;
  auto *p = (Path *)lv_obj_get_user_data(dot);
  int len = pathLength(*p);
  int pos = len * v / 1000;
  if (p->reverse) pos = len - pos;
  int seg1 = abs(p->p[1].x - p->p[0].x) + abs(p->p[1].y - p->p[0].y);
  const lv_point_t &a = pos <= seg1 ? p->p[0] : p->p[1];
  const lv_point_t &b = pos <= seg1 ? p->p[1] : p->p[2];
  int segLen = max(1, abs(b.x - a.x) + abs(b.y - a.y));
  int t = pos <= seg1 ? pos : pos - seg1;
  int r = (theme().glowStar ? glow_star.header.w : glow_dot.header.w) / 2;
  lv_obj_set_pos(dot, a.x + (b.x - a.x) * t / segLen - r, a.y + (b.y - a.y) * t / segLen - r);
}

void makePath(lv_obj_t *parent, Path &p, lv_point_t node, lv_point_t hubEdge, int cornerX) {
  bool right = node.x > POS_HUB.x;
  p.p[0] = {(lv_coord_t)(node.x + (right ? -RING_D / 2 : RING_D / 2)), node.y};
  p.p[1] = {(lv_coord_t)cornerX, node.y};
  p.p[2] = {(lv_coord_t)cornerX, hubEdge.y};
  p.speed = -1;
  p.reverse = false;
  p.line = lv_line_create(parent);
  lv_line_set_points(p.line, p.p, 3);
  lv_obj_set_style_line_width(p.line, 2, 0);
  lv_obj_set_style_line_color(p.line, COL_LINE, 0);
  lv_obj_add_flag(p.line, LV_OBJ_FLAG_IGNORE_LAYOUT);
  // Stipje = gloeiende ster in de kleur van de stroom (A8-afbeelding, ingekleurd) met een witte kern
  // Licht en Nacht: een zacht stipje zonder witte kern (op wit valt een ster weg, 's nachts verblindt wit)
  for (auto &d : p.dot) {
    d = lv_img_create(parent);
    lv_img_set_src(d, theme().glowStar ? &glow_star : &glow_dot);
    lv_obj_set_style_img_recolor_opa(d, LV_OPA_COVER, 0);
    if (theme().glowStar) {
      lv_obj_t *core = lv_img_create(d);
      lv_img_set_src(core, &glow_core);
      lv_obj_set_style_img_recolor(core, lv_color_white(), 0);
      lv_obj_set_style_img_recolor_opa(core, LV_OPA_COVER, 0);
      lv_obj_center(core);
    }
    lv_obj_clear_flag(d, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_user_data(d, &p);
  }
}

// Meer vermogen = snellere stipjes. reverse: stroom van hub naar knoop.
void setPath(Path &p, float watts, bool reverse, bool visible, lv_color_t color) {
  if (visible) lv_obj_clear_flag(p.line, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(p.line, LV_OBJ_FLAG_HIDDEN);
  int speed = 0;
  if (visible && watts >= 1) speed = watts < 50 ? 3000 : watts < 200 ? 2100 : watts < 600 ? 1400 : 900;
  for (auto d : p.dot) lv_obj_set_style_img_recolor(d, color, 0);
  if (speed == p.speed && reverse == p.reverse) return;
  p.speed = speed;
  p.reverse = reverse;
  for (int i = 0; i < 2; i++) {
    lv_anim_del(p.dot[i], dotAnim);
    if (!speed) {
      lv_obj_add_flag(p.dot[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(p.dot[i], LV_OBJ_FLAG_HIDDEN);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, p.dot[i]);
    lv_anim_set_exec_cb(&a, dotAnim);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_time(&a, speed);
    lv_anim_set_delay(&a, i * speed / 2);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
    dotAnim(p.dot[i], 0);
  }
}

void makeNode(lv_obj_t *parent, Node &n, lv_point_t pos, lv_color_t color, const char *icon, const char *label) {
  n.peak = 100;
  n.root = lv_obj_create(parent);
  lv_obj_add_flag(n.root, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(n.root, 150, RING_D + 44);
  lv_obj_set_pos(n.root, pos.x - 75, pos.y - RING_D / 2);
  lv_obj_set_style_bg_opa(n.root, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(n.root, 0, 0);
  lv_obj_set_style_pad_all(n.root, 0, 0);
  lv_obj_clear_flag(n.root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  // Ringmeter: donkere achtergrond, boog in de kleur van de knoop
  lv_obj_t *disc = lv_obj_create(n.root);
  lv_obj_set_size(disc, RING_D - 16, RING_D - 16);
  lv_obj_align(disc, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(disc, COL_CARD, 0);
  lv_obj_set_style_border_width(disc, 0, 0);
  lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  n.arc = lv_arc_create(n.root);
  lv_obj_set_size(n.arc, RING_D, RING_D);
  lv_obj_align(n.arc, LV_ALIGN_TOP_MID, 0, 0);
  lv_arc_set_rotation(n.arc, 135);
  lv_arc_set_bg_angles(n.arc, 0, 270);
  lv_arc_set_range(n.arc, 0, 1000);
  lv_obj_remove_style(n.arc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(n.arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(n.arc, 5, LV_PART_MAIN);
  lv_obj_set_style_arc_width(n.arc, 5, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(n.arc, COL_LINE, LV_PART_MAIN);
  lv_obj_set_style_arc_color(n.arc, color, LV_PART_INDICATOR);

  n.icon = makeLabel(n.root, &lv_font_montserrat_20, color, icon);
  lv_obj_align(n.icon, LV_ALIGN_TOP_MID, 0, 18);
  n.value = makeLabel(n.root, &lv_font_montserrat_20, COL_TEXT, "--");
  lv_obj_align(n.value, LV_ALIGN_TOP_MID, 0, 42);
  n.inner = makeLabel(n.root, &lv_font_montserrat_14, COL_MUTED, "");
  lv_obj_align(n.inner, LV_ALIGN_TOP_MID, 0, 66);

  // Label-chip onder de ring, zoals "Pv" / "Bat" in het voorbeeld
  n.chip = makeLabel(n.root, &lv_font_montserrat_14, COL_INK, label);
  lv_obj_set_style_bg_color(n.chip, color, 0);
  lv_obj_set_style_bg_opa(n.chip, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(n.chip, 4, 0);
  lv_obj_set_style_pad_hor(n.chip, 8, 0);
  lv_obj_set_style_pad_ver(n.chip, 1, 0);
  lv_obj_align(n.chip, LV_ALIGN_TOP_MID, 0, RING_D + 2);

  n.sub = makeLabel(n.root, &lv_font_montserrat_14, COL_MUTED, "");
  lv_obj_align(n.sub, LV_ALIGN_TOP_MID, 0, RING_D + 24);
}

// LVGL heeft geen zon-symbool: kleine zon van een cirkel met 8 stralen
void makeSunIcon(lv_obj_t *label) {
  lv_label_set_text(label, "");
  lv_obj_set_size(label, 26, 26);
  lv_obj_t *core = lv_obj_create(label);
  lv_obj_set_size(core, 12, 12);
  lv_obj_center(core);
  lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(core, COL_SUN, 0);
  lv_obj_set_style_border_width(core, 0, 0);
  static lv_point_t rays[8][2];
  for (int i = 0; i < 8; i++) {
    float a = i * 3.14159265f / 4;
    rays[i][0] = {(lv_coord_t)(13 + cosf(a) * 8.5f), (lv_coord_t)(13 + sinf(a) * 8.5f)};
    rays[i][1] = {(lv_coord_t)(13 + cosf(a) * 12.5f), (lv_coord_t)(13 + sinf(a) * 12.5f)};
    lv_obj_t *ray = lv_line_create(label);
    lv_line_set_points(ray, rays[i], 2);
    lv_obj_set_style_line_width(ray, 2, 0);
    lv_obj_set_style_line_color(ray, COL_SUN, 0);
    lv_obj_set_style_line_rounded(ray, true, 0);
  }
}

void showNode(Node &n, bool visible) {
  if (visible) lv_obj_clear_flag(n.root, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(n.root, LV_OBJ_FLAG_HIDDEN);
}

// Boog op schaal van het hoogste vermogen sinds opstarten
void setNodePower(Node &n, float watts) {
  if (isnan(watts)) {
    lv_label_set_text(n.value, "--");
    lv_arc_set_value(n.arc, 0);
    return;
  }
  n.peak = max(n.peak, watts);
  lv_label_set_text_fmt(n.value, "%d W", (int)lroundf(watts));
  lv_arc_set_value(n.arc, (int)(max(watts, 0.0f) * 1000 / n.peak));
}

N2kState n2k;  // NMEA 2000-momentopname, eens per seconde ververst
int n2kBattCount();
bool hasTemperature(SensorType t);

void makeWxCell(lv_obj_t *parent, WxCell &c, int centerX, lv_color_t color, const char *icon) {
  c.root = lv_obj_create(parent);
  lv_obj_add_flag(c.root, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(c.root, 156, WX_H);
  lv_obj_set_pos(c.root, centerX - 78, WX_Y);
  lv_obj_set_style_bg_opa(c.root, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(c.root, 0, 0);
  lv_obj_set_style_pad_all(c.root, 0, 0);
  lv_obj_set_style_pad_column(c.root, 8, 0);
  lv_obj_set_flex_flow(c.root, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(c.root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(c.root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  c.icon = makeLabel(c.root, &lv_font_montserrat_20, color, icon);
  c.value = makeLabel(c.root, &lv_font_montserrat_20, COL_TEXT, "--");
}

// Icoon naar de luchtdruk, zoals de woorden op een ouderwetse barometer (mooi weer ... storm)
const char *baroIcon(float hPa, lv_color_t &color) {
  if (hPa >= 1022) { color = COL_SUN; return SYM_SUN; }
  if (hPa >= 1009) { color = COL_SUN; return SYM_CLOUD_SUN; }
  if (hPa >= 1000) { color = COL_MUTED; return SYM_CLOUD; }
  if (hPa >= 990) { color = COL_RAIN; return SYM_CLOUD_RAIN; }
  color = COL_WARN;
  return SYM_STORM;
}

void buildWeatherRow(lv_obj_t *tile) {
  makeWxCell(tile, wxTemp, POS_SUN.x, COL_WARN, SYM_THERMO);
  makeWxCell(tile, wxHum, POS_HUB.x, COL_DROP, SYM_DROP);
  makeWxCell(tile, wxBaro, POS_LOAD.x, COL_SUN, SYM_CLOUD_SUN);
}

void showWx(WxCell &c, bool visible) {
  if (visible) lv_obj_clear_flag(c.root, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(c.root, LV_OBJ_FLAG_HIDDEN);
}

void updateWeatherRow() {
  // Temperatuur en vochtigheid van de eerste klimaatsensor (volgorde van de apparatenlijst)
  // of de sensor die in de instellingen gekozen is (meet die even niet, dan "--")
  constexpr uint32_t FRESH_MS = 600000;  // binnen 10 min gemeten
  auto fresh = [](const DeviceState &d) {
    return d.hasData && d.lastSeenMs && millis() - d.lastSeenMs < FRESH_MS && !isnan(d.reading.m.tempC);
  };
  const char *chosen = settings().overviewSensor;
  int pick = -1;
  bool anyTemp = false;
  for (int i = 0; i < snapCount; i++)
    if (hasTemperature(snap[i].cfg.type)) {
      anyTemp = true;
      if (chosen[0] && strcasecmp(snap[i].cfg.mac, chosen) == 0) pick = i;
    }
  if (pick < 0)  // automatisch, of de gekozen sensor is verwijderd
    for (int i = 0; i < snapCount && pick < 0; i++)
      if (hasTemperature(snap[i].cfg.type) && fresh(snap[i])) pick = i;
  float t = NAN, h = NAN;
  if (pick >= 0 && fresh(snap[pick])) {
    t = snap[pick].reading.m.tempC;
    h = snap[pick].reading.m.humidity;
  }
  // Geen Bluetooth-meting: temperatuur en vocht van NMEA 2000 (binnen/kajuit eerst, anders buiten)
  bool n2kWx = false;
  auto n2kOk = [](uint32_t ms) { return ms && millis() - ms < 60000; };
  if (isnan(t)) {
    for (uint8_t want : {(uint8_t)4, (uint8_t)2, (uint8_t)1})  // kajuit, binnen, buiten
      for (auto &x : n2k.temps)
        if (isnan(t) && n2kOk(x.ms) && x.source == want) t = x.c;
    n2kWx |= !isnan(t);
  }
  if (isnan(h)) {
    for (uint8_t want : {(uint8_t)0, (uint8_t)1})  // binnen, buiten
      for (auto &x : n2k.humids)
        if (isnan(h) && n2kOk(x.ms) && x.source == want) h = x.pct;
    n2kWx |= !isnan(h);
  }
  showWx(wxTemp, anyTemp || n2kWx);
  showWx(wxHum, anyTemp || n2kWx);
  if (isnan(t)) lv_label_set_text(wxTemp.value, "--");
  else lv_label_set_text_fmt(wxTemp.value, "%.1f °C", t);
  if (isnan(h)) lv_label_set_text(wxHum.value, "--");
  else lv_label_set_text_fmt(wxHum.value, "%d %%", (int)lroundf(h));

  // Luchtdruk met de trend over 3 uur als pijl
  showWx(wxBaro, baro_available());
  BaroInfo b = baro_info();
  if (isnan(b.hPa)) {
    lv_label_set_text(wxBaro.value, "--");
    lv_label_set_text(wxBaro.icon, SYM_CLOUD);
    lv_obj_set_style_text_color(wxBaro.icon, COL_MUTED, 0);
    return;
  }
  lv_color_t col;
  lv_label_set_text(wxBaro.icon, baroIcon(b.hPa, col));
  lv_obj_set_style_text_color(wxBaro.icon, col, 0);
  const char *arrow = "";
  switch (b.tendency) {
    case BaroTendency::Steady: arrow = "  " SYM_ARROW_RIGHT; break;
    case BaroTendency::RisingSlow:
    case BaroTendency::Rising:
    case BaroTendency::RisingFast:
    case BaroTendency::RisingVeryFast: arrow = "  " SYM_ARROW_UP; break;
    case BaroTendency::Unknown: break;
    default: arrow = "  " SYM_ARROW_DOWN; break;
  }
  lv_label_set_text_fmt(wxBaro.value, "%d%s", (int)lroundf(b.hPa), arrow);
  // Pijl kleurt mee met de stormwaarschuwing
  lv_color_t vc = COL_TEXT;
  const Settings &s = settings();
  if (!isnan(b.diff3h) && !isnan(s.stormWarn) && -b.diff3h >= s.stormWarn)
    vc = !isnan(s.stormAlarm) && -b.diff3h >= s.stormAlarm ? COL_ERR : COL_WARN;
  lv_obj_set_style_text_color(wxBaro.value, vc, 0);
}

void buildOverview(lv_obj_t *tile) {
  buildWeatherRow(tile);
  lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  const lv_point_t hubTop = {POS_HUB.x, (lv_coord_t)(POS_HUB.y - 42)}, hubBot = {POS_HUB.x, (lv_coord_t)(POS_HUB.y + 42)};
  // Lijnen eerst, zodat knopen en hub erbovenop liggen
  makePath(tile, pathSun, POS_SUN, hubTop, 222);
  makePath(tile, pathLoad, POS_LOAD, hubTop, 258);
  makePath(tile, pathBatt, POS_BATT, hubBot, 222);
  makePath(tile, pathShore, POS_SHORE, hubBot, 258);

  makeNode(tile, nodeSun, POS_SUN, COL_SUN, "", T("Zon"));
  nodeSun.peak = SCALE_SUN_W;
  makeSunIcon(nodeSun.icon);
  lv_obj_align(nodeSun.icon, LV_ALIGN_TOP_MID, 0, 16);
  makeNode(tile, nodeLoad, POS_LOAD, COL_LOAD, LV_SYMBOL_HOME, T("Verbruik"));
  nodeLoad.peak = SCALE_LOAD_W;
  makeNode(tile, nodeBatt, POS_BATT, COL_BATTN, LV_SYMBOL_BATTERY_FULL, T("Accu"));
  makeNode(tile, nodeShore, POS_SHORE, COL_SHORE, LV_SYMBOL_POWER, T("Wal"));
  nodeShore.peak = SCALE_SHORE_W;

  hub = lv_obj_create(tile);
  lv_obj_add_flag(hub, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(hub, 84, 84);
  lv_obj_set_pos(hub, POS_HUB.x - 42, POS_HUB.y - 42);
  lv_obj_set_style_radius(hub, 14, 0);
  lv_obj_set_style_bg_color(hub, COL_CARD, 0);
  lv_obj_set_style_border_color(hub, COL_LINE, 0);
  lv_obj_set_style_border_width(hub, 2, 0);
  lv_obj_set_style_pad_all(hub, 0, 0);
  lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  hubIcon = makeLabel(hub, &lv_font_montserrat_28, COL_MUTED, LV_SYMBOL_CHARGE);
  lv_obj_align(hubIcon, LV_ALIGN_CENTER, 0, -12);
  hubText = makeLabel(hub, &lv_font_montserrat_14, COL_TEXT, "");
  lv_obj_align(hubText, LV_ALIGN_CENTER, 0, 20);
}

const char *batterySymbol(float soc) {
  if (isnan(soc)) return LV_SYMBOL_BATTERY_FULL;
  if (soc > 85) return LV_SYMBOL_BATTERY_FULL;
  if (soc > 60) return LV_SYMBOL_BATTERY_3;
  if (soc > 35) return LV_SYMBOL_BATTERY_2;
  if (soc > 10) return LV_SYMBOL_BATTERY_1;
  return LV_SYMBOL_BATTERY_EMPTY;
}

void updateOverview() {
  updateWeatherRow();
  EnergySummary e = energy_summarize(snap, snapCount);
  float pv = e.pv, solarOut = e.solarOut, yield = e.yield, shoreOut = e.shoreOut, battW = e.battW;
  float battV = e.battV, battA = e.battA, soc = e.soc, load = e.load;
  int32_t ttg = e.ttg;
  bool haveSolar = e.haveSolar, haveShore = e.haveShore, haveShunt = e.haveShunt;
  uint8_t chargeState = e.chargeState;

  bool anySolar = false, anyShore = false, anyShunt = false;
  for (int i = 0; i < snapCount; i++) {
    anySolar |= snap[i].cfg.type == SensorType::VictronSolar || snap[i].cfg.type == SensorType::VictronMultiRs;
    anyShore |= snap[i].cfg.type == SensorType::VictronAcCharger || isVictronMulti(snap[i].cfg.type);
    anyShunt |= isShunt(snap[i].cfg.type);
  }
  float battFlow = haveShunt ? battW : solarOut + shoreOut;  // zonder shunt: aannemen dat alles naar de accu gaat

  // Zon
  showNode(nodeSun, anySolar);
  setNodePower(nodeSun, haveSolar ? pv : NAN);
  lv_label_set_text(nodeSun.inner, haveSolar ? "PV" : "");
  if (haveSolar) lv_label_set_text_fmt(nodeSun.sub, T("%.2f kWh vandaag"), yield);
  else lv_label_set_text(nodeSun.sub, T("geen signaal"));

  // Wal
  showNode(nodeShore, anyShore);
  setNodePower(nodeShore, haveShore ? shoreOut : NAN);
  lv_label_set_text(nodeShore.inner, T("lader"));
  lv_label_set_text(nodeShore.sub, haveShore ? (shoreOut > 1 ? T("aangesloten") : T("standby")) : T("geen signaal"));

  // Verbruik
  showNode(nodeLoad, anyShunt);
  setNodePower(nodeLoad, load);
  lv_label_set_text(nodeLoad.inner, "DC");
  lv_label_set_text(nodeLoad.sub, "");

  // Accu: boog = SoC, kleur naar laadtoestand
  lv_label_set_text(nodeBatt.icon, batterySymbol(soc));
  if (!isnan(soc)) {
    lv_label_set_text_fmt(nodeBatt.value, "%d%%", (int)lroundf(soc));
    lv_arc_set_value(nodeBatt.arc, (int)(soc * 10));
  } else {
    if (!isnan(battV)) lv_label_set_text_fmt(nodeBatt.value, "%.1fV", battV);
    else lv_label_set_text(nodeBatt.value, "--");
    lv_arc_set_value(nodeBatt.arc, 0);
  }
  lv_color_t socCol = isnan(soc) ? COL_BATTN : soc > 50 ? COL_OK : soc > 20 ? COL_WARN : COL_ERR;
  lv_obj_set_style_arc_color(nodeBatt.arc, socCol, LV_PART_INDICATOR);
  if (haveShunt) {
    lv_label_set_text_fmt(nodeBatt.inner, "%+d W", (int)lroundf(battW));
    lv_label_set_text_fmt(nodeBatt.sub, "%.2f V  %+.1f A", battV, battA);
  } else {
    lv_label_set_text(nodeBatt.inner, "");
    lv_label_set_text(nodeBatt.sub, isnan(battV) ? "" : T("geen shunt"));
  }

  // Stroompaden (pad loopt van knoop naar hub; reverse = van hub naar knoop)
  setPath(pathSun, solarOut, false, anySolar, COL_SUN);
  setPath(pathShore, shoreOut, false, anyShore, COL_SHORE);
  setPath(pathBatt, fabsf(battFlow), battFlow > 0, true, battFlow > 0 ? (shoreOut > solarOut ? COL_SHORE : COL_SUN) : COL_BATTN);
  setPath(pathLoad, isnan(load) ? 0 : load, true, anyShunt, battFlow < 0 ? COL_BATTN : COL_SUN);

  // Hub: laadfase zoals Victron ("Absorptie"), anders ontlaadtijd
  if (chargeState != 0xFF) {
    lv_label_set_text(hubText, T(victronStateName(chargeState)));
    lv_obj_set_style_text_color(hubIcon, COL_OK, 0);
  } else if (haveShunt && battW < -1) {
    char t[20];
    formatTtg(t, sizeof(t), ttg);
    lv_label_set_text(hubText, ttg >= 0 ? t : T("Ontladen"));
    lv_obj_set_style_text_color(hubIcon, COL_WARN, 0);
  } else {
    lv_label_set_text(hubText, haveShunt || haveSolar ? T("Rust") : "--");
    lv_obj_set_style_text_color(hubIcon, COL_MUTED, 0);
  }
}

// ---------- Zonnepanelen ----------

struct SolarCard {
  int dev;
  lv_obj_t *card, *pv, *state, *vals, *status;
};
SolarCard solarCards[MAX_DEVICES];
int solarCount;
lv_obj_t *solarTotal;

lv_color_t stateColor(uint8_t s) {
  switch (s) {
    case 3: return COL_WARN;                   // bulk
    case 4: case 7: case 247: return COL_SUN;  // absorptie, egaliseren
    case 5: case 6: return COL_OK;             // float, opslag
    case 2: return COL_ERR;                    // storing
    default: return COL_MUTED;
  }
}

void buildSolarPage(lv_obj_t *tile) {
  makePageContainer(tile, LV_FLEX_FLOW_COLUMN);
  solarTotal = makeLabel(tile, &lv_font_montserrat_16, COL_TEXT, "");
  solarCount = 0;
  for (int i = 0; i < snapCount; i++) {
    if (snap[i].cfg.type != SensorType::VictronSolar) continue;
    SolarCard &c = solarCards[solarCount++];
    c.dev = i;
    lv_obj_t *card = makeCard(tile, 464, 128, i);
    c.card = card;
    lv_obj_t *name = makeLabel(card, &lv_font_montserrat_16, COL_ACCENT, snap[i].cfg.name);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, 280);

    c.state = makeLabel(card, &lv_font_montserrat_14, COL_INK, "");
    lv_obj_set_style_bg_opa(c.state, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c.state, 10, 0);
    lv_obj_set_style_pad_hor(c.state, 10, 0);
    lv_obj_set_style_pad_ver(c.state, 2, 0);
    lv_obj_align(c.state, LV_ALIGN_TOP_RIGHT, 0, 0);

    c.pv = makeLabel(card, &lv_font_montserrat_28, COL_SUN, "--");
    lv_obj_align(c.pv, LV_ALIGN_LEFT_MID, 0, 6);

    c.vals = makeLabel(card, &lv_font_montserrat_14, COL_MUTED, "");
    lv_obj_align(c.vals, LV_ALIGN_LEFT_MID, 150, 6);

    c.status = makeLabel(card, &lv_font_montserrat_14, COL_MUTED, "");
    lv_obj_align(c.status, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  }
}

void updateSolarPage() {
  float total = 0, yield = 0;
  for (int k = 0; k < solarCount; k++) {
    SolarCard &c = solarCards[k];
    const DeviceState &d = snap[c.dev];
    markAlarm(c.card, d);
    const Metrics &m = d.reading.m;
    bool ok = d.hasData;
    if (ok && isFresh(d)) {
      if (!isnan(m.pvWatts)) total += m.pvWatts;
      if (!isnan(m.yieldKwh)) yield += m.yieldKwh;
    }

    if (ok && !isnan(m.pvWatts)) lv_label_set_text_fmt(c.pv, "%d W", (int)lroundf(m.pvWatts));
    else lv_label_set_text(c.pv, "--");

    if (ok && m.state != 0xFF) {
      lv_label_set_text(c.state, T(victronStateName(m.state)));
      lv_obj_set_style_bg_color(c.state, stateColor(m.state), 0);
      lv_obj_clear_flag(c.state, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(c.state, LV_OBJ_FLAG_HIDDEN);
    }

    if (ok) {
      char load[24] = "";
      if (!isnan(m.loadAmps)) snprintf(load, sizeof(load), T("\nLoad: %.1f A"), m.loadAmps);
      char v[12] = "--", a[12] = "--", y[12] = "--";
      if (!isnan(m.volts)) snprintf(v, sizeof(v), "%.2f", m.volts);
      if (!isnan(m.amps)) snprintf(a, sizeof(a), "%.1f", m.amps);
      if (!isnan(m.yieldKwh)) snprintf(y, sizeof(y), "%.2f", m.yieldKwh);
      lv_label_set_text_fmt(c.vals, T("Accu: %s V  " LV_SYMBOL_BULLET "  %s A\nVandaag: %s kWh%s"), v, a, y, load);
    } else {
      lv_label_set_text(c.vals, "");
    }

    char st[48];
    lv_color_t col;
    deviceStatus(d, st, sizeof(st), col);
    lv_label_set_text(c.status, st);
    lv_obj_set_style_text_color(c.status, col, 0);
  }
  lv_label_set_text_fmt(solarTotal, T("Totaal %d W  " LV_SYMBOL_BULLET "  vandaag %.2f kWh"), (int)lroundf(total), yield);
}

// ---------- Accu's (SmartShunt, BMV, Battery Sense) ----------

struct BattCard {
  int dev;
  lv_obj_t *card, *arc, *center, *power, *vals, *status;
};
BattCard battCards[MAX_DEVICES];
int battCount;
// Accu's via NMEA 2000 (127508/127506), bv. van een Victron GX, Mastervolt of een andere accumonitor op de bus
struct N2kBattCard {
  int slot;
  lv_obj_t *card, *big, *sub;
};
N2kBattCard n2kBattCards[N2K_BATTS];
int n2kBattCardCount;

void buildBatteryPage(lv_obj_t *tile) {
  makePageContainer(tile, LV_FLEX_FLOW_COLUMN);
  battCount = 0;
  for (int i = 0; i < snapCount; i++) {
    if (!isShunt(snap[i].cfg.type) && snap[i].cfg.type != SensorType::VictronLithium) continue;
    BattCard &c = battCards[battCount++];
    c.dev = i;
    lv_obj_t *card = makeCard(tile, 464, 150, i);
    c.card = card;

    c.arc = lv_arc_create(card);
    lv_obj_set_size(c.arc, 120, 120);
    lv_obj_align(c.arc, LV_ALIGN_LEFT_MID, 0, 0);
    lv_arc_set_rotation(c.arc, 135);
    lv_arc_set_bg_angles(c.arc, 0, 270);
    lv_arc_set_range(c.arc, 0, 1000);
    lv_obj_remove_style(c.arc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(c.arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(c.arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(c.arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(c.arc, COL_LINE, LV_PART_MAIN);

    c.center = makeLabel(card, &lv_font_montserrat_20, COL_TEXT, "--");
    lv_obj_set_style_text_align(c.center, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(c.center, c.arc, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *name = makeLabel(card, &lv_font_montserrat_16, COL_ACCENT, snap[i].cfg.name);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, 300);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 136, 0);

    c.power = makeLabel(card, &lv_font_montserrat_28, COL_TEXT, "--");
    lv_obj_align(c.power, LV_ALIGN_TOP_LEFT, 136, 22);

    c.vals = makeLabel(card, &lv_font_montserrat_14, COL_MUTED, "");
    lv_obj_align(c.vals, LV_ALIGN_TOP_LEFT, 136, 60);

    c.status = makeLabel(card, &lv_font_montserrat_14, COL_MUTED, "");
    lv_obj_align(c.status, LV_ALIGN_BOTTOM_LEFT, 136, 0);
  }
  // Accu's van NMEA 2000, in kaartjes naast elkaar
  n2kBattCardCount = 0;
  if (n2kBattCount()) {
    lv_obj_t *row = lv_obj_create(tile);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < N2K_BATTS; i++) {
      const N2kBattery &b = n2k.batts[i];
      if (!b.ms && !b.detailMs) continue;
      N2kBattCard &c = n2kBattCards[n2kBattCardCount++];
      c.slot = i;
      c.card = makeCard(row, 224, 120, -1);
      char title[40];
      snprintf(title, sizeof(title), T("Accu %u (NMEA 2000)"), b.instance);
      makeLabel(c.card, &lv_font_montserrat_14, COL_ACCENT, title);
      c.big = makeLabel(c.card, &lv_font_montserrat_28, COL_TEXT, "--");
      lv_obj_align(c.big, LV_ALIGN_TOP_LEFT, 0, 22);
      c.sub = makeLabel(c.card, &lv_font_montserrat_14, COL_MUTED, "");
      lv_label_set_long_mode(c.sub, LV_LABEL_LONG_DOT);
      lv_obj_set_size(c.sub, 204, 36);
      lv_obj_align(c.sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
  }
}

void updateN2kBatteries() {
  for (int k = 0; k < n2kBattCardCount; k++) {
    N2kBattCard &c = n2kBattCards[k];
    const N2kBattery &b = n2k.batts[c.slot];
    bool ok = b.ms && millis() - b.ms < 30000, okDet = b.detailMs && millis() - b.detailMs < 30000;
    if (okDet && !isnan(b.soc)) lv_label_set_text_fmt(c.big, "%.0f %%", b.soc);
    else if (ok && !isnan(b.volts)) lv_label_set_text_fmt(c.big, "%.2f V", b.volts);
    else lv_label_set_text(c.big, "--");
    char buf[96];
    size_t n = 0;
    buf[0] = 0;
    if (ok && !isnan(b.volts) && okDet && !isnan(b.soc)) n = appendf(buf, sizeof(buf), n, "%.2f V  ", b.volts);
    if (ok && !isnan(b.amps)) n = appendf(buf, sizeof(buf), n, "%+.1f A  ", b.amps);
    // Sommige apparaten sturen een vaste nepwaarde (bv. 128 °C): alleen een geloofwaardige accutemperatuur tonen
    if (ok && !isnan(b.tempC) && b.tempC > -40 && b.tempC < 90) n = appendf(buf, sizeof(buf), n, "%.0f °C", b.tempC);
    if (okDet && b.ttgMin >= 0 && ok && b.amps < 0) {
      char ttg[20];
      formatTtg(ttg, sizeof(ttg), b.ttgMin);
      n = appendf(buf, sizeof(buf), n, T("\nResterend: %s"), ttg);
    }
    lv_label_set_text(c.sub, buf);
  }
}

void updateBatteryPage() {
  updateN2kBatteries();
  for (int k = 0; k < battCount; k++) {
    BattCard &c = battCards[k];
    const DeviceState &d = snap[c.dev];
    markAlarm(c.card, d);
    const Metrics &m = d.reading.m;
    bool ok = d.hasData;

    float soc = ok ? m.soc : NAN;
    lv_arc_set_value(c.arc, isnan(soc) ? 0 : (int)(soc * 10));
    lv_color_t socCol = isnan(soc) ? COL_MUTED : soc > 50 ? COL_OK : soc > 20 ? COL_WARN : COL_ERR;
    lv_obj_set_style_arc_color(c.arc, socCol, LV_PART_INDICATOR);
    if (!isnan(soc)) lv_label_set_text_fmt(c.center, "%d %%", (int)lroundf(soc));
    else if (ok) lv_label_set_text_fmt(c.center, "%.2f\nV", m.volts);  // Battery Sense: geen SoC
    else lv_label_set_text(c.center, "--");
    lv_obj_align_to(c.center, c.arc, LV_ALIGN_CENTER, 0, 0);

    if (ok && isnan(m.amps)) lv_label_set_text(c.power, "");  // Battery Sense: geen stroom/vermogen
    else setPowerLabel(c.power, ok ? m.watts : NAN);

    if (ok) {
      char buf[160], ttg[20];
      size_t n = isnan(m.amps) ? snprintf(buf, sizeof(buf), "%.2f V", m.volts)
                               : snprintf(buf, sizeof(buf), "%.2f V  " LV_SYMBOL_BULLET "  %+.1f A", m.volts, m.amps);
      if (m.ttgMin >= 0 && m.amps < 0) {
        formatTtg(ttg, sizeof(ttg), m.ttgMin);
        n = appendf(buf, sizeof(buf), n, T("\nResterend: %s"), ttg);
      } else if (m.amps > 0.05f) {
        n = appendf(buf, sizeof(buf), n, T("\nLaden"));
      }
      if (!isnan(m.tempC)) n = appendf(buf, sizeof(buf), n, T("\nTemperatuur: %.1f °C"), m.tempC);
      if (d.cfg.type == SensorType::VictronLithium) {  // celspanningen: laagste en hoogste
        float lo = INFINITY, hi = -INFINITY;
        int cells = 0;
        for (float v : d.reading.cellV)
          if (!isnan(v)) lo = fminf(lo, v), hi = fmaxf(hi, v), cells++;
        if (cells) snprintf(buf + n, sizeof(buf) - n, T("\nCellen %.2f-%.2f V (%d)"), lo, hi, cells);
      }
      lv_label_set_text(c.vals, buf);
    } else {
      lv_label_set_text(c.vals, "");
    }

    char st[48];
    lv_color_t col;
    deviceStatus(d, st, sizeof(st), col);
    lv_label_set_text(c.status, st);
    lv_obj_set_style_text_color(c.status, col, 0);
  }
}

// ---------- NMEA 2000: navigatie, motor, tanks ----------

constexpr uint32_t N2K_FRESH_MS = 10000;
constexpr uint32_t N2K_TANK_FRESH_MS = 30000;

bool n2kFresh(uint32_t ms, uint32_t maxAge = N2K_FRESH_MS) { return ms && millis() - ms < maxAge; }
float knots(float ms) { return ms * 1.943844f; }

lv_color_t tankColor(uint8_t t) {
  const Palette &p = theme();
  switch (t) {
    case 0: return lv_color_hex(p.tank[0]);  // diesel
    case 1: return lv_color_hex(p.tank[1]);  // water
    case 2: return lv_color_hex(p.tank[2]);  // grijs water
    case 5: return lv_color_hex(p.tank[3]);  // vuilwater
    default: return COL_ACCENT;
  }
}

const char *tempSourceName(uint8_t s, uint8_t instance) {
  static const char *n[] = {TN("Zeewater"), TN("Buiten"), TN("Binnen"), TN("Machinekamer"), TN("Kajuit"), TN("Aaskom"), TN("Aasbak"), TN("Koelkast"),
                            TN("Verwarming"), TN("Dauwpunt"), TN("Gevoelstemp."), TN("Gevoelstemp."), TN("Hitte-index"), TN("Vriezer"),
                            TN("Uitlaatgas"), TN("Asafdichting")};
  if (s < 16) return T(n[s]);
  static char buf[16];
  snprintf(buf, sizeof(buf), T("Sensor %u"), instance);
  return buf;
}

// "52°22.123' N"
void formatLatLon(char *buf, size_t len, double v, bool lat) {
  char h = lat ? (v >= 0 ? 'N' : 'S') : (v >= 0 ? 'E' : 'W');
  // Eerst afronden op 0,001 minuut, dan splitsen: anders kan er 60.000' staan
  long milli = lround(fabs(v) * 60000.0);
  int deg = (int)(milli / 60000);
  double min = (milli % 60000) / 1000.0;
  snprintf(buf, len, lat ? "%02d°%06.3f' %c" : "%03d°%06.3f' %c", deg, min, h);
}

// Windhoek t.o.v. de boeg: "35° SB" of "120° BB"
void formatWindAngle(char *buf, size_t len, float a) {
  if (isnan(a)) snprintf(buf, len, "--°");
  else if (a <= 180) snprintf(buf, len, T("%.0f° SB"), a);
  else snprintf(buf, len, T("%.0f° BB"), 360 - a);
}

int engineCount() {
  int n = 0;
  for (auto &e : n2k.engine) n += (e.rapidMs || e.dynMs) ? 1 : 0;
  return n;
}
int tankCount() {
  int n = 0;
  for (auto &t : n2k.tanks) n += t.ms ? 1 : 0;
  return n;
}
int n2kBattCount() {
  int n = 0;
  for (auto &b : n2k.batts) n += (b.ms || b.detailMs) ? 1 : 0;
  return n;
}
bool hasNavData() {
  return n2k.posMs || n2k.cogSogMs || n2k.depthMs || n2k.appWindMs || n2k.trueWindMs || n2k.headingMs || n2k.stwMs ||
         n2k.rudderMs || n2k.attitudeMs;
}

// Verandert als er pagina's of kaarten bij moeten komen: dan wordt het dashboard opnieuw opgebouwd
uint32_t n2kLayoutKey() {
  int temps = 0;
  for (auto &t : n2k.temps) temps += t.ms ? 1 : 0;
  return (hasNavData() ? 1 : 0) | engineCount() << 1 | tankCount() << 4 | min(temps, 3) << 9 | (baro_available() ? 1 : 0) << 11 |
         n2kBattCount() << 12 | (n2k.rudderMs ? 1 : 0) << 15 | (n2k.attitudeMs ? 1 : 0) << 16;
}

// --- Navigatie ---

struct NavCard {
  lv_obj_t *card, *big, *sub;
};

// Rand in de kleur van de melding (oranje/rood), of geen rand
void markLevel(lv_obj_t *obj, AlarmLevel l) {
  lv_obj_set_style_border_width(obj, l != AlarmLevel::None ? 2 : 0, 0);
  lv_obj_set_style_border_color(obj, l == AlarmLevel::Alarm ? COL_ERR : COL_WARN, 0);
}
NavCard navDepth, navSpeed, navCourse, navWind, navPos, navTemp, navRudder, navAttitude;

NavCard makeNavCard(lv_obj_t *parent, const char *title, const lv_font_t *bigFont) {
  lv_obj_t *card = makeCard(parent, 224, 120, -1);
  makeLabel(card, &lv_font_montserrat_14, COL_ACCENT, title);
  NavCard c;
  c.card = card;
  c.big = makeLabel(card, bigFont, COL_TEXT, "--");
  lv_obj_align(c.big, LV_ALIGN_TOP_LEFT, 0, 22);
  c.sub = makeLabel(card, &lv_font_montserrat_14, COL_MUTED, "");
  lv_label_set_long_mode(c.sub, LV_LABEL_LONG_DOT);
  lv_obj_set_size(c.sub, 204, 36);
  lv_obj_align(c.sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  return c;
}

void buildNavPage(lv_obj_t *tile) {
  makePageContainer(tile, LV_FLEX_FLOW_ROW_WRAP);
  navDepth = makeNavCard(tile, T("Diepte"), &lv_font_montserrat_28);
  navSpeed = makeNavCard(tile, T("Snelheid (SOG)"), &lv_font_montserrat_28);
  navCourse = makeNavCard(tile, T("Koers (COG)"), &lv_font_montserrat_28);
  navWind = makeNavCard(tile, T("Wind"), &lv_font_montserrat_28);
  navPos = makeNavCard(tile, T("Positie"), &lv_font_montserrat_20);
  navTemp = makeNavCard(tile, T("Temperatuur"), &lv_font_montserrat_28);
  // Alleen als de bus ze stuurt (stuurautomaat, roerstandgever, kompas met bewegingssensor)
  navRudder = n2k.rudderMs ? makeNavCard(tile, T("Roer"), &lv_font_montserrat_28) : NavCard{};
  navAttitude = n2k.attitudeMs ? makeNavCard(tile, T("Helling"), &lv_font_montserrat_28) : NavCard{};
}

void updateNavPage() {
  char a[48], b[96];
  // Diepte
  markLevel(navDepth.card, alarms_for_device("n2k:diepte"));
  if (n2kFresh(n2k.depthMs)) {
    lv_label_set_text_fmt(navDepth.big, "%.1f m", n2k.depth);
    const char *ref = isnan(n2k.depthOffset) || n2k.depthOffset == 0 ? T("onder de transducer")
                      : n2k.depthOffset > 0                          ? T("vanaf de waterlijn")
                                                                     : T("onder de kiel");
    lv_label_set_text(navDepth.sub, ref);
  } else {
    lv_label_set_text(navDepth.big, "--");
    lv_label_set_text(navDepth.sub, "");
  }
  // Snelheid
  if (n2kFresh(n2k.cogSogMs) && !isnan(n2k.sog)) lv_label_set_text_fmt(navSpeed.big, "%.1f kn", knots(n2k.sog));
  else lv_label_set_text(navSpeed.big, "--");
  if (n2kFresh(n2k.stwMs)) lv_label_set_text_fmt(navSpeed.sub, T("Door het water %.1f kn"), knots(n2k.stw));
  else lv_label_set_text(navSpeed.sub, "");
  // Koers
  bool moving = n2kFresh(n2k.cogSogMs) && !isnan(n2k.cog) && n2k.sog > 0.25f;  // onder ± 0,5 kn is COG onzin
  if (moving) lv_label_set_text_fmt(navCourse.big, "%03.0f°", n2k.cog);
  else lv_label_set_text(navCourse.big, "--");
  if (n2kFresh(n2k.headingMs)) lv_label_set_text_fmt(navCourse.sub, T("Kompas %03.0f° %s"), n2k.heading, n2k.headingRef == 1 ? T("magn.") : T("waar"));
  else lv_label_set_text(navCourse.sub, "");
  // Wind
  if (n2kFresh(n2k.appWindMs) && !isnan(n2k.aws)) {
    formatWindAngle(a, sizeof(a), n2k.awa);
    lv_label_set_text_fmt(navWind.big, "%.0f kn", knots(n2k.aws));
    snprintf(b, sizeof(b), T("Schijnbaar %s"), a);
  } else {
    lv_label_set_text(navWind.big, "--");
    b[0] = 0;
  }
  if (n2kFresh(n2k.trueWindMs) && !isnan(n2k.tws)) {
    size_t n = strlen(b);
    if (n2k.trueRef >= 3) {
      formatWindAngle(a, sizeof(a), n2k.twa);
      snprintf(b + n, sizeof(b) - n, T("%sWaar %.0f kn %s"), n ? "\n" : "", knots(n2k.tws), a);
    } else {
      char dir[8] = "--";
      if (!isnan(n2k.twa)) snprintf(dir, sizeof(dir), "%03.0f", n2k.twa);
      snprintf(b + n, sizeof(b) - n, T("%sWaar %.0f kn uit %s°"), n ? "\n" : "", knots(n2k.tws), dir);
    }
  }
  lv_label_set_text(navWind.sub, b);
  // Positie
  if (n2kFresh(n2k.posMs) && !isnan(n2k.lat)) {
    formatLatLon(a, sizeof(a), n2k.lat, true);
    formatLatLon(b, sizeof(b), n2k.lon, false);
    lv_label_set_text_fmt(navPos.big, "%s\n%s", a, b);
  } else {
    lv_label_set_text(navPos.big, "--");
  }
  // Temperaturen: zeewater groot, de rest eronder
  const N2kTemp *sea = nullptr;
  for (auto &t : n2k.temps)
    if (n2kFresh(t.ms, 30000) && t.source == 0 && !sea) sea = &t;
  size_t n = 0;
  b[0] = 0;
  int shown = 0;
  for (auto &t : n2k.temps) {
    if (!n2kFresh(t.ms, 30000) || &t == sea || shown >= 2) continue;
    n = appendf(b, sizeof(b), n, "%s%s %.1f °C", shown ? "\n" : "", tempSourceName(t.source, t.instance), t.c);
    shown++;
  }
  // Luchtvochtigheid (130313) erbij, als er nog ruimte is
  for (auto &h : n2k.humids) {
    if (!n2kFresh(h.ms, 30000) || shown >= 2) continue;
    n = appendf(b, sizeof(b), n, "%s%s %.0f %%", shown ? "\n" : "", h.source == 1 ? T("Vocht buiten") : T("Vocht binnen"), h.pct);
    shown++;
  }
  if (sea) lv_label_set_text_fmt(navTemp.big, "%.1f °C", sea->c);
  else lv_label_set_text(navTemp.big, shown ? "" : "--");
  lv_label_set_text(navTemp.sub, sea ? (shown ? b : T("Zeewater")) : b);
  // Roer: + = stuurboord
  if (navRudder.card) {
    if (n2kFresh(n2k.rudderMs) && !isnan(n2k.rudder)) {
      float r = n2k.rudder;
      if (fabsf(r) < 0.5f) lv_label_set_text(navRudder.big, "0°");
      else lv_label_set_text_fmt(navRudder.big, r > 0 ? T("%.0f° SB") : T("%.0f° BB"), fabsf(r));
      lv_label_set_text(navRudder.sub, T("Roerstand"));
    } else {
      lv_label_set_text(navRudder.big, "--");
      lv_label_set_text(navRudder.sub, "");
    }
  }
  // Helling (rollen) groot, stampen eronder
  if (navAttitude.card) {
    if (n2kFresh(n2k.attitudeMs) && !isnan(n2k.roll)) {
      float r = n2k.roll;
      if (fabsf(r) < 0.5f) lv_label_set_text(navAttitude.big, "0°");
      else lv_label_set_text_fmt(navAttitude.big, r > 0 ? T("%.0f° SB") : T("%.0f° BB"), fabsf(r));
      if (!isnan(n2k.pitch)) lv_label_set_text_fmt(navAttitude.sub, T("Stampen %+.0f°"), fabsf(n2k.pitch) < 0.5f ? 0.0f : n2k.pitch);
      else lv_label_set_text(navAttitude.sub, "");
    } else {
      lv_label_set_text(navAttitude.big, "--");
      lv_label_set_text(navAttitude.sub, "");
    }
  }
}

// --- Motor ---

struct EngineCard {
  int slot;
  lv_obj_t *card, *arc, *rpm, *vals[6];
  float peak;
};
EngineCard engineCards[N2K_ENGINES];
int engineCardCount;
const char *ENGINE_CAPTIONS[6] = {TN("Koelwater"), TN("Oliedruk"), TN("Dynamo"), TN("Verbruik"), TN("Draaiuren"), TN("Belasting")};

void buildEnginePage(lv_obj_t *tile) {
  makePageContainer(tile, LV_FLEX_FLOW_COLUMN);
  int count = engineCount();
  engineCardCount = 0;
  int h = count > 1 ? 186 : 380;
  for (int i = 0; i < N2K_ENGINES; i++) {
    const N2kEngine &e = n2k.engine[i];
    if (!e.rapidMs && !e.dynMs) continue;
    EngineCard &c = engineCards[engineCardCount++];
    c.slot = i;
    c.peak = 3000;
    lv_obj_t *card = makeCard(tile, 464, h, -1);
    c.card = card;

    char name[32];
    n2kEngineLabel(n2k, e, name, sizeof(name));
    lv_obj_t *t = makeLabel(card, &lv_font_montserrat_16, COL_ACCENT, name);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

    int d = count > 1 ? 140 : 190;
    c.arc = lv_arc_create(card);
    lv_obj_set_size(c.arc, d, d);
    lv_obj_align(c.arc, count > 1 ? LV_ALIGN_LEFT_MID : LV_ALIGN_TOP_MID, 0, count > 1 ? 10 : 24);
    lv_arc_set_rotation(c.arc, 135);
    lv_arc_set_bg_angles(c.arc, 0, 270);
    lv_arc_set_range(c.arc, 0, 1000);
    lv_obj_remove_style(c.arc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(c.arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(c.arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(c.arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(c.arc, COL_LINE, LV_PART_MAIN);
    lv_obj_set_style_arc_color(c.arc, COL_ACCENT, LV_PART_INDICATOR);
    c.rpm = makeLabel(card, &lv_font_montserrat_28, COL_TEXT, "--");
    lv_obj_set_style_text_align(c.rpm, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(c.rpm, c.arc, LV_ALIGN_CENTER, 0, -6);
    lv_obj_t *unit = makeLabel(card, &lv_font_montserrat_14, COL_MUTED, T("toeren/min"));
    lv_obj_align_to(unit, c.arc, LV_ALIGN_CENTER, 0, 22);

    // Zes waarden in een raster rechts (twee motoren) of onder de meter (één motor)
    for (int k = 0; k < 6; k++) {
      int col = k % 3, row = k / 3;
      int x = count > 1 ? 160 + col * 98 : 20 + col * 146;
      int y = count > 1 ? 34 + row * 70 : 236 + row * 62;
      lv_obj_set_pos(makeLabel(card, &lv_font_montserrat_14, COL_MUTED, T(ENGINE_CAPTIONS[k])), x, y);
      c.vals[k] = makeLabel(card, &lv_font_montserrat_20, COL_TEXT, "--");
      lv_obj_set_pos(c.vals[k], x, y + 18);
    }
  }
}

void updateEnginePage() {
  for (int i = 0; i < engineCardCount; i++) {
    EngineCard &c = engineCards[i];
    const N2kEngine &e = n2k.engine[c.slot];
    char key[18];
    snprintf(key, sizeof(key), "n2k:motor:%u", e.instance);
    markLevel(c.card, alarms_for_device(key));
    bool rapid = n2kFresh(e.rapidMs), dyn = n2kFresh(e.dynMs);
    if (rapid && !isnan(e.rpm)) {
      c.peak = fmaxf(c.peak, e.rpm);
      lv_label_set_text_fmt(c.rpm, "%.0f", e.rpm);
      lv_arc_set_value(c.arc, (int)(e.rpm * 1000 / c.peak));
    } else {
      lv_label_set_text(c.rpm, rapid || dyn ? "--" : T("uit"));
      lv_arc_set_value(c.arc, 0);
    }
    char buf[6][16];
    auto fmt = [&](int k, float v, const char *f) {
      if (dyn && !isnan(v)) snprintf(buf[k], 16, f, v);
      else snprintf(buf[k], 16, "--");
      lv_label_set_text(c.vals[k], buf[k]);
    };
    fmt(0, e.coolantC, "%.0f °C");
    fmt(1, e.oilBar, "%.1f bar");
    fmt(2, e.altV, "%.1f V");
    fmt(3, isnan(e.fuelLph) ? NAN : fmaxf(e.fuelLph, 0), T("%.1f L/u"));  // sommige motoren sturen -0,1
    fmt(4, e.hours, T("%.0f u"));
    if (dyn && e.loadPct >= 0) lv_label_set_text_fmt(c.vals[5], "%d%%", e.loadPct);
    else lv_label_set_text(c.vals[5], "--");
    // Koelwater: oranje boven 85 °C, rood boven 95 °C
    lv_color_t tc = !dyn || isnan(e.coolantC) ? COL_TEXT : e.coolantC > 95 ? COL_ERR : e.coolantC > 85 ? COL_WARN : COL_TEXT;
    lv_obj_set_style_text_color(c.vals[0], tc, 0);
  }
}

// --- Tanks ---

struct TankCol {
  int slot;
  lv_obj_t *bar, *pct, *liters;
};
TankCol tankCols[N2K_TANKS];
int tankColCount;

void buildTankPage(lv_obj_t *tile) {
  lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  int count = tankCount();
  int perRow = min(count, 6), rows = (count + 5) / 6;
  int colW = 76, barH = rows > 1 ? 96 : 228;
  int x0 = (W - perRow * colW) / 2;
  tankColCount = 0;
  for (int i = 0; i < N2K_TANKS; i++) {
    const N2kTank &t = n2k.tanks[i];
    if (!t.ms) continue;
    int idx = tankColCount, row = idx / 6, col = idx % 6;
    TankCol &c = tankCols[tankColCount++];
    c.slot = i;
    int x = x0 + col * colW, y = 8 + row * (barH + 92);

    char name[24];
    n2kTankLabel(n2k, t, name, sizeof(name));
    lv_obj_t *n = makeLabel(tile, &lv_font_montserrat_14, COL_TEXT, name);
    lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
    lv_obj_set_size(n, colW - 4, 18);
    lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(n, x + 2, y);

    c.bar = lv_bar_create(tile);
    lv_obj_set_size(c.bar, 40, barH);
    lv_obj_set_pos(c.bar, x + (colW - 40) / 2, y + 22);
    lv_bar_set_range(c.bar, 0, 1000);
    lv_obj_set_style_radius(c.bar, 8, 0);
    lv_obj_set_style_radius(c.bar, 8, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(c.bar, COL_CARD, 0);
    lv_obj_set_style_bg_opa(c.bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(c.bar, tankColor(t.type), LV_PART_INDICATOR);

    c.pct = makeLabel(tile, &lv_font_montserrat_20, COL_TEXT, "--");
    lv_obj_set_width(c.pct, colW);
    lv_obj_set_style_text_align(c.pct, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(c.pct, x, y + 28 + barH);
    c.liters = makeLabel(tile, &lv_font_montserrat_14, COL_MUTED, "");
    lv_obj_set_width(c.liters, colW);
    lv_obj_set_style_text_align(c.liters, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(c.liters, x, y + 54 + barH);
  }
}

void updateTankPage() {
  for (int i = 0; i < tankColCount; i++) {
    TankCol &c = tankCols[i];
    const N2kTank &t = n2k.tanks[c.slot];
    if (!n2kFresh(t.ms, N2K_TANK_FRESH_MS)) {
      lv_label_set_text(c.pct, "--");
      lv_label_set_text(c.liters, "");
      continue;
    }
    float pct = fmaxf(0, fminf(100, t.levelPct));
    lv_bar_set_value(c.bar, (int)(pct * 10), LV_ANIM_ON);
    lv_label_set_text_fmt(c.pct, "%.0f%%", pct);
    if (!isnan(t.capacityL) && t.capacityL > 0) lv_label_set_text_fmt(c.liters, "%.0f L", t.capacityL * pct / 100);
    else lv_label_set_text(c.liters, "");
    // Kleur volgens de NMEA 2000-alarmen (oranje/rood); zonder alarmen: rood bij bijna leeg of bijna vol
    char key[18];
    snprintf(key, sizeof(key), "n2k:tank:%u:%u", t.type, t.instance);
    AlarmLevel l = alarms_for_device(key);
    bool waste = t.type == 2 || t.type == 5;
    bool critical = waste ? pct > 85 : pct < 15;
    lv_color_t col = l == AlarmLevel::Alarm ? COL_ERR : l == AlarmLevel::Warning ? COL_WARN
                     : !settings().n2kAlarms && critical ? COL_ERR : COL_TEXT;
    lv_obj_set_style_text_color(c.pct, col, 0);
  }
}

// ---------- Historie ----------

enum class HistView : uint8_t { Battery, Energy, SolarDays, Temp };

const char *histViewName(HistView v) {
  switch (v) {
    case HistView::Battery: return T("Accu");
    case HistView::Energy: return T("Energie");
    case HistView::SolarDays: return T("Zon/dag");
    default: return T("Temp");
  }
}

lv_color_t histTempColor(int i) { return lv_color_hex(theme().hist[i % 6]); }

HistView histViews[4];
int histViewCount;
HistView histView = HistView::Battery;  // blijft bewaard
bool histLong = false;
lv_obj_t *histSel, *histRange, *histChart, *histSummary, *histLegend, *histEmpty;
lv_coord_t (*histPts)[HIST_SHORT_N];  // grafiekpunten, in PSRAM
uint32_t histShownVersion;
int histBarCount;
uint32_t histDayEnds[8];

// Ruwe historiewaarden naar chartpunten; geeft min/max (in ruwe eenheden) terug
int loadSeries(HistSeries s, lv_coord_t *pts, int32_t &lo, int32_t &hi) {
  static int16_t raw[HIST_SHORT_N];
  int n = history_get(s, histLong, raw);
  int valid = 0;
  for (int i = 0; i < n; i++) {
    if (raw[i] == HIST_NONE) {
      pts[i] = LV_CHART_POINT_NONE;
      continue;
    }
    pts[i] = raw[i];
    lo = min<int32_t>(lo, raw[i]);
    hi = max<int32_t>(hi, raw[i]);
    valid++;
  }
  return valid;
}

// Ronde asgrenzen: 4 vakken van een rond getal (zo vallen de rasterlijnen op ronde waarden)
void niceRange(int32_t &lo, int32_t &hi, int32_t minSpan) {
  if (hi - lo < minSpan) hi = lo + minSpan;
  for (int32_t step : {1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000, 10000}) {
    int32_t base = (int32_t)floorf((float)lo / step) * step;
    if (base + 4 * step >= hi) {
      lo = base;
      hi = base + 4 * step;
      return;
    }
  }
}

void histTickLabel(lv_event_t *e) {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
  if (!lv_obj_draw_part_check_type(dsc, &lv_chart_class, LV_CHART_DRAW_PART_TICK_LABEL) || !dsc->text) return;
  if (dsc->id == LV_CHART_AXIS_PRIMARY_X) {
    // Met klok: echte tijden en weekdagen; zonder klok: terugtellen vanaf nu
    int i = dsc->value;
    uint32_t now = clock_now();
    if (histView == HistView::SolarDays) {
      if (i < 0 || i >= histBarCount) {  // bv. één staaf maar twee assenlabels
        dsc->text[0] = 0;
        return;
      }
      int back = histBarCount - 1 - i;
      uint32_t end = histDayEnds[i];
      if (back == 0) lv_snprintf(dsc->text, dsc->text_length, now ? T("vand.") : T("nu"));
      else if (end) lv_snprintf(dsc->text, dsc->text_length, "%s", weekdayOf(end - 12 * 3600));  // dag vóór de wissel
      else lv_snprintf(dsc->text, dsc->text_length, T("-%dd"), back);
    } else if (histLong) {
      if (i == 7) lv_snprintf(dsc->text, dsc->text_length, T("nu"));
      else if (now) lv_snprintf(dsc->text, dsc->text_length, "%s", weekdayOf(now - (7 - i) * 86400UL));
      else lv_snprintf(dsc->text, dsc->text_length, T("-%dd"), 7 - i);
    } else {
      if (i == 4) lv_snprintf(dsc->text, dsc->text_length, T("nu"));
      else if (now) formatHM(dsc->text, dsc->text_length, now - (24 - i * 6) * 3600UL);
      else lv_snprintf(dsc->text, dsc->text_length, T("-%du"), 24 - i * 6);
    }
  } else {
    int32_t v = dsc->value;
    switch (histView) {
      case HistView::Battery: lv_snprintf(dsc->text, dsc->text_length, "%d%%", (int)v); break;
      case HistView::Temp: lv_snprintf(dsc->text, dsc->text_length, "%d°", (int)lroundf(v / 10.0f)); break;
      case HistView::SolarDays:
        if (v % 1000 == 0) lv_snprintf(dsc->text, dsc->text_length, "%d", (int)(v / 1000));
        else lv_snprintf(dsc->text, dsc->text_length, "%.1f", v / 1000.0f);
        break;
      default: lv_snprintf(dsc->text, dsc->text_length, "%d", (int)v); break;
    }
  }
}

lv_chart_series_t *addSeries(lv_color_t c) { return lv_chart_add_series(histChart, c, LV_CHART_AXIS_PRIMARY_Y); }

void refreshHistory() {
  histShownVersion = history_version();
  while (lv_chart_get_series_next(histChart, nullptr)) lv_chart_remove_series(histChart, lv_chart_get_series_next(histChart, nullptr));

  char legend[256] = "", summary[96] = "";
  size_t ln = 0;
  int32_t lo = INT32_MAX, hi = INT32_MIN;
  int valid = 0;
  bool bars = histView == HistView::SolarDays;
  lv_chart_set_type(histChart, bars ? LV_CHART_TYPE_BAR : LV_CHART_TYPE_LINE);
  if (bars) lv_obj_add_flag(histRange, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(histRange, LV_OBJ_FLAG_HIDDEN);

  int n = histLong ? HIST_LONG_N : HIST_SHORT_N;
  auto line = [&](HistSeries s, lv_color_t c, int slot, const char *name) {
    valid += loadSeries(s, histPts[slot], lo, hi);
    lv_chart_series_t *ser = addSeries(c);
    lv_chart_set_ext_y_array(histChart, ser, histPts[slot]);
    if (name) {
      ln = appendf(legend, sizeof(legend), ln, "%s#%02x%02x%02x " LV_SYMBOL_MINUS "# %s", ln ? "   " : "", c.ch.red << 3,
                     c.ch.green << 2, c.ch.blue << 3, name);
    }
  };

  switch (histView) {
    case HistView::Battery: {
      lv_chart_set_point_count(histChart, n);
      line(H_SOC, COL_OK, 0, nullptr);
      if (valid) {
        // Laatste geldige waarde = nu
        int last = n - 1;
        while (last > 0 && histPts[0][last] == LV_CHART_POINT_NONE) last--;
        snprintf(summary, sizeof(summary), T("SoC min %d%%  max %d%%  nu %d%%"), (int)lo, (int)hi, (int)histPts[0][last]);
      }
      lo = 0;
      hi = 100;
      niceRange(lo, hi, 100);
      break;
    }
    case HistView::Energy: {
      lv_chart_set_point_count(histChart, n);
      line(H_PV_W, COL_SUN, 0, T("Zon"));
      line(H_LOAD_W, COL_LOAD, 1, T("Verbruik"));
      line(H_BATT_W, COL_BATTN, 2, T("Accu (+laden)"));
      lo = min<int32_t>(lo, 0);
      if (valid) niceRange(lo, hi, 50);
      snprintf(summary, sizeof(summary), T("Vermogen in W"));
      break;
    }
    case HistView::SolarDays: {
      float days[HIST_DAYS_N + 1];
      uint32_t ends[HIST_DAYS_N + 1];
      int nd = history_days(days, ends);
      int show = min(nd, 7);
      histBarCount = show;
      lv_chart_set_point_count(histChart, show);
      lv_chart_series_t *ser = addSeries(COL_SUN);
      float total = 0;
      lo = 0;
      hi = 0;
      for (int i = 0; i < show; i++) {
        float kwh = days[nd - show + i];
        histDayEnds[i] = ends[nd - show + i];
        histPts[0][i] = (lv_coord_t)lroundf(kwh * 1000);  // in Wh
        hi = max<int32_t>(hi, histPts[0][i]);
        total += kwh;
      }
      lv_chart_set_ext_y_array(histChart, ser, histPts[0]);
      valid = show;
      niceRange(lo, hi, 500);
      if (show > 0)
        snprintf(summary, sizeof(summary), T("kWh  " LV_SYMBOL_BULLET "  vandaag %.2f  " LV_SYMBOL_BULLET "  gem. %.2f"),
                 days[nd - 1], total / show);
      else
        snprintf(summary, sizeof(summary), T("Nog geen dagopbrengst"));
      break;
    }
    case HistView::Temp: {
      lv_chart_set_point_count(histChart, n);
      for (int t = 0; t < HIST_TEMPS; t++)
        if (history_temp_name(t)[0]) line((HistSeries)(H_TEMP0 + t), histTempColor(t), t, history_temp_name(t));
      if (valid) {
        lo -= 10;
        hi += 10;
        niceRange(lo, hi, 50);
      }
      snprintf(summary, sizeof(summary), T("Temperatuur in °C"));
      break;
    }
  }

  int majors = bars ? histBarCount : histLong ? 8 : 5;
  lv_chart_set_axis_tick(histChart, LV_CHART_AXIS_PRIMARY_X, 4, 0, max(majors, 2), 1, true, 30);
  if (!valid) {
    lo = 0;
    hi = 100;
  }
  lv_chart_set_range(histChart, LV_CHART_AXIS_PRIMARY_Y, lo, hi);
  lv_label_set_text(histSummary, summary);
  lv_label_set_text(histLegend, legend);
  if (valid) lv_obj_add_flag(histEmpty, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(histEmpty, LV_OBJ_FLAG_HIDDEN);
  lv_chart_refresh(histChart);
}

lv_obj_t *makeSegmented(lv_obj_t *parent, const char **map, int checked, lv_event_cb_t cb) {
  lv_obj_t *bm = lv_btnmatrix_create(parent);
  lv_btnmatrix_set_map(bm, map);
  lv_btnmatrix_set_btn_ctrl_all(bm, LV_BTNMATRIX_CTRL_CHECKABLE);
  lv_btnmatrix_set_one_checked(bm, true);
  lv_btnmatrix_set_btn_ctrl(bm, checked, LV_BTNMATRIX_CTRL_CHECKED);
  lv_obj_set_style_bg_opa(bm, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bm, 0, 0);
  lv_obj_set_style_pad_all(bm, 0, 0);
  lv_obj_set_style_pad_column(bm, 6, 0);
  lv_obj_set_style_bg_color(bm, COL_CARD, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(bm, COL_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(bm, COL_TEXT, LV_PART_ITEMS);
  lv_obj_set_style_text_color(bm, COL_INK, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_font(bm, &lv_font_montserrat_14, LV_PART_ITEMS);
  lv_obj_set_style_radius(bm, 16, LV_PART_ITEMS);
  lv_obj_add_event_cb(bm, cb, LV_EVENT_VALUE_CHANGED, nullptr);
  return bm;
}

void buildHistoryPage(lv_obj_t *tile) {
  lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  if (!histPts) histPts = (lv_coord_t(*)[HIST_SHORT_N])ps_malloc(HIST_TEMPS * HIST_SHORT_N * sizeof(lv_coord_t));
  bool battery = false, solar = false, climate = false;
  for (int i = 0; i < snapCount; i++) {
    Category c = categoryOf(snap[i].cfg.type);
    battery |= c == Category::Battery;
    solar |= c == Category::Solar;
    climate |= c == Category::Climate;
  }
  histViewCount = 0;
  if (battery) histViews[histViewCount++] = HistView::Battery;
  if (battery || solar) histViews[histViewCount++] = HistView::Energy;
  if (solar) histViews[histViewCount++] = HistView::SolarDays;
  if (climate) histViews[histViewCount++] = HistView::Temp;
  int sel = 0;
  for (int i = 0; i < histViewCount; i++)
    if (histViews[i] == histView) sel = i;
  histView = histViews[sel];

  static const char *selMap[5];
  for (int i = 0; i < histViewCount; i++) selMap[i] = histViewName(histViews[i]);
  selMap[histViewCount] = "";
  histSel = makeSegmented(tile, selMap, sel, [](lv_event_t *e) {
    histView = histViews[lv_btnmatrix_get_selected_btn(lv_event_get_target(e))];
    refreshHistory();
  });
  lv_obj_set_size(histSel, 464, 36);
  lv_obj_set_pos(histSel, 8, 4);

  static const char *rangeMap[3];
  rangeMap[0] = T("24 uur");
  rangeMap[1] = T("7 dagen");
  rangeMap[2] = "";
  histRange = makeSegmented(tile, rangeMap, histLong ? 1 : 0, [](lv_event_t *e) {
    histLong = lv_btnmatrix_get_selected_btn(lv_event_get_target(e)) == 1;
    refreshHistory();
  });
  lv_obj_set_size(histRange, 180, 32);
  lv_obj_set_pos(histRange, 292, 48);

  histSummary = makeLabel(tile, &lv_font_montserrat_14, COL_MUTED, "");
  lv_label_set_long_mode(histSummary, LV_LABEL_LONG_DOT);
  lv_obj_set_size(histSummary, 276, 18);
  lv_obj_set_pos(histSummary, 12, 56);

  histChart = lv_chart_create(tile);
  lv_obj_set_size(histChart, 412, 236);
  lv_obj_set_pos(histChart, 56, 92);
  lv_obj_clear_flag(histChart, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(histChart, COL_CARD, 0);
  lv_obj_set_style_border_width(histChart, 0, 0);
  lv_obj_set_style_radius(histChart, 8, 0);
  lv_obj_set_style_line_color(histChart, COL_LINE, LV_PART_MAIN);  // rasterlijnen
  lv_obj_set_style_line_width(histChart, 2, LV_PART_ITEMS);
  lv_obj_set_style_size(histChart, 0, LV_PART_INDICATOR);  // geen bolletjes op de lijn
  lv_obj_set_style_pad_column(histChart, 8, 0);             // ruimte tussen staven
  lv_obj_set_style_text_color(histChart, COL_MUTED, LV_PART_TICKS);
  lv_obj_set_style_text_font(histChart, &lv_font_montserrat_14, LV_PART_TICKS);
  lv_obj_set_style_line_color(histChart, COL_LINE, LV_PART_TICKS);
  lv_chart_set_div_line_count(histChart, 5, 0);
  lv_chart_set_update_mode(histChart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_axis_tick(histChart, LV_CHART_AXIS_PRIMARY_Y, 4, 0, 5, 1, true, 48);
  lv_obj_add_event_cb(histChart, histTickLabel, LV_EVENT_DRAW_PART_BEGIN, nullptr);

  histEmpty = makeLabel(histChart, &lv_font_montserrat_14, COL_MUTED, T("Nog geen gegevens.\nDe eerste meting volgt binnen 5 minuten."));
  lv_obj_set_style_text_align(histEmpty, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(histEmpty);

  histLegend = makeLabel(tile, &lv_font_montserrat_14, COL_TEXT, "");
  lv_label_set_recolor(histLegend, true);
  lv_label_set_long_mode(histLegend, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(histLegend, 456);
  lv_obj_set_pos(histLegend, 12, 356);

  refreshHistory();
}

void updateHistoryPage() {
  if (history_version() != histShownVersion) refreshHistory();
}

// ---------- Barometer: luchtdruk, trend over 3 uur en 48 uur verloop ----------

lv_obj_t *baroCard, *baroValue, *baroArrow, *baroTrend, *baroSource, *baroChart, *baroEmpty;
lv_chart_series_t *baroSer;
lv_coord_t *baroPts;
uint32_t baroShownVersion = UINT32_MAX;

void baroTickLabel(lv_event_t *e) {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
  if (!lv_obj_draw_part_check_type(dsc, &lv_chart_class, LV_CHART_DRAW_PART_TICK_LABEL) || !dsc->text) return;
  if (dsc->id == LV_CHART_AXIS_PRIMARY_X) {
    int i = dsc->value;  // 0..4 = 48, 36, 24, 12 uur terug, nu
    uint32_t now = clock_now();
    if (i == 4) lv_snprintf(dsc->text, dsc->text_length, T("nu"));
    else if (now) formatHM(dsc->text, dsc->text_length, now - (48 - i * 12) * 3600UL);
    else lv_snprintf(dsc->text, dsc->text_length, T("-%du"), 48 - i * 12);
  } else {
    lv_snprintf(dsc->text, dsc->text_length, "%d", (int)lroundf(dsc->value / 10.0f));  // hPa (opgeslagen x10)
  }
}

void refreshBaroChart() {
  baroShownVersion = baro_version();
  static int16_t raw[BARO_N];
  int n = baro_series(raw, BARO_N);
  int32_t lo = INT32_MAX, hi = INT32_MIN;
  int valid = 0;
  for (int i = 0; i < n; i++) {
    if (raw[i] == BARO_NONE) {
      baroPts[i] = LV_CHART_POINT_NONE;
      continue;
    }
    baroPts[i] = raw[i];
    lo = min<int32_t>(lo, raw[i]);
    hi = max<int32_t>(hi, raw[i]);
    valid++;
  }
  if (valid) {
    // Bereik van 20 (of 40, 60 ...) hPa rond het midden, ondergrens op hele 5 hPa: 4 vakken van 5 hPa of meer.
    // Zo ziet een kleine schommeling er niet uit als een storm.
    int32_t span = (hi - lo + 50 + 199) / 200 * 200, mid = (lo + hi) / 2;
    int32_t base = (mid - span / 2) / 50 * 50;
    if (base > lo) base -= 50;
    if (base + span < hi) span += 200;
    lo = base;
    hi = base + span;
    lv_chart_set_range(baroChart, LV_CHART_AXIS_PRIMARY_Y, lo, hi);
    lv_obj_add_flag(baroEmpty, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_chart_set_range(baroChart, LV_CHART_AXIS_PRIMARY_Y, 10000, 10200);
    lv_obj_clear_flag(baroEmpty, LV_OBJ_FLAG_HIDDEN);
  }
  lv_chart_refresh(baroChart);
}

void updateBaroPage() {
  BaroInfo b = baro_info();
  if (isnan(b.hPa)) lv_label_set_text(baroValue, "-- hPa");
  else lv_label_set_text_fmt(baroValue, "%.1f hPa", b.hPa);
  bool falling = !isnan(b.diff3h) && b.diff3h < 0;
  bool steady = b.tendency == BaroTendency::Steady;
  lv_label_set_text(baroArrow, isnan(b.diff3h) ? "" : steady ? LV_SYMBOL_RIGHT : falling ? LV_SYMBOL_DOWN : LV_SYMBOL_UP);
  char t[96];
  if (isnan(b.diff3h)) snprintf(t, sizeof(t), "%s", T(baro_tendency_text(b.tendency)));
  else snprintf(t, sizeof(t), T("%s\n%+.1f hPa in 3 uur"), T(baro_tendency_text(b.tendency)), b.diff3h);
  lv_label_set_text(baroTrend, t);
  // Kleur: hard dalen = oranje of rood (zoals de stormwaarschuwing), anders gewoon
  AlarmLevel lvl = alarms_for_device("baro");
  lv_color_t c = lvl == AlarmLevel::Alarm ? COL_ERR : lvl == AlarmLevel::Warning ? COL_WARN : COL_TEXT;
  lv_obj_set_style_text_color(baroArrow, c, 0);
  lv_obj_set_style_text_color(baroTrend, c, 0);
  lv_obj_set_style_border_color(baroCard, c, 0);
  lv_obj_set_style_border_width(baroCard, lvl == AlarmLevel::None ? 0 : 2, 0);
  if (b.source == BaroSource::None) snprintf(t, sizeof(t), "%s", T("Geen meting op dit moment"));
  else snprintf(t, sizeof(t), T("Bron: %s"), b.sourceName);
  lv_label_set_text(baroSource, t);
  if (baro_version() != baroShownVersion) refreshBaroChart();
}

void buildBaroPage(lv_obj_t *tile) {
  lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  if (!baroPts) baroPts = (lv_coord_t *)ps_malloc(BARO_N * sizeof(lv_coord_t));
  baroCard = makeCard(tile, W - 24, 104, -1);
  lv_obj_set_pos(baroCard, 12, 8);
  baroValue = makeLabel(baroCard, &lv_font_montserrat_28, COL_TEXT, "-- hPa");
  lv_obj_align(baroValue, LV_ALIGN_TOP_LEFT, 0, 4);
  baroSource = makeLabel(baroCard, &lv_font_montserrat_14, COL_MUTED, "");
  lv_label_set_long_mode(baroSource, LV_LABEL_LONG_DOT);
  lv_obj_set_width(baroSource, 220);
  lv_obj_align(baroSource, LV_ALIGN_BOTTOM_LEFT, 0, -2);
  baroArrow = makeLabel(baroCard, &lv_font_montserrat_28, COL_TEXT, "");
  lv_obj_align(baroArrow, LV_ALIGN_LEFT_MID, 230, 0);
  baroTrend = makeLabel(baroCard, &lv_font_montserrat_16, COL_TEXT, "");
  lv_label_set_long_mode(baroTrend, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(baroTrend, 170);
  lv_obj_align(baroTrend, LV_ALIGN_LEFT_MID, 266, 0);

  baroChart = lv_chart_create(tile);
  lv_obj_set_size(baroChart, 412, 214);
  lv_obj_set_pos(baroChart, 56, 128);
  lv_obj_clear_flag(baroChart, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(baroChart, COL_CARD, 0);
  lv_obj_set_style_border_width(baroChart, 0, 0);
  lv_obj_set_style_radius(baroChart, 8, 0);
  lv_obj_set_style_line_color(baroChart, COL_LINE, LV_PART_MAIN);
  lv_obj_set_style_line_width(baroChart, 2, LV_PART_ITEMS);
  lv_obj_set_style_size(baroChart, 0, LV_PART_INDICATOR);
  lv_obj_set_style_text_color(baroChart, COL_MUTED, LV_PART_TICKS);
  lv_obj_set_style_text_font(baroChart, &lv_font_montserrat_14, LV_PART_TICKS);
  lv_obj_set_style_line_color(baroChart, COL_LINE, LV_PART_TICKS);
  lv_chart_set_div_line_count(baroChart, 5, 3);
  lv_chart_set_point_count(baroChart, BARO_N);
  lv_chart_set_axis_tick(baroChart, LV_CHART_AXIS_PRIMARY_Y, 4, 0, 5, 1, true, 48);
  lv_chart_set_axis_tick(baroChart, LV_CHART_AXIS_PRIMARY_X, 4, 0, 5, 1, true, 30);
  lv_obj_add_event_cb(baroChart, baroTickLabel, LV_EVENT_DRAW_PART_BEGIN, nullptr);
  baroSer = lv_chart_add_series(baroChart, COL_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_ext_y_array(baroChart, baroSer, baroPts);
  baroEmpty = makeLabel(baroChart, &lv_font_montserrat_14, COL_MUTED, T("Nog geen verloop.\nElke 10 minuten komt er een punt bij."));
  lv_obj_set_style_text_align(baroEmpty, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(baroEmpty);
  baroShownVersion = UINT32_MAX;
  updateBaroPage();
}

// ---------- Opbouw en navigatie ----------

void updateDots() {
  for (int p = 0; p < pageCount; p++) {
    bool active = pageIds[p] == currentPage;
    lv_obj_set_style_bg_color(pageDots[p], active ? COL_ACCENT : COL_LINE, 0);
    lv_obj_set_width(pageDots[p], active ? 22 : 8);
  }
  lv_label_set_text(pageTitleLbl, pageTitle(currentPage));
}

void onPageChanged(lv_event_t *) {
  lv_obj_t *act = lv_tileview_get_tile_act(tileview);
  for (int p = 0; p < pageCount; p++)
    if (lv_obj_get_child(tileview, p) == act) currentPage = pageIds[p];
  updateDots();
}

void updateClock() {
  char buf[8] = "";
  if (clock_valid()) formatHM(buf, sizeof(buf), clock_now());
  lv_label_set_text(clockLbl, buf);
}

void updateDashboard() {
  n2k_get(n2k);
  if (store_version() != dashVersion || n2kLayoutKey() != dashN2kKey) {
    buildDashboard();
    return;
  }
  updateClock();
  loadSnapshot();
  for (int p = 0; p < pageCount; p++) {
    switch (pageIds[p]) {
      case PageId::Overview: updateOverview(); break;
      case PageId::Solar: updateSolarPage(); break;
      case PageId::Battery: updateBatteryPage(); break;
      case PageId::History: updateHistoryPage(); break;
      case PageId::Navigation: updateNavPage(); break;
      case PageId::Engine: updateEnginePage(); break;
      case PageId::Tanks: updateTankPage(); break;
      case PageId::Barometer: updateBaroPage(); break;
      default: break;
    }
  }
  updateTiles();
}

void buildDashboard() {
  scanFromSettings = false;
  dashVersion = store_version();
  loadSnapshot();

  // Welke pagina's zijn er? Alleen als er passende apparaten gekoppeld zijn.
  pageCount = 0;
  bool solar = hasCategory(Category::Solar), batt = hasCategory(Category::Battery);
  bool shore = false;
  for (int i = 0; i < snapCount; i++) shore |= snap[i].cfg.type == SensorType::VictronAcCharger || isVictronMulti(snap[i].cfg.type);
  n2k_get(n2k);
  dashN2kKey = n2kLayoutKey();
  if (solar || batt || shore) pageIds[pageCount++] = PageId::Overview;
  if (hasNavData()) pageIds[pageCount++] = PageId::Navigation;
  if (solar) pageIds[pageCount++] = PageId::Solar;
  if (batt || n2kBattCount()) pageIds[pageCount++] = PageId::Battery;
  if (engineCount()) pageIds[pageCount++] = PageId::Engine;
  if (tankCount()) pageIds[pageCount++] = PageId::Tanks;
  if (hasCategory(Category::Tank)) pageIds[pageCount++] = PageId::TankSensors;
  if (hasCategory(Category::Climate)) pageIds[pageCount++] = PageId::Climate;
  if (baro_available()) pageIds[pageCount++] = PageId::Barometer;
  if (hasCategory(Category::Charger)) pageIds[pageCount++] = PageId::Chargers;
  if (solar || batt || hasCategory(Category::Climate)) pageIds[pageCount++] = PageId::History;
  pageIds[pageCount++] = PageId::Devices;

  int startPage = pageCount - 1;
  for (int p = 0; p < pageCount; p++)
    if (pageIds[p] == currentPage) startPage = p;
  currentPage = pageIds[startPage];

  lv_obj_t *scr = makeScreen();
  pageTitleLbl = makeHeader(scr, "", nullptr);
  // Apparaat toevoegen staat in de instellingen (en op de lege apparatenpagina)
  lv_obj_t *gear = makeButton(headerOf(scr), LV_SYMBOL_SETTINGS, COL_CARD, [](lv_event_t *) { buildSettings(); });
  lv_obj_set_size(gear, 56, 40);
  lv_obj_align(gear, LV_ALIGN_RIGHT_MID, -8, 0);
  clockLbl = makeLabel(headerOf(scr), &lv_font_montserrat_20, COL_TEXT, "");
  lv_obj_align(clockLbl, LV_ALIGN_RIGHT_MID, -76, 0);
  lv_obj_set_width(pageTitleLbl, 220);

  tileview = lv_tileview_create(scr);
  lv_obj_set_size(tileview, W, PAGE_H);
  lv_obj_set_pos(tileview, 0, HEADER_H);
  lv_obj_set_style_bg_opa(tileview, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);

  tileCount = 0;
  solarCount = battCount = 0;
  for (int p = 0; p < pageCount; p++) {
    lv_dir_t dir = (p > 0 ? LV_DIR_LEFT : LV_DIR_NONE) | (p < pageCount - 1 ? LV_DIR_RIGHT : LV_DIR_NONE);
    lv_obj_t *tile = lv_tileview_add_tile(tileview, p, 0, dir);
    switch (pageIds[p]) {
      case PageId::Overview: buildOverview(tile); break;
      case PageId::Solar: buildSolarPage(tile); break;
      case PageId::Battery: buildBatteryPage(tile); break;
      case PageId::Climate: buildTilePage(tile, (int)Category::Climate); break;
      case PageId::Barometer: buildBaroPage(tile); break;
      case PageId::Chargers: buildTilePage(tile, (int)Category::Charger); break;
      case PageId::History: buildHistoryPage(tile); break;
      case PageId::Navigation: buildNavPage(tile); break;
      case PageId::Engine: buildEnginePage(tile); break;
      case PageId::Tanks: buildTankPage(tile); break;
      case PageId::TankSensors: buildTilePage(tile, (int)Category::Tank); break;
      default: buildTilePage(tile, -1); break;
    }
  }
  lv_obj_set_tile_id(tileview, startPage, 0, LV_ANIM_OFF);
  lv_obj_add_event_cb(tileview, onPageChanged, LV_EVENT_VALUE_CHANGED, nullptr);

  // Pagina-indicator onderin
  lv_obj_t *dotsRow = lv_obj_create(scr);
  lv_obj_set_size(dotsRow, W, DOTS_H);
  lv_obj_align(dotsRow, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(dotsRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(dotsRow, 0, 0);
  lv_obj_set_style_pad_all(dotsRow, 0, 0);
  lv_obj_set_style_pad_column(dotsRow, 8, 0);
  lv_obj_set_flex_flow(dotsRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(dotsRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(dotsRow, LV_OBJ_FLAG_SCROLLABLE);
  for (int p = 0; p < pageCount; p++) {
    pageDots[p] = lv_obj_create(dotsRow);
    lv_obj_set_size(pageDots[p], 8, 8);
    lv_obj_set_style_radius(pageDots[p], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(pageDots[p], 0, 0);
    lv_obj_clear_flag(pageDots[p], LV_OBJ_FLAG_CLICKABLE);
  }

  showScreen(scr, Page::Dashboard);
  updateDots();
  updateDashboard();
}

// ================= Scannen =================

struct ScanRow {
  char mac[18];
  lv_obj_t *btn, *title, *sub;
};
ScanRow rows[MAX_DISCOVERED];
int rowCount;
lv_obj_t *scanList, *scanHint;

void updateScan() {
  static Discovered found[MAX_DISCOVERED];
  int n = store_discovered(found, MAX_DISCOVERED);

  for (int r = 0; r < rowCount; r++) lv_obj_add_flag(rows[r].btn, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < n; i++) {
    const Discovered &d = found[i];
    int r = 0;
    while (r < rowCount && strcmp(rows[r].mac, d.mac) != 0) r++;
    if (r == rowCount) {
      if (rowCount >= MAX_DISCOVERED) continue;
      ScanRow &row = rows[rowCount++];
      strlcpy(row.mac, d.mac, sizeof(row.mac));
      row.btn = lv_btn_create(scanList);
      lv_obj_set_size(row.btn, lv_pct(100), 64);
      lv_obj_set_style_bg_color(row.btn, COL_CARD, 0);
      lv_obj_set_style_shadow_width(row.btn, 0, 0);
      lv_obj_add_event_cb(
          row.btn, [](lv_event_t *e) { buildEdit(true, -1, rows[(intptr_t)lv_event_get_user_data(e)].mac); },
          LV_EVENT_CLICKED, (void *)(intptr_t)r);
      row.title = makeLabel(row.btn, &lv_font_montserrat_16, COL_TEXT);
      lv_obj_align(row.title, LV_ALIGN_TOP_LEFT, 0, 0);
      row.sub = makeLabel(row.btn, &lv_font_montserrat_14, COL_MUTED);
      lv_obj_align(row.sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
      lv_obj_t *plus = makeLabel(row.btn, &lv_font_montserrat_20, COL_ACCENT, LV_SYMBOL_PLUS);
      lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    ScanRow &row = rows[r];
    lv_obj_clear_flag(row.btn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(row.title, d.name[0] ? d.name : T(typeName(d.type)));
    lv_label_set_text_fmt(row.sub, "%s  " LV_SYMBOL_BLUETOOTH " %d dBm  %s", T(typeName(d.type)), d.rssi, d.mac);
  }

  if (n == 0) lv_label_set_text(scanHint, T("Zoeken naar BLE-sensoren...\nZorg dat de sensor aan staat en dichtbij is."));
  else lv_label_set_text_fmt(scanHint, T("%d sensor(en) gevonden - tik om toe te voegen"), n);
}

void buildScan() {
  if (store_count() >= MAX_DEVICES) {
    // Eén melding tegelijk: nogmaals tikken stapelt geen nieuwe vensters op
    static lv_obj_t *fullBox = nullptr;
    if (fullBox) return;
    static const char *btns[] = {"OK", ""};
    fullBox = lv_msgbox_create(nullptr, T("Vol"), T("Maximaal aantal apparaten bereikt."), btns, false);
    lv_obj_add_event_cb(fullBox, [](lv_event_t *e) { lv_msgbox_close_async(lv_event_get_current_target(e)); },
                        LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(fullBox, [](lv_event_t *) { fullBox = nullptr; }, LV_EVENT_DELETE, nullptr);
    lv_obj_center(fullBox);
    return;
  }

  lv_obj_t *scr = makeScreen();
  // Terug naar waar je vandaan kwam: de instellingen of het dashboard
  makeHeader(scr, T("Apparaat toevoegen"), [](lv_event_t *) {
    if (scanFromSettings) buildSettings();
    else buildDashboard();
  });
  lv_obj_t *spin = lv_spinner_create(headerOf(scr), 1000, 60);
  lv_obj_set_size(spin, 32, 32);
  lv_obj_align(spin, LV_ALIGN_RIGHT_MID, -16, 0);

  scanHint = makeLabel(scr, &lv_font_montserrat_14, COL_MUTED);
  lv_obj_set_pos(scanHint, 16, HEADER_H + 10);

  scanList = lv_obj_create(scr);
  lv_obj_set_size(scanList, W, 480 - HEADER_H - 50);
  lv_obj_set_pos(scanList, 0, HEADER_H + 50);
  lv_obj_set_style_bg_opa(scanList, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(scanList, 0, 0);
  lv_obj_set_style_pad_all(scanList, 8, 0);
  lv_obj_set_style_pad_row(scanList, 8, 0);
  lv_obj_set_flex_flow(scanList, LV_FLEX_FLOW_COLUMN);

  rowCount = 0;
  showScreen(scr, Page::Scan);
  updateScan();
}

// ================= Toevoegen / bewerken =================

struct {
  bool isNew;
  int index;
  DeviceConfig cfg;
} edit;
lv_obj_t *editForm, *taName, *taKey, *taFull, *taEmpty, *taTankW, *taTankA, *taBattW, *taBattA, *keyboard, *editError;
// Per grens een oranje (waarschuwing) en rood (alarm) veld
lv_obj_t *taSocW, *taSocA, *taVoltW, *taVoltA, *taHighW, *taHighA, *taLowW, *taLowA;

enum TaKind : intptr_t { TA_TEXT = 0, TA_HEX = 1, TA_NUM = 2 };

bool parseHexKey(const char *s, uint8_t *out) { return parseVictronKey(s, out); }

// Leeg = NAN (alarm uit). Geeft false bij een ongeldige waarde.
bool parseLimit(lv_obj_t *ta, float lo, float hi, float &out, const char *what) {
  if (!ta) return true;
  const char *s = lv_textarea_get_text(ta);
  if (!s[0]) {
    out = NAN;
    return true;
  }
  char *end;
  float v = strtof(s, &end);
  if (*end || v < lo || v > hi) {
    lv_label_set_text_fmt(editError, T("%s: kies %g t/m %g"), what, lo, hi);
    return false;
  }
  out = v;
  return true;
}

void setLimitText(lv_obj_t *ta, float v, int decimals) {
  if (isnan(v)) return;
  char buf[16];
  snprintf(buf, sizeof(buf), "%.*f", decimals, v);
  lv_textarea_set_text(ta, buf);
}

void editBack(lv_event_t *) {
  if (edit.isNew) buildScan();
  else buildDetail(edit.index);
}

void editSave(lv_event_t *) {
  const char *name = lv_textarea_get_text(taName);
  if (!name[0]) {
    lv_label_set_text(editError, T("Vul een naam in"));
    return;
  }
  strlcpy(edit.cfg.name, name, sizeof(edit.cfg.name));

  if (taKey) {
    const char *key = lv_textarea_get_text(taKey);
    if (!key[0]) {
      edit.cfg.hasKey = false;
    } else if (parseHexKey(key, edit.cfg.key)) {
      edit.cfg.hasKey = true;
    } else {
      lv_label_set_text(editError, T("Sleutel moet 32 hex-tekens zijn"));
      return;
    }
  }

  // Mopeka-kalibratie (leeg laten = niet gekalibreerd)
  DeviceConfig &c = edit.cfg;
  if (taFull) {
    float full = c.tankFullMm, empty = c.tankEmptyMm;
    if (!parseLimit(taFull, 0, 2000, full, T("Afstand vol")) || !parseLimit(taEmpty, 0, 2000, empty, T("Afstand leeg")))
      return;
    // Onder de tank is vol de grootste afstand, bovenop de tank (TD40, Pro-200) de kleinste: beide goed
    if (!isnan(full) && !isnan(empty) && fabsf(full - empty) < 10) {
      lv_label_set_text(editError, T("Afstand bij vol en bij leeg moet minstens 10 mm verschillen"));
      return;
    }
    c.tankFullMm = full;
    c.tankEmptyMm = empty;

    // Meldingsgrenzen: bij een ondergrens ligt oranje hoger dan rood, bij vuilwater (bovengrens) lager
    float tw = c.tankWarn, ta = c.tankAlarm, bw = c.battWarn, ba = c.battAlarm;
    if (!parseLimit(taTankW, 0, 100, tw, T("Tank")) || !parseLimit(taTankA, 0, 100, ta, T("Tank")) ||
        !parseLimit(taBattW, 0, 100, bw, T("Batterij")) || !parseLimit(taBattA, 0, 100, ba, T("Batterij")))
      return;
    bool whenFull = tankAlarmWhenFull(c.tankMedium);
    if ((!whenFull && tw < ta) || bw < ba) {
      lv_label_set_text(editError, T("Oranje moet hoger liggen dan rood"));
      return;
    }
    if (whenFull && tw > ta) {
      lv_label_set_text(editError, T("Oranje moet lager liggen dan rood"));
      return;
    }
    c.tankWarn = tw;
    c.tankAlarm = ta;
    c.battWarn = bw;
    c.battAlarm = ba;
  }

  // DeviceConfig is packed: eerst in lokale variabelen inlezen
  float socW = c.socWarn, socA = c.socLow, voltW = c.voltWarn, voltA = c.voltLow;
  float highW = c.tempHighWarn, highA = c.tempHigh, lowW = c.tempLowWarn, lowA = c.tempLow;
  if (!parseLimit(taSocW, 0, 100, socW, "SoC") || !parseLimit(taSocA, 0, 100, socA, "SoC") ||
      !parseLimit(taVoltW, 0, 70, voltW, T("Spanning")) || !parseLimit(taVoltA, 0, 70, voltA, T("Spanning")) ||
      !parseLimit(taHighW, -40, 100, highW, T("Temp boven")) || !parseLimit(taHighA, -40, 100, highA, T("Temp boven")) ||
      !parseLimit(taLowW, -40, 100, lowW, T("Temp onder")) || !parseLimit(taLowA, -40, 100, lowA, T("Temp onder")))
    return;
  // De waarschuwing hoort eerder te komen dan het alarm
  if ((socW < socA) || (voltW < voltA) || (lowW < lowA) || (highW > highA)) {
    lv_label_set_text(editError, T("Oranje moet eerder komen dan rood"));
    return;
  }
  c.socWarn = socW;
  c.socLow = socA;
  c.voltWarn = voltW;
  c.voltLow = voltA;
  c.tempHighWarn = highW;
  c.tempHigh = highA;
  c.tempLowWarn = lowW;
  c.tempLow = lowA;

  bool ok = edit.isNew ? store_add(edit.cfg) : store_update(edit.index, edit.cfg);
  if (!ok) {
    lv_label_set_text(editError, T("Opslaan mislukt"));
    return;
  }
  if (edit.isNew) buildDashboard();
  else buildDetail(edit.index);
}

void hideKeyboard(lv_event_t * = nullptr) {
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_height(editForm, 480 - HEADER_H);
}

void taEvent(lv_event_t *e) {
  lv_obj_t *ta = lv_event_get_target(e);
  auto kind = (intptr_t)lv_obj_get_user_data(ta);
  lv_keyboard_set_textarea(keyboard, ta);
  lv_keyboard_set_mode(keyboard, kind == TA_NUM   ? LV_KEYBOARD_MODE_NUMBER
                                 : kind == TA_HEX ? LV_KEYBOARD_MODE_TEXT_UPPER
                                                  : LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  // Formulier kleiner maken zodat het veld boven het toetsenbord in beeld scrolt
  lv_obj_set_height(editForm, 480 - HEADER_H - 200);
  lv_obj_update_layout(editForm);
  lv_obj_scroll_to_view_recursive(ta, LV_ANIM_ON);
}

lv_obj_t *makeFormRow(lv_obj_t *parent) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 16, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  return row;
}

// Label + invoerveld onder elkaar
lv_obj_t *makeField(lv_obj_t *parent, const char *label, int width, int maxLen, const char *placeholder, TaKind kind) {
  lv_obj_t *col = lv_obj_create(parent);
  lv_obj_set_size(col, width, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(col, 0, 0);
  lv_obj_set_style_pad_all(col, 0, 0);
  lv_obj_set_style_pad_row(col, 4, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  if (label) makeLabel(col, &lv_font_montserrat_14, COL_TEXT, label);

  lv_obj_t *ta = lv_textarea_create(col);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_max_length(ta, maxLen);
  lv_textarea_set_placeholder_text(ta, placeholder);
  lv_obj_set_width(ta, lv_pct(100));
  lv_obj_set_user_data(ta, (void *)(intptr_t)kind);
  if (kind == TA_NUM) lv_textarea_set_accepted_chars(ta, "0123456789.-");
  if (kind == TA_HEX) lv_textarea_set_accepted_chars(ta, "0123456789abcdefABCDEF");
  lv_obj_add_event_cb(ta, taEvent, LV_EVENT_FOCUSED, nullptr);
  return ta;
}

// Rij met een oranje en een rode grens, zoals in de andere formulieren
void warnAlarmRow(lv_obj_t *parent, const char *label, float w, float a, int dec, lv_obj_t *&warn, lv_obj_t *&alarm) {
  const int labelW = 150, fieldW = (W - 32 - labelW - 2 * 12) / 2;
  lv_obj_t *r = makeFormRow(parent);
  lv_obj_set_style_pad_column(r, 12, 0);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *l = makeLabel(r, &lv_font_montserrat_14, COL_TEXT, label);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, labelW);
  warn = makeField(r, nullptr, fieldW, 5, T("uit"), TA_NUM);
  alarm = makeField(r, nullptr, fieldW, 5, T("uit"), TA_NUM);
  lv_obj_set_style_border_color(warn, COL_WARN, 0);
  lv_obj_set_style_border_color(alarm, COL_ERR, 0);
  lv_obj_set_style_border_width(warn, 1, 0);
  lv_obj_set_style_border_width(alarm, 1, 0);
  setLimitText(warn, w, dec);
  setLimitText(alarm, a, dec);
}

bool hasTemperature(SensorType t) {
  return t == SensorType::Ruuvi || t == SensorType::AtcThermo || t == SensorType::BTHome || t == SensorType::ThermoBeacon;
}

void buildEdit(bool isNew, int index, const char *mac) {
  memset(&edit, 0, sizeof(edit));
  edit.isNew = isNew;
  edit.index = index;
  if (isNew) {
    Discovered d;
    if (!store_find_discovered(mac, d)) {
      buildScan();
      return;
    }
    strlcpy(edit.cfg.mac, d.mac, sizeof(edit.cfg.mac));
    strlcpy(edit.cfg.name, d.name[0] ? d.name : T(typeName(d.type)), sizeof(edit.cfg.name));
    edit.cfg.type = d.type;
    deviceConfigDefaults(edit.cfg);
  } else {
    DeviceState s;
    if (!store_get(index, s)) {
      buildDashboard();
      return;
    }
    edit.cfg = s.cfg;
  }

  lv_obj_t *scr = makeScreen();
  makeHeader(scr, isNew ? T("Nieuw apparaat") : T("Apparaat bewerken"), editBack);

  editForm = lv_obj_create(scr);
  lv_obj_set_size(editForm, W, 480 - HEADER_H);
  lv_obj_set_pos(editForm, 0, HEADER_H);
  lv_obj_set_style_bg_opa(editForm, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(editForm, 0, 0);
  lv_obj_set_style_pad_all(editForm, 16, 0);
  lv_obj_set_style_pad_row(editForm, 12, 0);
  lv_obj_set_flex_flow(editForm, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(editForm, LV_DIR_VER);

  lv_obj_t *info = makeLabel(editForm, &lv_font_montserrat_14, COL_MUTED);
  lv_label_set_text_fmt(info, "%s  -  %s", T(typeName(edit.cfg.type)), edit.cfg.mac);

  taName = makeField(editForm, T("Naam"), W - 32, sizeof(edit.cfg.name) - 1, T("bv. Huishoudaccu"), TA_TEXT);
  lv_textarea_set_text(taName, edit.cfg.name);

  taKey = nullptr;
  if (isVictron(edit.cfg.type)) {
    taKey = makeField(editForm, T("Sleutel (VictronConnect > Productinfo > Instant readout)"), W - 32, 32,
                      T("32 hex-tekens"), TA_HEX);
    if (edit.cfg.hasKey) {
      char hex[33];
      for (int i = 0; i < 16; i++) sprintf(hex + 2 * i, "%02X", edit.cfg.key[i]);
      lv_textarea_set_text(taKey, hex);
    }
  }

  // Mopeka: afstand die de sensor meet als de tank vol / leeg is (meet die af op het scherm van de tegel)
  taFull = taEmpty = taTankW = taTankA = taBattW = taBattA = nullptr;
  if (edit.cfg.type == SensorType::MopekaPro) {
    // Inhoud: bepaalt de geluidssnelheid (en dus de afstand) en of de melding bij leeg of bij vol komt
    lv_obj_t *mrow = makeFormRow(editForm);
    lv_obj_set_flex_align(mrow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    makeLabel(mrow, &lv_font_montserrat_16, COL_TEXT, T("Inhoud van de tank"));
    static char mopts[96];
    mopts[0] = 0;
    for (int i = 0; i < (int)TankMedium::Count; i++) {
      if (i) strlcat(mopts, "\n", sizeof(mopts));
      size_t at = strlen(mopts);
      strlcat(mopts, T(tankMediumName((TankMedium)i)), sizeof(mopts));
      if (mopts[at] >= 'a' && mopts[at] <= 'z') mopts[at] -= 32;  // hoofdletter in de keuzelijst
    }
    lv_obj_t *md = lv_dropdown_create(mrow);
    themeDropdown(md);
    lv_dropdown_set_options(md, mopts);
    lv_dropdown_set_selected(md, edit.cfg.tankMedium < (uint8_t)TankMedium::Count ? edit.cfg.tankMedium : 0);
    lv_obj_set_width(md, 200);
    lv_obj_add_event_cb(md, [](lv_event_t *e) {
      bool wasFull = tankAlarmWhenFull(edit.cfg.tankMedium);
      edit.cfg.tankMedium = lv_dropdown_get_selected(lv_event_get_target(e));
      bool full = tankAlarmWhenFull(edit.cfg.tankMedium);
      if (!taTankW || full == wasFull) return;
      // Melding draait om: label aanpassen, en standaardgrenzen meenemen als ze nog standaard zijn
      lv_label_set_text(lv_obj_get_child(lv_obj_get_parent(taTankW), 0), full ? T("Tank vol boven (%)") : T("Tank leeg onder (%)"));
      const char *w = lv_textarea_get_text(taTankW), *a = lv_textarea_get_text(taTankA);
      if (full && !strcmp(w, "20") && !strcmp(a, "10")) {
        lv_textarea_set_text(taTankW, "75");
        lv_textarea_set_text(taTankA, "90");
      } else if (!full && !strcmp(w, "75") && !strcmp(a, "90")) {
        lv_textarea_set_text(taTankW, "20");
        lv_textarea_set_text(taTankA, "10");
      }
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *h = makeLabel(editForm, &lv_font_montserrat_16, COL_ACCENT, T("Tankkalibratie"));
    lv_obj_set_style_pad_top(h, 6, 0);
    lv_obj_t *hint = makeLabel(editForm, &lv_font_montserrat_14, COL_MUTED,
                               T("Kies eerst de inhoud en sla op. Vul daarna de afstand in mm in die de tegel toont als de tank vol is, en als hij leeg is."));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, W - 32);
    lv_obj_t *row = makeFormRow(editForm);
    int half = (W - 32 - 16) / 2;
    taFull = makeField(row, T("Afstand bij vol (mm)"), half, 4, T("bv. 180"), TA_NUM);
    taEmpty = makeField(row, T("Afstand bij leeg (mm)"), half, 4, T("bv. 30"), TA_NUM);
    setLimitText(taFull, edit.cfg.tankFullMm, 0);
    setLimitText(taEmpty, edit.cfg.tankEmptyMm, 0);

    lv_obj_t *mh = makeLabel(editForm, &lv_font_montserrat_16, COL_ACCENT, T(LV_SYMBOL_BELL "  Meldingen Mopeka"));
    lv_obj_set_style_pad_top(mh, 6, 0);
    makeLabel(editForm, &lv_font_montserrat_14, COL_MUTED, T("Tankniveau alleen na kalibratie. Leeg laten = uit."));
    lv_obj_t *lh = makeFormRow(editForm);
    lv_obj_set_style_pad_column(lh, 12, 0);
    const int lw = 150, fw = (W - 32 - lw - 2 * 12) / 2;
    lv_obj_set_width(makeLabel(lh, &lv_font_montserrat_14, COL_MUTED, ""), lw);
    lv_obj_set_width(makeLabel(lh, &lv_font_montserrat_14, COL_WARN, T(LV_SYMBOL_WARNING " Waarschuwing")), fw);
    lv_obj_set_width(makeLabel(lh, &lv_font_montserrat_14, COL_ERR, T(LV_SYMBOL_BELL " Alarm")), fw);
    warnAlarmRow(editForm, tankAlarmWhenFull(edit.cfg.tankMedium) ? T("Tank vol boven (%)") : T("Tank leeg onder (%)"),
                 edit.cfg.tankWarn, edit.cfg.tankAlarm, 0, taTankW, taTankA);
    warnAlarmRow(editForm, T("Batterij onder (%)"), edit.cfg.battWarn, edit.cfg.battAlarm, 0, taBattW, taBattA);
  }

  // Grenzen, afhankelijk van wat het apparaat meet: kolom oranje (waarschuwing) en rood (alarm)
  taSocW = taSocA = taVoltW = taVoltA = taHighW = taHighA = taLowW = taLowA = nullptr;
  bool battery = isShunt(edit.cfg.type), temp = hasTemperature(edit.cfg.type);
  if (battery || temp) {
    lv_obj_t *h = makeLabel(editForm, &lv_font_montserrat_16, COL_ACCENT, T(LV_SYMBOL_BELL "  Meldingen"));
    lv_obj_set_style_pad_top(h, 6, 0);
    makeLabel(editForm, &lv_font_montserrat_14, COL_MUTED, T("Leeg laten = uit. Alleen rood laat de zoemer gaan."));

    const int labelW = 150, fieldW = (W - 32 - labelW - 2 * 12) / 2;
    lv_obj_t *head = makeFormRow(editForm);
    lv_obj_set_style_pad_column(head, 12, 0);
    lv_obj_set_width(makeLabel(head, &lv_font_montserrat_14, COL_MUTED, ""), labelW);
    lv_obj_set_width(makeLabel(head, &lv_font_montserrat_14, COL_WARN, T(LV_SYMBOL_WARNING " Waarschuwing")), fieldW);
    lv_obj_set_width(makeLabel(head, &lv_font_montserrat_14, COL_ERR, T(LV_SYMBOL_BELL " Alarm")), fieldW);

    auto limitRow = [&](const char *label, lv_obj_t *&warn, lv_obj_t *&alarm, float w, float a, int dec) {
      lv_obj_t *r = makeFormRow(editForm);
      lv_obj_set_style_pad_column(r, 12, 0);
      lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      lv_obj_t *l = makeLabel(r, &lv_font_montserrat_14, COL_TEXT, label);
      lv_obj_set_width(l, labelW);
      warn = makeField(r, nullptr, fieldW, 5, T("uit"), TA_NUM);
      alarm = makeField(r, nullptr, fieldW, 5, T("uit"), TA_NUM);
      lv_obj_set_style_border_color(warn, COL_WARN, 0);
      lv_obj_set_style_border_color(alarm, COL_ERR, 0);
      lv_obj_set_style_border_width(warn, 1, 0);
      lv_obj_set_style_border_width(alarm, 1, 0);
      setLimitText(warn, w, dec);
      setLimitText(alarm, a, dec);
    };
    const DeviceConfig &c = edit.cfg;
    if (battery) {
      limitRow(T("SoC onder (%)"), taSocW, taSocA, c.socWarn, c.socLow, 0);
      limitRow(T("Spanning onder (V)"), taVoltW, taVoltA, c.voltWarn, c.voltLow, 1);
    } else {
      limitRow(T("Temp boven (°C)"), taHighW, taHighA, c.tempHighWarn, c.tempHigh, 1);
      limitRow(T("Temp onder (°C)"), taLowW, taLowA, c.tempLowWarn, c.tempLow, 1);
    }
  }

  lv_obj_t *row = makeFormRow(editForm);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(row, 6, 0);
  lv_obj_t *save = makeButton(row, T(LV_SYMBOL_OK "  Opslaan"), COL_ACCENT, editSave);
  lv_obj_set_size(save, 160, 44);
  editError = makeLabel(row, &lv_font_montserrat_14, COL_ERR);
  lv_obj_set_flex_grow(editError, 1);
  lv_label_set_long_mode(editError, LV_LABEL_LONG_WRAP);

  keyboard = lv_keyboard_create(scr);
  lv_obj_set_size(keyboard, W, 200);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(keyboard, nullptr);
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(keyboard, hideKeyboard, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(keyboard, hideKeyboard, LV_EVENT_CANCEL, nullptr);

  showScreen(scr, Page::Edit);
}

// ================= NMEA 2000-alarmgrenzen =================

struct LimitField {
  lv_obj_t *warn, *alarm;
};
LimitField limDepth, limFuel, limWater, limWaste, limCoolant, limOil;

LimitField makeLimitRow(lv_obj_t *parent, const char *label, float w, float a, int dec) {
  const int labelW = 150, fieldW = (W - 32 - labelW - 2 * 12) / 2;
  lv_obj_t *r = makeFormRow(parent);
  lv_obj_set_style_pad_column(r, 12, 0);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *l = makeLabel(r, &lv_font_montserrat_14, COL_TEXT, label);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, labelW);
  LimitField f;
  f.warn = makeField(r, nullptr, fieldW, 5, T("uit"), TA_NUM);
  f.alarm = makeField(r, nullptr, fieldW, 5, T("uit"), TA_NUM);
  lv_obj_set_style_border_color(f.warn, COL_WARN, 0);
  lv_obj_set_style_border_color(f.alarm, COL_ERR, 0);
  lv_obj_set_style_border_width(f.warn, 1, 0);
  lv_obj_set_style_border_width(f.alarm, 1, 0);
  setLimitText(f.warn, w, dec);
  setLimitText(f.alarm, a, dec);
  return f;
}

// below = alarm als de waarde onder de grens zakt (oranje moet dan hoger liggen dan rood)
bool readLimits(const LimitField &f, float lo, float hi, const char *what, bool below, float &w, float &a) {
  if (!parseLimit(f.warn, lo, hi, w, what) || !parseLimit(f.alarm, lo, hi, a, what)) return false;
  if (below ? w < a : w > a) {
    lv_label_set_text_fmt(editError, T("%s: oranje moet eerder komen dan rood"), what);
    return false;
  }
  return true;
}

void buildN2kLimits() {
  lv_obj_t *scr = makeScreen();
  makeHeader(scr, T("NMEA 2000-alarmen"), [](lv_event_t *) { buildSettings(); });
  const Settings &s = settings();

  editForm = lv_obj_create(scr);
  lv_obj_set_size(editForm, W, 480 - HEADER_H);
  lv_obj_set_pos(editForm, 0, HEADER_H);
  lv_obj_set_style_bg_opa(editForm, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(editForm, 0, 0);
  lv_obj_set_style_pad_all(editForm, 16, 0);
  lv_obj_set_style_pad_row(editForm, 10, 0);
  lv_obj_set_flex_flow(editForm, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(editForm, LV_DIR_VER);

  lv_obj_t *info = makeLabel(editForm, &lv_font_montserrat_14, s.n2kAlarms ? COL_MUTED : COL_WARN,
                             s.n2kAlarms ? T("Leeg laten = dat alarm uit. Alleen rood laat de zoemer gaan.")
                                         : T("Let op: de NMEA 2000-alarmen staan uit in de instellingen."));
  lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(info, W - 32);
  const int labelW = 150, fieldW = (W - 32 - labelW - 2 * 12) / 2;
  lv_obj_t *head = makeFormRow(editForm);
  lv_obj_set_style_pad_column(head, 12, 0);
  lv_obj_set_width(makeLabel(head, &lv_font_montserrat_14, COL_MUTED, ""), labelW);
  lv_obj_set_width(makeLabel(head, &lv_font_montserrat_14, COL_WARN, T(LV_SYMBOL_WARNING " Waarschuwing")), fieldW);
  lv_obj_set_width(makeLabel(head, &lv_font_montserrat_14, COL_ERR, T(LV_SYMBOL_BELL " Alarm")), fieldW);

  limDepth = makeLimitRow(editForm, T("Diepte onder (m)"), s.depthWarn, s.depthAlarm, 1);
  limFuel = makeLimitRow(editForm, T("Diesel onder (%)"), s.fuelWarn, s.fuelAlarm, 0);
  limWater = makeLimitRow(editForm, T("Water onder (%)"), s.waterWarn, s.waterAlarm, 0);
  limWaste = makeLimitRow(editForm, T("Vuilwater boven (%)"), s.wasteWarn, s.wasteAlarm, 0);
  limCoolant = makeLimitRow(editForm, T("Koelwater boven (°C)"), s.coolantWarn, s.coolantAlarm, 0);
  limOil = makeLimitRow(editForm, T("Oliedruk onder (bar), motor draait"), s.oilWarn, s.oilAlarm, 1);

  lv_obj_t *row = makeFormRow(editForm);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(row, 6, 0);
  lv_obj_t *save = makeButton(row, T(LV_SYMBOL_OK "  Opslaan"), COL_ACCENT, [](lv_event_t *) {
    Settings n = settings();
    if (!readLimits(limDepth, 0, 100, T("Diepte"), true, n.depthWarn, n.depthAlarm) ||
        !readLimits(limFuel, 0, 100, T("Diesel"), true, n.fuelWarn, n.fuelAlarm) ||
        !readLimits(limWater, 0, 100, T("Water"), true, n.waterWarn, n.waterAlarm) ||
        !readLimits(limWaste, 0, 100, T("Vuilwater"), false, n.wasteWarn, n.wasteAlarm) ||
        !readLimits(limCoolant, 0, 150, T("Koelwater"), false, n.coolantWarn, n.coolantAlarm) ||
        !readLimits(limOil, 0, 20, T("Oliedruk"), true, n.oilWarn, n.oilAlarm))
      return;
    settings_save(n);
    buildSettings();
  });
  lv_obj_set_size(save, 160, 44);
  editError = makeLabel(row, &lv_font_montserrat_14, COL_ERR);
  lv_obj_set_flex_grow(editError, 1);
  lv_label_set_long_mode(editError, LV_LABEL_LONG_WRAP);

  keyboard = lv_keyboard_create(scr);
  lv_obj_set_size(keyboard, W, 200);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(keyboard, nullptr);
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(keyboard, hideKeyboard, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(keyboard, hideKeyboard, LV_EVENT_CANCEL, nullptr);

  showScreen(scr, Page::N2kLimits);
}

// ================= Details =================

int detailIndex;
lv_obj_t *detailTable;

void updateDetail() {
  DeviceState d;
  if (!store_get(detailIndex, d)) return;
  int row = 0;
  char buf[48];
  auto put = [&](const char *k, const char *v) {
    lv_table_set_cell_value(detailTable, row, 0, k);
    lv_table_set_cell_value(detailTable, row, 1, v);
    row++;
  };
  static Alarm *al = psram_alloc<Alarm>(MAX_ALARMS);  // niet op de kleine stack van de lus, niet in het interne RAM
  if (!al) return;
  int na = alarms_active(al, MAX_ALARMS);
  for (int i = 0; i < na; i++)
    if (strcmp(al[i].mac, d.cfg.mac) == 0) {
      const char *msg = strstr(al[i].text, ": ");  // ": " en niet ":", want een MAC-adres bevat ook dubbele punten
      put(al[i].level == AlarmLevel::Alarm ? T(LV_SYMBOL_BELL " Alarm") : T(LV_SYMBOL_WARNING " Waarschuwing"),
          msg ? msg + 2 : al[i].text);
    }

  if (d.hasData)
    for (int i = 0; i < d.reading.count; i++) {
      char vt[40];
      reading_value_text(d.reading, i, vt, sizeof(vt));
      put(T(d.reading.values[i].label), vt);
    }

  lv_color_t c;
  deviceStatus(d, buf, sizeof(buf), c);
  put(T("Status"), buf);
  // Ingestelde grenzen als "oranje / rood"
  auto limits = [&](const char *k, float w, float a, const char *fmt) {
    if (isnan(w) && isnan(a)) return;
    char ws[16], as[16];
    strlcpy(ws, T("uit"), sizeof(ws));
    strlcpy(as, T("uit"), sizeof(as));
    if (!isnan(w)) snprintf(ws, sizeof(ws), fmt, w);
    if (!isnan(a)) snprintf(as, sizeof(as), fmt, a);
    snprintf(buf, sizeof(buf), "%s  /  %s", ws, as);
    put(k, buf);
  };
  limits(T("SoC onder (oranje/rood)"), d.cfg.socWarn, d.cfg.socLow, "%.0f %%");
  limits(T("Spanning onder"), d.cfg.voltWarn, d.cfg.voltLow, "%.1f V");
  limits(T("Temp boven"), d.cfg.tempHighWarn, d.cfg.tempHigh, "%.1f °C");
  limits(T("Temp onder"), d.cfg.tempLowWarn, d.cfg.tempLow, "%.1f °C");
  if (d.cfg.type == SensorType::MopekaPro && !isnan(d.cfg.tankFullMm) && !isnan(d.cfg.tankEmptyMm)) {
    snprintf(buf, sizeof(buf), T("%.0f mm  /  leeg %.0f mm"), d.cfg.tankFullMm, d.cfg.tankEmptyMm);
    put(T("Kalibratie vol"), buf);
  }
  if (d.cfg.type == SensorType::MopekaPro) {
    limits(tankAlarmWhenFull(d.cfg.tankMedium) ? T("Tank boven (oranje/rood)") : T("Tank onder (oranje/rood)"), d.cfg.tankWarn,
           d.cfg.tankAlarm, "%.0f %%");
    limits(T("Batterij onder (oranje/rood)"), d.cfg.battWarn, d.cfg.battAlarm, "%.0f %%");
  }
  put(T("Type"), T(typeName(d.cfg.type)));
  put(T("MAC-adres"), d.cfg.mac);
  lv_table_set_row_cnt(detailTable, row);
}

void buildDetail(int index) {
  DeviceState d;
  if (!store_get(index, d)) {
    buildDashboard();
    return;
  }
  detailIndex = index;

  lv_obj_t *scr = makeScreen();
  makeHeader(scr, d.cfg.name, [](lv_event_t *) { buildDashboard(); });

  detailTable = lv_table_create(scr);
  lv_obj_set_pos(detailTable, 8, HEADER_H + 8);
  lv_obj_set_size(detailTable, W - 16, 480 - HEADER_H - 80);
  lv_table_set_col_cnt(detailTable, 2);
  lv_table_set_col_width(detailTable, 0, 190);
  lv_table_set_col_width(detailTable, 1, W - 16 - 190 - 4);
  lv_obj_set_style_bg_color(detailTable, COL_CARD, LV_PART_ITEMS);
  lv_obj_set_style_text_color(detailTable, COL_TEXT, LV_PART_ITEMS);
  lv_obj_set_style_border_color(detailTable, COL_BG, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(detailTable, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(detailTable, 0, 0);

  lv_obj_t *editBtn = makeButton(scr, T(LV_SYMBOL_EDIT "  Bewerken"), COL_HEADER,
                                 [](lv_event_t *) { buildEdit(false, detailIndex, nullptr); });
  lv_obj_set_size(editBtn, 220, 48);
  lv_obj_align(editBtn, LV_ALIGN_BOTTOM_LEFT, 12, -12);

  lv_obj_t *delBtn = makeButton(scr, T(LV_SYMBOL_TRASH "  Verwijderen"), COL_ERR, [](lv_event_t *) {
    static const char *btns[3];
    btns[0] = T("Verwijderen");
    btns[1] = T("Annuleren");
    btns[2] = "";
    lv_obj_t *m = lv_msgbox_create(nullptr, T("Verwijderen?"), T("Dit apparaat van het dashboard verwijderen?"), btns, false);
    lv_obj_center(m);
    lv_obj_add_event_cb(
        m,
        [](lv_event_t *e) {
          lv_obj_t *mbox = lv_event_get_current_target(e);
          bool confirm = strcmp(lv_msgbox_get_active_btn_text(mbox), T("Verwijderen")) == 0;
          lv_msgbox_close_async(mbox);
          if (confirm) {
            store_remove(detailIndex);
            buildDashboard();
          }
        },
        LV_EVENT_VALUE_CHANGED, nullptr);
  });
  lv_obj_set_size(delBtn, 220, 48);
  lv_obj_align(delBtn, LV_ALIGN_BOTTOM_RIGHT, -12, -12);

  showScreen(scr, Page::Detail);
  updateDetail();
}

// ================= Instellingen =================

const uint8_t NO_SIGNAL_OPTS[] = {0, 5, 10, 30, 60};
const uint16_t TIMEOUT_OPTS[] = {0, 30, 60, 120, 300, 600};

// Kaart met titel + uitleg links, bediening rechts
// "UTC+2:00", "UTC-3:30", "UTC" (kwartieren)
void utcText(char *buf, size_t len, int32_t m) {
  if (m == 0) snprintf(buf, len, "UTC");
  else snprintf(buf, len, "UTC%c%ld:%02ld", m < 0 ? '-' : '+', (long)(labs(m) / 60), (long)(labs(m) % 60));
}

lv_obj_t *makeSettingRow(lv_obj_t *parent, const char *title, const char *sub) {
  lv_obj_t *card = makeCard(parent, W - 24, 72, -1);
  lv_obj_t *t = makeLabel(card, &lv_font_montserrat_16, COL_TEXT, title);
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 2);
  lv_obj_t *s = makeLabel(card, &lv_font_montserrat_14, COL_MUTED, sub);
  lv_label_set_long_mode(s, LV_LABEL_LONG_DOT);
  lv_obj_set_size(s, 290, 18);
  lv_obj_align(s, LV_ALIGN_BOTTOM_LEFT, 0, -2);
  return card;
}

// Schakelaar en keuzelijst in de kleuren van het thema (de LVGL-standaardstijl kent alleen licht/donker)
void themeSwitch(lv_obj_t *sw) {
  lv_obj_set_style_bg_color(sw, COL_LINE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(sw, COL_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(sw, lv_color_hex(theme().dark ? theme().text : 0xFFFFFF), LV_PART_KNOB);
}

void themeDropdown(lv_obj_t *dd) {
  lv_obj_set_style_bg_color(dd, COL_HEADER, 0);
  lv_obj_set_style_text_color(dd, COL_TEXT, 0);
  lv_obj_set_style_border_color(dd, COL_LINE, 0);
  lv_obj_t *list = lv_dropdown_get_list(dd);
  lv_obj_set_style_bg_color(list, COL_CARD, 0);
  lv_obj_set_style_text_color(list, COL_TEXT, 0);
  lv_obj_set_style_border_color(list, COL_LINE, 0);
  lv_obj_set_style_bg_color(list, COL_ACCENT, LV_PART_SELECTED | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(list, COL_INK, LV_PART_SELECTED | LV_STATE_CHECKED);
}

lv_obj_t *makeSwitch(lv_obj_t *card, bool on, lv_event_cb_t cb) {
  lv_obj_t *sw = lv_switch_create(card);
  themeSwitch(sw);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
  if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, nullptr);
  return sw;
}

lv_obj_t *makeDropdown(lv_obj_t *card, const char *opts, int selected, lv_event_cb_t cb) {
  lv_obj_t *dd = lv_dropdown_create(card);
  themeDropdown(dd);
  lv_dropdown_set_options(dd, opts);
  lv_dropdown_set_selected(dd, selected);
  lv_obj_set_width(dd, 120);
  lv_obj_align(dd, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(dd, cb, LV_EVENT_VALUE_CHANGED, nullptr);
  return dd;
}

template <typename E, size_t N>
int indexOf(const E (&arr)[N], E v) {
  for (size_t i = 0; i < N; i++)
    if (arr[i] == v) return i;
  return 0;
}

// ---------- Thema: kiezen en automatisch wisselen ----------

// Laatst bekende positie (van de GPS op NMEA 2000) voor zonsopgang en -ondergang; bewaard over een herstart
bool themePosKnown = false;
float themeLat, themeLon;

void loadThemePos() {
  Preferences p;
  p.begin("thema", true);
  float pos[2];
  if (p.getBytesLength("pos") == sizeof(pos) && p.getBytes("pos", pos, sizeof(pos)) == sizeof(pos) && !isnan(pos[0])) {
    themeLat = pos[0];
    themeLon = pos[1];
    themePosKnown = true;
  }
  p.end();
}

void updateThemePos() {
  if (!n2k.posMs || millis() - n2k.posMs > 60000 || isnan(n2k.lat) || isnan(n2k.lon)) return;
  // Alleen bewaren bij een flinke verplaatsing (> 0,5°, ± 50 km): weinig schrijfacties
  bool moved = !themePosKnown || fabs(n2k.lat - themeLat) > 0.5 || fabs(n2k.lon - themeLon) > 0.5;
  themeLat = n2k.lat;
  themeLon = n2k.lon;
  themePosKnown = true;
  if (!moved) return;
  float pos[2] = {themeLat, themeLon};
  Preferences p;
  p.begin("thema", false);
  p.putBytes("pos", pos, sizeof(pos));
  p.end();
}

// Welk thema hoort er nu? Met automatisch wisselen na zonsondergang het nachtthema.
ThemeId wantedTheme() {
  const Settings &s = settings();
  ThemeId day = (ThemeId)(s.theme < (uint8_t)ThemeId::Count ? s.theme : 0);
  if (!s.autoTheme || !clock_valid()) return day;
  struct tm t;
  uint32_t now = clock_now();
  int mins = clock_local(now, t) ? t.tm_hour * 60 + t.tm_min : 12 * 60;
  bool dark = theme_is_dark_outside(now, themePosKnown, themeLat, themeLon, mins);
  return dark ? (ThemeId)(s.themeNight < (uint8_t)ThemeId::Count ? s.themeNight : (uint8_t)ThemeId::Night) : day;
}

// Thema toepassen: kleuren, LVGL-standaardstijl (lijsten, schakelaars), helderheid
void applyTheme(ThemeId id) {
  theme_set(id);
  lv_disp_t *disp = lv_disp_get_default();
  lv_theme_t *th = lv_theme_default_init(disp, COL_ACCENT, COL_WARN, theme().dark, &lv_font_montserrat_16);
  lv_disp_set_theme(disp, th);
  display_set_max_brightness(theme().maxBrightness);
}

// Eens per minuut: wisselen als het donker of licht wordt. Alleen op het dashboard (niet midden in het bewerken).
void themeTick() {
  static uint32_t last = 0;
  updateThemePos();
  if (last && millis() - last < 60000) return;
  last = millis();
  ThemeId want = wantedTheme();
  if (want == theme_active() || page != Page::Dashboard) {
    if (want != theme_active()) last = 0;  // straks opnieuw proberen
    return;
  }
  logf("[thema] %s\n", theme_name(want));
  applyTheme(want);
  buildDashboard();
}

void buildSettings() {
  lv_obj_t *scr = makeScreen();
  makeHeader(scr, T("Instellingen"), [](lv_event_t *) { buildDashboard(); });

  lv_obj_t *list = lv_obj_create(scr);
  lv_obj_set_size(list, W, 480 - HEADER_H);
  lv_obj_set_pos(list, 0, HEADER_H);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 12, 0);
  lv_obj_set_style_pad_row(list, 8, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);

  const Settings &s = settings();

  // Apparaten die uitgeschakeld zijn omdat het board erdoor crashte
  {
    QEntry q[Q_MAX];
    int nq = quarantine_list(q, Q_MAX);
    for (int i = 0; i < nq; i++) {
      char desc[64];
      quarantine_describe(q[i], desc, sizeof(desc));
      lv_obj_t *qc = makeSettingRow(list, desc, T("Uitgeschakeld na crashes"));
      lv_obj_t *title = lv_obj_get_child(qc, 0);
      lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);  // lange namen niet onder de knop door
      lv_obj_set_size(title, 280, 20);
      lv_obj_set_style_text_color(lv_obj_get_child(qc, 1), COL_ERR, 0);
      lv_obj_t *b = makeButton(qc, T("Inschakelen"), COL_HEADER, [](lv_event_t *e) {
        quarantine_release((int)(intptr_t)lv_event_get_user_data(e));
        buildSettings();
      }, (void *)(intptr_t)i);
      lv_obj_set_size(b, 140, 40);
      lv_obj_align(b, LV_ALIGN_RIGHT_MID, 0, 0);
    }
  }

  // Apparaat toevoegen
  {
    char sub[48];
    snprintf(sub, sizeof(sub), T("%d van %d apparaten"), store_count(), MAX_DEVICES);
    lv_obj_t *ac = makeSettingRow(list, T("Apparaat toevoegen"), sub);
    lv_obj_t *ab = makeButton(ac, LV_SYMBOL_PLUS, COL_ACCENT, [](lv_event_t *) { scanFromSettings = true; buildScan(); });
    lv_obj_set_size(ab, 120, 40);
    lv_obj_align(ab, LV_ALIGN_RIGHT_MID, 0, 0);
  }

  // Taal
  {
    lv_obj_t *lc = makeSettingRow(list, T("Taal"), i18n_name(i18n_lang()));
    lv_obj_t *lb = makeButton(lc, T("Kiezen"), COL_HEADER, [](lv_event_t *) { buildLanguage(false); });
    lv_obj_set_size(lb, 120, 40);
    lv_obj_align(lb, LV_ALIGN_RIGHT_MID, 0, 0);
  }

  // Thema, en automatisch wisselen tussen dag en nacht
  {
    static char opts[96];
    opts[0] = 0;
    for (int i = 0; i < (int)ThemeId::Count; i++) {
      if (i) strlcat(opts, "\n", sizeof(opts));
      strlcat(opts, T(theme_name((ThemeId)i)), sizeof(opts));
    }
    lv_obj_t *tc = makeSettingRow(list, T("Thema"), s.autoTheme ? T("Overdag") : T("Kleuren van het scherm"));
    lv_obj_t *dd = makeDropdown(tc, opts, s.theme < (uint8_t)ThemeId::Count ? s.theme : 0, [](lv_event_t *e) {
      Settings n = settings();
      n.theme = lv_dropdown_get_selected(lv_event_get_target(e));
      settings_save(n);
      applyTheme(wantedTheme());
      buildSettings();  // meteen in de nieuwe kleuren
    });
    lv_obj_set_width(dd, 150);

    lv_obj_t *ac = makeSettingRow(list, T("Automatisch dag/nacht"),
                                  themePosKnown ? T("Op zonsop- en ondergang (GPS)") : T("Vaste tijden 21:00–7:00 (geen GPS)"));
    makeSwitch(ac, s.autoTheme, [](lv_event_t *e) {
      Settings n = settings();
      n.autoTheme = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
      settings_save(n);
      applyTheme(wantedTheme());
      buildSettings();
    });
    if (s.autoTheme) {
      lv_obj_t *nc = makeSettingRow(list, T("Thema 's nachts"), T("Na zonsondergang tot zonsopgang"));
      lv_obj_t *nd = makeDropdown(nc, opts, s.themeNight < (uint8_t)ThemeId::Count ? s.themeNight : 3, [](lv_event_t *e) {
        Settings n = settings();
        n.themeNight = lv_dropdown_get_selected(lv_event_get_target(e));
        settings_save(n);
        applyTheme(wantedTheme());
        buildSettings();
      });
      lv_obj_set_width(nd, 150);
    }
  }

  // Klok
  char clk[64];
  struct tm t;
  if (clock_local(clock_now(), t))
    snprintf(clk, sizeof(clk), "%s %d %s %d  %02d:%02d", T(WEEKDAYS[t.tm_wday]), t.tm_mday, T(MONTHS[t.tm_mon]),
             t.tm_year + 1900, t.tm_hour, t.tm_min);
  else snprintf(clk, sizeof(clk), T("Niet ingesteld"));
  char sub[96];
  if (clock_rtc_lost_power()) snprintf(sub, sizeof(sub), T("RTC-batterij leeg? Stel de tijd opnieuw in"));
  else snprintf(sub, sizeof(sub), "%s", clock_rtc_present() ? T("RTC gevonden: tijd blijft bewaard")
                                                           : T("Geen RTC: tijd weg na stroomuitval"));
  lv_obj_t *card = makeSettingRow(list, clk, sub);
  if (clock_rtc_lost_power() || !clock_valid()) lv_obj_set_style_text_color(lv_obj_get_child(card, 1), COL_WARN, 0);
  lv_obj_t *setBtn = makeButton(card, T("Instellen"), COL_HEADER, [](lv_event_t *) { buildClockSet(); });
  lv_obj_set_size(setBtn, 120, 40);
  lv_obj_align(setBtn, LV_ALIGN_RIGHT_MID, 0, 0);

  // Lokale tijd van het NMEA 2000-netwerk: alleen als er ooit een verschil is ontvangen
  if (clock_network_known()) {
    char zone[96];
    utcText(zone, sizeof(zone), clock_network_offset_min());
    char sub2[128];
    snprintf(sub2, sizeof(sub2), T("Plotter geeft %s (NMEA 2000)"), zone);
    card = makeSettingRow(list, T("Tijd van de plotter"), sub2);
    makeSwitch(card, clock_source() == ClockSource::Network, [](lv_event_t *e) {
      clock_set_source(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED) ? ClockSource::Network : ClockSource::Own);
      buildSettings();  // tijd bovenaan bijwerken
    });
  }

  // Micro-SD-kaart (alleen op boards met een kaartslot)
  if (sd_supported()) {
    SdInfo si = sd_info();
    char sdSub[96];
    if (si.present)
      snprintf(sdSub, sizeof(sdSub), T("%.1f GB vrij · metingen elke minuut"),
               (double)(si.totalBytes - si.usedBytes) / 1e9);
    else
      snprintf(sdSub, sizeof(sdSub), "%s", si.wasPresent && si.lastError[0] ? T(si.lastError) : T("Geen kaart (niet nodig)"));
    card = makeSettingRow(list, T("Micro-SD-kaart"), sdSub);
    if (!si.present) {
      lv_obj_t *sub = lv_obj_get_child(card, 1);
      if (si.wasPresent) lv_obj_set_style_text_color(sub, COL_WARN, 0);  // kaart weggevallen
      // De kaart wordt alleen bij het opstarten gezocht: uitleg met een pijl naar de resetknop
      // (Waveshare: aan de rechterkant van voren gezien, de middelste van PWRKEY, RESET, BOOT)
      lv_obj_set_height(card, 112);
      lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 0, 28);
      lv_obj_t *note = makeLabel(card, &lv_font_montserrat_14, COL_MUTED,
                                 T("Kaart erin gedaan? Druk op RESET: de middelste knop aan de rechterkant."));
      lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(note, 330);
      lv_obj_align(note, LV_ALIGN_TOP_LEFT, 0, 52);
      lv_obj_t *arrow = makeLabel(card, &lv_font_montserrat_28, COL_ACCENT, LV_SYMBOL_RIGHT);
      lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -16, -8);
      lv_obj_t *rst = makeLabel(card, &lv_font_montserrat_14, COL_ACCENT, "RESET");
      lv_obj_align_to(rst, arrow, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    }
  }

  card = makeSettingRow(list, T("Zoemer bij alarm"),
                                  buzzer_present() ? T("Alleen rood; oranje piept niet") : T("Geen zoemer aangesloten"));
  makeSwitch(card, s.buzzer, [](lv_event_t *e) {
    Settings n = settings();
    n.buzzer = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    settings_save(n);
  });

  card = makeSettingRow(list, T("Zoemer testen"), T("Laat de zoemer kort piepen"));
  lv_obj_t *test = makeButton(card, T("Test"), COL_HEADER, [](lv_event_t *) { alarms_test_buzzer(); });
  lv_obj_set_size(test, 120, 40);
  lv_obj_align(test, LV_ALIGN_RIGHT_MID, 0, 0);

  card = makeSettingRow(list, T("Victron-alarmen"), T("Alarmen en laderfouten van de apparaten zelf"));
  makeSwitch(card, s.victronAlarms, [](lv_event_t *e) {
    Settings n = settings();
    n.victronAlarms = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    settings_save(n);
  });

  // NMEA 2000
  char n2kSub[64];
  N2kStats ns = n2k_stats();
  if (!n2k_available()) snprintf(n2kSub, sizeof(n2kSub), T("Niet beschikbaar op dit board"));
  else if (!ns.running) snprintf(n2kSub, sizeof(n2kSub), T("Uit"));
  else if (ns.framesPerSec) snprintf(n2kSub, sizeof(n2kSub), T("%lu berichten/s  " LV_SYMBOL_BULLET "  %u apparaten"), (unsigned long)ns.framesPerSec, ns.sources);
  else snprintf(n2kSub, sizeof(n2kSub), T("Geen verkeer: kabel of CAN-module?"));
  card = makeSettingRow(list, "NMEA 2000", n2kSub);
  if (n2k_available()) makeSwitch(card, s.n2k, [](lv_event_t *e) {
    Settings n = settings();
    n.n2k = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    settings_save(n);
    n2k_set_enabled(n.n2k);
  });

  card = makeSettingRow(list, T("NMEA 2000-alarmen"), s.n2kAlarms ? T("Aan: diepte, tanks, motor") : T("Uit"));
  lv_obj_t *limBtn = makeButton(card, T("Grenzen"), COL_HEADER, [](lv_event_t *) { buildN2kLimits(); });
  lv_obj_set_size(limBtn, 100, 40);
  lv_obj_align(limBtn, LV_ALIGN_RIGHT_MID, -66, 0);
  makeSwitch(card, s.n2kAlarms, [](lv_event_t *e) {
    Settings n = settings();
    n.n2kAlarms = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    settings_save(n);
    lv_label_set_text(lv_obj_get_child(lv_obj_get_parent(lv_event_get_target(e)), 1), n.n2kAlarms ? T("Aan: diepte, tanks, motor") : T("Uit"));
  });

  // Wifi-pagina: Victron-sleutels en firmware-update
  const char *otaSub = T("Sleutels invoeren en firmware-update");
  switch (ota_phase()) {
    case OtaPhase::Serving: otaSub = T("Aan: open de pagina op je telefoon"); break;
    case OtaPhase::Connecting: otaSub = T("Verbinden met wifi..."); break;
    case OtaPhase::Staged: otaSub = T("Nieuwe versie klaar: bevestig"); break;
    case OtaPhase::Uploading: otaSub = T("Bezig met uploaden"); break;
    default: break;
  }
  card = makeSettingRow(list, T("Wifi-pagina"), otaSub);
  lv_obj_t *otaBtn = makeButton(card, T("Open"), COL_HEADER, [](lv_event_t *) { buildOta(); });
  lv_obj_set_size(otaBtn, 120, 40);
  lv_obj_align(otaBtn, LV_ALIGN_RIGHT_MID, 0, 0);

  card = makeSettingRow(list, T("Klok via GPS"), ns.lastClockSync ? T("Gelijkgezet met de GPS op NMEA 2000") : T("Zet de klok gelijk zodra er GPS-tijd is"));
  makeSwitch(card, s.gpsClock, [](lv_event_t *e) {
    Settings n = settings();
    n.gpsClock = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    settings_save(n);
  });

  // Sensor voor temperatuur en vochtigheid op het Overzicht (alleen als er een keuze is)
  {
    static char macs[MAX_DEVICES][18];
    static int nMacs;
    nMacs = 0;
    static char opts[MAX_DEVICES * 36 + 32];
    strlcpy(opts, T("Automatisch"), sizeof(opts));
    int sel = 0;
    for (int i = 0; i < snapCount && nMacs < MAX_DEVICES; i++) {
      if (!hasTemperature(snap[i].cfg.type)) continue;
      strlcpy(macs[nMacs], snap[i].cfg.mac, sizeof(macs[0]));
      strlcat(opts, "\n", sizeof(opts));
      strlcat(opts, snap[i].cfg.name, sizeof(opts));
      nMacs++;
      if (strcasecmp(snap[i].cfg.mac, s.overviewSensor) == 0) sel = nMacs;
    }
    if (nMacs >= 2) {
      card = makeSettingRow(list, T("Sensor op overzicht"), T("Temperatuur en vochtigheid"));
      lv_obj_set_width(lv_obj_get_child(card, 1), 240);
      lv_obj_t *dd = makeDropdown(card, opts, sel, [](lv_event_t *e) {
        Settings n = settings();
        int i = lv_dropdown_get_selected(lv_event_get_target(e));
        strlcpy(n.overviewSensor, i > 0 && i <= nMacs ? macs[i - 1] : "", sizeof(n.overviewSensor));
        settings_save(n);
      });
      lv_obj_set_width(dd, 170);  // ruimte voor de naam van de sensor
    }
  }

  // Stormwaarschuwing (alleen als er een barometer is)
  if (baro_available()) {
    static const float STORM_OPTS[] = {NAN, 2, 3, 4, 5};
    int si = 0;
    for (int i = 1; i < 5; i++)
      if (s.stormWarn == STORM_OPTS[i]) si = i;
    card = makeSettingRow(list, T("Stormwaarschuwing"), T("Luchtdruk in 3 uur gedaald (rood bij 2 hPa meer)"));
    makeDropdown(card, T("Uit\n2 hPa\n3 hPa\n4 hPa\n5 hPa"), si, [](lv_event_t *e) {
      Settings n = settings();
      int i = lv_dropdown_get_selected(lv_event_get_target(e));
      n.stormWarn = STORM_OPTS[i];
      n.stormAlarm = isnan(n.stormWarn) ? NAN : n.stormWarn + 2;
      settings_save(n);
    });
  }

  card = makeSettingRow(list, T("Alarm bij geen signaal"), T("Als een apparaat zo lang niets stuurt"));
  makeDropdown(card, T("Uit\n5 min\n10 min\n30 min\n60 min"), indexOf(NO_SIGNAL_OPTS, s.noSignalMin), [](lv_event_t *e) {
    Settings n = settings();
    n.noSignalMin = NO_SIGNAL_OPTS[lv_dropdown_get_selected(lv_event_get_target(e))];
    settings_save(n);
  });

  card = makeSettingRow(list, T("Scherm uit na"), T("Zonder aanraking; tik om aan te zetten"));
  makeDropdown(card, T("Nooit\n30 sec\n1 min\n2 min\n5 min\n10 min"), indexOf(TIMEOUT_OPTS, s.screenTimeoutS),
               [](lv_event_t *e) {
                 Settings n = settings();
                 n.screenTimeoutS = TIMEOUT_OPTS[lv_dropdown_get_selected(lv_event_get_target(e))];
                 settings_save(n);
               });

  // Geheugen en opslag, om op het echte board te kunnen controleren
  StorageInfo si = history_storage_info();
  card = makeCard(list, W - 24, 92, -1);
  lv_obj_t *memTitle = makeLabel(card, &lv_font_montserrat_16, COL_TEXT, T("Geheugen en opslag"));
  lv_obj_align(memTitle, LV_ALIGN_TOP_LEFT, 0, 2);
  lv_obj_t *m = makeLabel(card, &lv_font_montserrat_14, COL_MUTED, "");
  lv_label_set_text_fmt(m, T("Werkgeheugen vrij: %lu KB intern  " LV_SYMBOL_BULLET "  %.1f MB PSRAM\n"
                           "Historie %.1f KB  " LV_SYMBOL_BULLET "  opslag %lu KB van %lu KB gebruikt"),
                        (unsigned long)(si.internalFree / 1024), si.psramFree / 1048576.0f, si.histBytes / 1024.0f,
                        (unsigned long)(si.fsUsed / 1024), (unsigned long)(si.fsTotal / 1024));
  lv_obj_align(m, LV_ALIGN_BOTTOM_LEFT, 0, -2);

  showScreen(scr, Page::Settings);
}

// ================= Klok instellen =================

lv_obj_t *rollDay, *rollMonth, *rollYear, *rollHour, *rollMin;
constexpr int YEAR_FIRST = 2025, YEAR_LAST = 2040;

int daysInMonth(int year, int month) {
  static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return month == 2 && leap ? 29 : d[month - 1];
}

lv_obj_t *makeRoller(lv_obj_t *parent, const char *label, const char *opts, int selected, int width) {
  lv_obj_t *col = lv_obj_create(parent);
  lv_obj_set_size(col, width, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(col, 0, 0);
  lv_obj_set_style_pad_all(col, 0, 0);
  lv_obj_set_style_pad_row(col, 6, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  makeLabel(col, &lv_font_montserrat_14, COL_MUTED, label);

  lv_obj_t *r = lv_roller_create(col);
  lv_roller_set_options(r, opts, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(r, 5);
  lv_roller_set_selected(r, selected, LV_ANIM_OFF);
  lv_obj_set_width(r, width);
  lv_obj_set_style_bg_color(r, COL_CARD, 0);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_text_font(r, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(r, COL_MUTED, 0);
  lv_obj_set_style_bg_color(r, COL_ACCENT, LV_PART_SELECTED);
  lv_obj_set_style_text_color(r, COL_INK, LV_PART_SELECTED);
  return r;
}

// "a\nb\nc" van een getallenreeks
void numberOptions(char *buf, size_t len, int from, int to, bool twoDigits) {
  size_t n = 0;
  for (int v = from; v <= to; v++)
    n = appendf(buf, len, n, twoDigits ? "%s%02d" : "%s%d", v == from ? "" : "\n", v);
}

// Taal kiezen: bij de eerste start (zonder terugknop) en via Instellingen
bool languageFirstStart = false;

void buildLanguage(bool firstStart) {
  languageFirstStart = firstStart;
  lv_obj_t *scr = makeScreen();
  lv_event_cb_t back = [](lv_event_t *) { buildSettings(); };
  makeHeader(scr, T("Taal"), firstStart ? nullptr : back);

  lv_obj_t *list = lv_obj_create(scr);
  lv_obj_set_size(list, W, 480 - HEADER_H);
  lv_obj_set_pos(list, 0, HEADER_H);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 12, 0);
  lv_obj_set_style_pad_row(list, 8, 0);
  lv_obj_set_style_pad_column(list, 8, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_ROW_WRAP);  // twee kolommen: alle talen in één scherm
  lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // Elke taal in de taal zelf, zodat iedereen de eigen taal herkent
  for (int i = 0; i < (int)Lang::COUNT; i++) {
    bool cur = i18n_chosen() && i18n_lang() == (Lang)i;
    char txt[40];
    snprintf(txt, sizeof(txt), "%s%s", cur ? LV_SYMBOL_OK "  " : "", i18n_name((Lang)i));
    lv_obj_t *b = makeButton(list, txt, cur ? COL_ACCENT : COL_HEADER, [](lv_event_t *e) {
      i18n_set((Lang)(intptr_t)lv_event_get_user_data(e));
      logf("[taal] %s\n", i18n_name(i18n_lang()));
      if (languageFirstStart) buildDashboard();  // alle schermen worden in de nieuwe taal opgebouwd
      else buildSettings();
    }, (void *)(intptr_t)i);
    lv_obj_set_size(b, (W - 24 - 8) / 2, 52);
  }
  showScreen(scr, Page::Language);
}

void buildClockSet() {
  lv_obj_t *scr = makeScreen();
  makeHeader(scr, T("Klok instellen"), [](lv_event_t *) { buildSettings(); });

  // Beginwaarde: huidige tijd, of anders de bouwdatum van de firmware
  struct tm t = {};
  if (!clock_local(clock_now(), t)) {
    t.tm_year = 2026 - 1900;
    t.tm_mon = 0;
    t.tm_mday = 1;
    t.tm_hour = 12;
    const char *d = __DATE__;  // "Oct 10 2026"
    for (int m = 0; m < 12; m++)
      if (!strncmp(d, "JanFebMarAprMayJunJulAugSepOctNovDec" + m * 3, 3)) t.tm_mon = m;
    t.tm_mday = atoi(d + 4);
    t.tm_year = atoi(d + 7) - 1900;
  }

  lv_obj_t *row = lv_obj_create(scr);
  lv_obj_set_size(row, W, LV_SIZE_CONTENT);
  lv_obj_set_pos(row, 0, HEADER_H + 20);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  static char days[31 * 3], years[16 * 5], hours[24 * 3], mins[60 * 3], months[12 * 4];
  numberOptions(days, sizeof(days), 1, 31, false);
  numberOptions(years, sizeof(years), YEAR_FIRST, YEAR_LAST, false);
  numberOptions(hours, sizeof(hours), 0, 23, true);
  numberOptions(mins, sizeof(mins), 0, 59, true);
  size_t n = 0;
  for (int m = 0; m < 12; m++) n = appendf(months, sizeof(months), n, "%s%s", m ? "\n" : "", T(MONTHS[m]));

  int year = max(YEAR_FIRST, min(YEAR_LAST, t.tm_year + 1900));
  rollDay = makeRoller(row, T("Dag"), days, t.tm_mday - 1, 64);
  rollMonth = makeRoller(row, T("Maand"), months, t.tm_mon, 80);
  rollYear = makeRoller(row, T("Jaar"), years, year - YEAR_FIRST, 92);
  lv_obj_t *gap = lv_obj_create(row);  // ruimte tussen datum en tijd
  lv_obj_set_size(gap, 8, 1);
  lv_obj_set_style_bg_opa(gap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(gap, 0, 0);
  rollHour = makeRoller(row, T("Uur"), hours, t.tm_hour, 64);
  rollMin = makeRoller(row, T("Min"), mins, t.tm_min, 64);

  lv_obj_t *info = makeLabel(scr, &lv_font_montserrat_14, COL_MUTED,
                             clock_source() == ClockSource::Network
                                 ? T("De tijd komt nu van de plotter. Opslaan zet dat uit: daarna geldt je eigen tijd.")
                             : clock_rtc_present() ? T("Stel je eigen tijd in; zomer- en wintertijd gaan vanzelf.")
                                                   : T("Geen RTC-module: na een stroomuitval opnieuw instellen."));
  lv_obj_align(info, LV_ALIGN_BOTTOM_MID, 0, -84);

  lv_obj_t *save = makeButton(scr, T(LV_SYMBOL_OK "  Opslaan"), COL_ACCENT, [](lv_event_t *) {
    int year = YEAR_FIRST + lv_roller_get_selected(rollYear);
    int month = lv_roller_get_selected(rollMonth) + 1;
    int day = min<int>(lv_roller_get_selected(rollDay) + 1, daysInMonth(year, month));  // 31 feb -> 28/29 feb
    clock_set_local(year, month, day, lv_roller_get_selected(rollHour), lv_roller_get_selected(rollMin));
    buildSettings();
  });
  lv_obj_set_size(save, 200, 52);
  lv_obj_align(save, LV_ALIGN_BOTTOM_MID, 0, -16);

  showScreen(scr, Page::ClockSet);
}

// ================= Wifi-pagina (sleutels en firmware) =================

lv_obj_t *otaLabel, *otaSwitch, *otaSwitchCard, *otaModeCard, *otaIdleCard, *otaPopup, *otaQr, *otaWifiBtn;
char otaQrText[80];

const char *otaStatusText() {
  static char buf[240];
  switch (ota_phase()) {
    case OtaPhase::Connecting: return T("Verbinden met het wifi-netwerk...");
    case OtaPhase::Serving:
      if (ota_ap_name()[0])
        snprintf(buf, sizeof(buf), T("1. Verbind je telefoon met wifi\n    %s\n    wachtwoord %s\n   (of scan de QR-code)\n"
                 "2. De pagina opent vanzelf. Anders: open http://%s"), ota_ap_name(), ota_code(), ota_address());
      else
        snprintf(buf, sizeof(buf), T("Open op je telefoon (zelfde netwerk):\nhttp://%s\nGebruiker: boot   Wachtwoord: %s\n"
                 "Daar kun je Victron-sleutels plakken of firmware uploaden."), ota_address(), ota_code());
      return buf;
    case OtaPhase::Uploading: return T("Bezig met uploaden. Niet de stroom afhalen.");
    case OtaPhase::Staged: return T("Nieuwe firmware ontvangen. Bevestig hieronder.");
    case OtaPhase::Rejected:
      snprintf(buf, sizeof(buf), T("Upload niet gelukt: %s\nhttp://%s  Wachtwoord: %s"), T(ota_reject_reason()), ota_address(),
               ota_code());
      return buf;
    default:
      return wifi_ap_mode() ? T("Uit. Zet aan: het board maakt een eigen wifi-netwerk. Daarop open je met je telefoon "
                              "de pagina voor Victron-sleutels en firmware.")
                            : T("Uit. Zet aan: het board gaat op het ingestelde wifi-netwerk. Daarop open je met je "
                              "telefoon de pagina voor Victron-sleutels en firmware.");
  }
}

void otaRefresh() {
  lv_label_set_text(otaLabel, otaStatusText());
  if (ota_phase() == OtaPhase::Off) lv_obj_clear_state(otaSwitch, LV_STATE_CHECKED);
  else lv_obj_add_state(otaSwitch, LV_STATE_CHECKED);
  // QR-code om met de camera met het eigen wifipunt te verbinden
  char qr[80] = "";
  if (ota_phase() == OtaPhase::Serving && ota_ap_name()[0])
    snprintf(qr, sizeof(qr), "WIFI:T:WPA;S:%s;P:%s;;", ota_ap_name(), ota_code());
  if (qr[0]) {
    if (strcmp(qr, otaQrText) != 0) lv_qrcode_update(otaQr, qr, strlen(qr));
    lv_obj_clear_flag(otaQr, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(otaQr, LV_OBJ_FLAG_HIDDEN);
  }
  strlcpy(otaQrText, qr, sizeof(otaQrText));
  lv_obj_set_width(otaLabel, qr[0] ? W - 32 - 176 : W - 32);
  // Keuzes alleen als de wifi uit staat; als hij aan staat, is er ruimte voor de gegevens en de QR-code
  bool off = ota_phase() == OtaPhase::Off;
  for (lv_obj_t *o : {otaModeCard, otaIdleCard}) {
    if (off) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  }
  if (wifi_ap_mode()) lv_obj_add_flag(otaWifiBtn, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(otaWifiBtn, LV_OBJ_FLAG_HIDDEN);
}

void buildOta() {
  lv_obj_t *scr = makeScreen();
  makeHeader(scr, T("Wifi-pagina"), [](lv_event_t *) { buildSettings(); });

  lv_obj_t *col = lv_obj_create(scr);
  lv_obj_set_size(col, W, 480 - HEADER_H);
  lv_obj_set_pos(col, 0, HEADER_H);
  lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(col, 0, 0);
  lv_obj_set_style_pad_all(col, 16, 0);
  lv_obj_set_style_pad_row(col, 12, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);

  char sub[48];
  snprintf(sub, sizeof(sub), T("Stopt zelf na %u min zonder gebruik"), wifi_idle_min());
  lv_obj_t *card = makeSettingRow(col, T("Wifi-pagina aan"), sub);
  otaSwitchCard = card;
  otaSwitch = lv_switch_create(card);
  themeSwitch(otaSwitch);
  lv_obj_align(otaSwitch, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(otaSwitch, [](lv_event_t *e) {
    if (lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED)) {
      char ssid[40], pass[65];
      if (!wifi_ap_mode() && !wifi_credentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
        lv_label_set_text(otaLabel, T("Stel eerst het wifi-netwerk in."));
        lv_obj_clear_state(otaSwitch, LV_STATE_CHECKED);
        return;
      }
      ota_start();
    } else {
      ota_stop();
    }
    otaRefresh();
  }, LV_EVENT_VALUE_CHANGED, nullptr);

  card = otaModeCard = makeSettingRow(col, T("Verbinding"), "");
  lv_obj_t *mode = makeDropdown(card, T("Eigen wifipunt\nBestaand netwerk"), wifi_ap_mode() ? 0 : 1, [](lv_event_t *e) {
    bool ap = lv_dropdown_get_selected(lv_event_get_target(e)) == 0;
    if (ap == wifi_ap_mode()) return;
    ota_stop();  // eerst uit; tijdens een upload of beslissing gebeurt er niets
    if (ota_phase() != OtaPhase::Off) {
      lv_dropdown_set_selected(lv_event_get_target(e), wifi_ap_mode() ? 0 : 1);
      return;
    }
    wifi_set_ap_mode(ap);
    otaRefresh();
  });
  lv_obj_set_width(mode, 210);

  // Hoe lang de wifi aan blijft zonder gebruik
  static const uint8_t IDLE_MIN[] = {5, 10, 30, 60};
  int idleSel = 1;
  for (int i = 0; i < 4; i++)
    if (IDLE_MIN[i] == wifi_idle_min()) idleSel = i;
  card = otaIdleCard = makeSettingRow(col, T("Uit na"), T("Zonder gebruik van de pagina"));
  lv_obj_t *idle = makeDropdown(card, "5 min\n10 min\n30 min\n60 min", idleSel, [](lv_event_t *e) {
    uint8_t m = IDLE_MIN[lv_dropdown_get_selected(lv_event_get_target(e))];
    wifi_set_idle_min(m);
    char t[48];
    snprintf(t, sizeof(t), T("Stopt zelf na %u min zonder gebruik"), m);
    lv_label_set_text(lv_obj_get_child(otaSwitchCard, 1), t);
  });
  lv_obj_set_width(idle, 210);

  lv_obj_t *row = lv_obj_create(col);
  lv_obj_set_size(row, W - 32, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 16, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  otaLabel = makeLabel(row, &lv_font_montserrat_16, COL_TEXT, "");
  lv_label_set_long_mode(otaLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(otaLabel, W - 32);

  otaQr = lv_qrcode_create(row, 160, lv_color_black(), lv_color_white());
  lv_obj_set_style_border_color(otaQr, lv_color_white(), 0);
  lv_obj_set_style_border_width(otaQr, 6, 0);  // witte rand: nodig om te kunnen scannen
  otaQrText[0] = 0;

  otaWifiBtn = makeButton(col, T(LV_SYMBOL_WIFI "  Wifi-netwerk instellen"), COL_HEADER,
                          [](lv_event_t *) { buildWifiSet(); });
  lv_obj_set_size(otaWifiBtn, 300, 48);

  otaRefresh();
  showScreen(scr, Page::Ota);
}

void buildWifiSet() {
  lv_obj_t *scr = makeScreen();
  makeHeader(scr, T("Wifi-netwerk"), [](lv_event_t *) { buildOta(); });

  char ssid[40] = "", pass[65] = "";  // WPA: tot 63 tekens
  wifi_credentials(ssid, sizeof(ssid), pass, sizeof(pass));

  editForm = lv_obj_create(scr);
  lv_obj_set_size(editForm, W, 480 - HEADER_H);
  lv_obj_set_pos(editForm, 0, HEADER_H);
  lv_obj_set_style_bg_opa(editForm, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(editForm, 0, 0);
  lv_obj_set_style_pad_all(editForm, 16, 0);
  lv_obj_set_style_pad_row(editForm, 12, 0);
  lv_obj_set_flex_flow(editForm, LV_FLEX_FLOW_COLUMN);

  makeLabel(editForm, &lv_font_montserrat_14, COL_MUTED, T("Het board verbindt alleen als je de wifi-pagina aanzet."));
  taName = makeField(editForm, T("Netwerknaam (SSID)"), W - 32, 31, T("bv. Boot-router"), TA_TEXT);
  lv_textarea_set_text(taName, ssid);
  taKey = makeField(editForm, T("Wachtwoord"), W - 32, 63, T("wachtwoord"), TA_TEXT);
  lv_textarea_set_text(taKey, pass);

  lv_obj_t *row = makeFormRow(editForm);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *save = makeButton(row, T(LV_SYMBOL_OK "  Opslaan"), COL_ACCENT, [](lv_event_t *) {
    const char *s = lv_textarea_get_text(taName);
    if (!s[0]) {
      lv_label_set_text(editError, T("Vul de netwerknaam in"));
      return;
    }
    wifi_save_credentials(s, lv_textarea_get_text(taKey));
    buildOta();
  });
  lv_obj_set_size(save, 160, 44);
  editError = makeLabel(row, &lv_font_montserrat_14, COL_ERR);
  lv_obj_set_flex_grow(editError, 1);

  keyboard = lv_keyboard_create(scr);
  lv_obj_set_size(keyboard, W, 200);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(keyboard, nullptr);
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(keyboard, hideKeyboard, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(keyboard, hideKeyboard, LV_EVENT_CANCEL, nullptr);

  showScreen(scr, Page::WifiSet);
}

// Bevestiging na een upload: installeren of afwijzen, met een tijdslimiet
void otaPopupClose() {
  if (otaPopup) lv_obj_del_async(otaPopup);
  otaPopup = nullptr;
}

lv_obj_t *otaPopupMsg;
void openAlarmPopup(lv_event_t *e);

void otaPopupOpen() {
  if (otaPopup) return;
  display_wake();  // het scherm kan uit zijn als de upload klaar is
  otaPopup = lv_obj_create(lv_layer_top());
  lv_obj_set_size(otaPopup, W, 480);
  lv_obj_set_style_bg_color(otaPopup, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(otaPopup, LV_OPA_70, 0);
  lv_obj_set_style_border_width(otaPopup, 0, 0);
  lv_obj_set_style_radius(otaPopup, 0, 0);
  lv_obj_clear_flag(otaPopup, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *panel = lv_obj_create(otaPopup);
  lv_obj_set_size(panel, W - 40, 260);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, COL_HEADER, 0);
  lv_obj_set_style_border_color(panel, COL_ACCENT, 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 14, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = makeLabel(panel, &lv_font_montserrat_20, COL_TEXT, T("Nieuwe firmware installeren?"));
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t *msg = otaPopupMsg = makeLabel(panel, &lv_font_montserrat_14, COL_MUTED,
                            T("Het board herstart; dat duurt ongeveer 20 s. Meldingen gaan daarna weer door.\n"
                            "Zonder antwoord binnen 60 s wordt het afgewezen."));
  lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(msg, W - 80);
  lv_obj_align(msg, LV_ALIGN_TOP_LEFT, 0, 40);

  lv_obj_t *yes = makeButton(panel, T(LV_SYMBOL_OK "  Installeer"), COL_ACCENT, [](lv_event_t *) {
    if (alarms_unacked() > 0) {  // niet herstarten met een alarm dat nog niet gezien is
      lv_label_set_text(otaPopupMsg, T("Er is een alarm dat nog niet bevestigd is. Tik in de meldingen op Stil, "
                                     "sluit ze, en tik daarna hier op Installeer."));
      lv_obj_set_style_text_color(otaPopupMsg, COL_ERR, 0);
      openAlarmPopup(nullptr);  // komt boven deze vraag te staan
      return;
    }
    ota_confirm(true);
  });
  lv_obj_set_size(yes, 180, 48);
  lv_obj_align(yes, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t *no = makeButton(panel, T("Afwijzen"), COL_CARD, [](lv_event_t *) { ota_confirm(false); });
  lv_obj_set_size(no, 180, 48);
  lv_obj_align(no, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

// Elke seconde: popup tonen zodra er een upload klaarstaat, en weghalen als het beslist is
void otaTickUi() {
  static uint32_t lastRefresh = 0;
  bool staged = ota_phase() == OtaPhase::Staged;
  if (staged) otaPopupOpen();
  else otaPopupClose();
  if (page == Page::Ota && millis() - lastRefresh > 500) {
    lastRefresh = millis();
    otaRefresh();
  }
}

// ================= Vraag: lokale tijd van de plotter volgen? =================

lv_obj_t *clockPopup;

void clockPopupAnswer(ClockSource src) {
  clock_set_source(src);
  if (clockPopup) lv_obj_del_async(clockPopup);
  clockPopup = nullptr;
  if (page == Page::Settings) buildSettings();
}

// Eén keer vragen, zodra het netwerk een andere tijd geeft dan de eigen tijd (niet tijdens de taalkeuze)
void clockOfferTick() {
  static uint32_t last = 0;
  if (clockPopup || page == Page::Language || page == Page::ClockSet || millis() - last < 1000) return;
  last = millis();
  if (!clock_network_offer()) return;
  uint32_t now = clock_now();
  int32_t net = clock_network_offset_min();
  time_t tn = (time_t)now + net * 60;
  struct tm a, b;
  gmtime_r(&tn, &a);
  clock_local(now, b);
  char zone[24], msg[400];
  utcText(zone, sizeof(zone), net);
  snprintf(msg, sizeof(msg),
           T("Het NMEA 2000-netwerk (meestal de kaartplotter) geeft als lokale tijd %02d:%02d (%s). Bosun "
             "toont nu %02d:%02d.\nDe tijd van de plotter volgen? Dat kun je later wijzigen bij Instellingen."),
           a.tm_hour, a.tm_min, zone, b.tm_hour, b.tm_min);

  display_wake();
  clockPopup = lv_obj_create(lv_layer_top());
  lv_obj_set_size(clockPopup, W, 480);
  lv_obj_set_style_bg_color(clockPopup, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(clockPopup, LV_OPA_70, 0);
  lv_obj_set_style_border_width(clockPopup, 0, 0);
  lv_obj_set_style_radius(clockPopup, 0, 0);
  lv_obj_clear_flag(clockPopup, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *panel = lv_obj_create(clockPopup);
  lv_obj_set_size(panel, W - 40, 280);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, COL_HEADER, 0);
  lv_obj_set_style_border_color(panel, COL_ACCENT, 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 14, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = makeLabel(panel, &lv_font_montserrat_20, COL_TEXT, T("Tijd van de plotter"));
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t *m = makeLabel(panel, &lv_font_montserrat_14, COL_MUTED, msg);
  lv_label_set_long_mode(m, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(m, W - 80);
  lv_obj_align(m, LV_ALIGN_TOP_LEFT, 0, 40);

  lv_obj_t *yes = makeButton(panel, T(LV_SYMBOL_OK "  Volgen"), COL_ACCENT,
                             [](lv_event_t *) { clockPopupAnswer(ClockSource::Network); });
  lv_obj_set_size(yes, 180, 48);
  lv_obj_align(yes, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t *no = makeButton(panel, T("Eigen tijd"), COL_CARD, [](lv_event_t *) { clockPopupAnswer(ClockSource::Own); });
  lv_obj_set_size(no, 180, 48);
  lv_obj_align(no, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

// ================= Alarmbalk en alarmlijst =================

lv_obj_t *banner, *bannerLbl, *frame, *alarmPopup, *alarmList, *ackBtn;
uint32_t popupRefresh;

void closeAlarmPopup(lv_event_t * = nullptr) {
  if (alarmPopup) lv_obj_del_async(alarmPopup);
  alarmPopup = nullptr;
}

void fillAlarmList() {
  lv_obj_clean(alarmList);
  static Alarm *al = psram_alloc<Alarm>(MAX_ALARMS);  // niet op de kleine stack van de lus, niet in het interne RAM
  if (!al) return;
  int n = alarms_active(al, MAX_ALARMS);
  if (n == 0) {
    makeLabel(alarmList, &lv_font_montserrat_16, COL_OK, T(LV_SYMBOL_OK "  Geen meldingen"));
  }
  for (int i = 0; i < n; i++) {
    bool red = al[i].level == AlarmLevel::Alarm;
    lv_obj_t *l = makeLabel(alarmList, &lv_font_montserrat_16, red ? COL_ERR : COL_WARN, "");
    lv_label_set_text_fmt(l, "%s  %s%s", red ? LV_SYMBOL_BELL : LV_SYMBOL_WARNING, al[i].text,
                          red && al[i].acked ? T("  (stil)") : "");
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(100));
  }
  if (alarms_unacked()) lv_obj_clear_flag(ackBtn, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(ackBtn, LV_OBJ_FLAG_HIDDEN);
}

void openAlarmPopup(lv_event_t * = nullptr) {
  if (alarmPopup) return;
  // Donkere achtergrond die het scherm eronder blokkeert
  alarmPopup = lv_obj_create(lv_layer_top());
  lv_obj_set_size(alarmPopup, W, 480);
  lv_obj_set_style_bg_color(alarmPopup, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(alarmPopup, LV_OPA_70, 0);
  lv_obj_set_style_border_width(alarmPopup, 0, 0);
  lv_obj_set_style_radius(alarmPopup, 0, 0);
  lv_obj_clear_flag(alarmPopup, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *panel = lv_obj_create(alarmPopup);
  lv_obj_set_size(panel, W - 40, 360);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, COL_HEADER, 0);
  lv_obj_set_style_border_color(panel, levelColor(alarms_highest()), 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 14, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = makeLabel(panel, &lv_font_montserrat_20, COL_TEXT, T(LV_SYMBOL_BELL "  Meldingen"));
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  alarmList = lv_obj_create(panel);
  lv_obj_set_size(alarmList, lv_pct(100), 230);
  lv_obj_align(alarmList, LV_ALIGN_TOP_LEFT, 0, 36);
  lv_obj_set_style_bg_opa(alarmList, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(alarmList, 0, 0);
  lv_obj_set_style_pad_all(alarmList, 0, 0);
  lv_obj_set_style_pad_row(alarmList, 10, 0);
  lv_obj_set_flex_flow(alarmList, LV_FLEX_FLOW_COLUMN);

  ackBtn = makeButton(panel, T(LV_SYMBOL_MUTE "  Stil"), COL_ERR, [](lv_event_t *) {
    alarms_ack_all();
    fillAlarmList();
  });
  lv_obj_set_size(ackBtn, 180, 48);
  lv_obj_align(ackBtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  lv_obj_t *close = makeButton(panel, T("Sluiten"), COL_CARD, closeAlarmPopup);
  lv_obj_set_size(close, 180, 48);
  lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  fillAlarmList();
  popupRefresh = millis();
}

// Rand om het hele scherm in de kleur van de zwaarste melding
void buildFrame() {
  frame = lv_obj_create(lv_layer_top());
  lv_obj_set_size(frame, W, 480);
  lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(frame, 5, 0);
  lv_obj_set_style_radius(frame, 0, 0);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(frame, LV_OBJ_FLAG_HIDDEN);
}

void buildBanner() {
  // Rode pil in de kopbalk, op de plek van de titel: knoppen blijven bereikbaar, inhoud blijft zichtbaar
  banner = lv_obj_create(lv_layer_top());
  lv_obj_set_height(banner, 40);
  lv_obj_set_style_radius(banner, 20, 0);
  lv_obj_set_style_border_width(banner, 0, 0);
  lv_obj_set_style_pad_hor(banner, 12, 0);
  lv_obj_set_style_pad_ver(banner, 0, 0);
  lv_obj_clear_flag(banner, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(banner, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(banner, openAlarmPopup, LV_EVENT_CLICKED, nullptr);
  bannerLbl = makeLabel(banner, &lv_font_montserrat_14, COL_TEXT, "");
  lv_label_set_long_mode(bannerLbl, LV_LABEL_LONG_DOT);
  lv_obj_set_width(bannerLbl, lv_pct(100));
  lv_obj_align(bannerLbl, LV_ALIGN_LEFT_MID, 0, 0);
}

void updateBanner() {
  static Alarm *al = psram_alloc<Alarm>(MAX_ALARMS);  // niet op de kleine stack van de lus, niet in het interne RAM
  if (!al) return;
  int n = alarms_active(al, MAX_ALARMS);  // rood eerst
  AlarmLevel top = alarms_highest();
  int unacked = alarms_unacked();
  // Onbevestigd rood knippert; bevestigd rood is rustig rood; alleen waarschuwingen = oranje
  bool blink = unacked && (millis() / 600) % 2;
  lv_color_t col = top == AlarmLevel::Alarm ? (blink ? lv_color_hex(theme().errDim) : COL_ERR) : COL_WARN;

  if (n == 0) {
    lv_obj_add_flag(frame, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_border_color(frame, col, 0);
  }

  // Pil niet over het bewerkscherm heen; daar zit je in een invoerveld
  if (n == 0 || page == Page::Edit || page == Page::N2kLimits) {
    lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(banner, LV_OBJ_FLAG_HIDDEN);
    // Dashboard heeft geen terugknop; rechts ruimte voor tandwiel en +
    int x = page == Page::Dashboard ? 8 : 56;
    lv_obj_set_pos(banner, x, 8);
    bool clockShown = page == Page::Dashboard && clock_valid();
    lv_obj_set_width(banner, W - 136 - x - (clockShown ? 72 : 0));
    lv_obj_set_style_bg_color(banner, col, 0);
    lv_obj_set_style_text_color(bannerLbl, lv_color_hex(theme_text_on(lv_color_to32(col) & 0xFFFFFF)), 0);
    lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
    const char *icon = top == AlarmLevel::Alarm ? LV_SYMBOL_BELL : LV_SYMBOL_WARNING;
    if (n == 1) lv_label_set_text_fmt(bannerLbl, "%s  %s", icon, al[0].text);
    else lv_label_set_text_fmt(bannerLbl, "%s  %s  (+%d)", icon, al[0].text, n - 1);
  }
  if (alarmPopup && millis() - popupRefresh > 1000) {
    popupRefresh = millis();
    fillAlarmList();
  }
}

// ================= Periodieke update =================

void tick(lv_timer_t *) {
  themeTick();
  switch (page) {
    case Page::Dashboard: updateDashboard(); break;
    case Page::Scan: updateScan(); break;
    case Page::Detail: updateDetail(); break;
    default: break;
  }
}

void alarmTick(lv_timer_t *) {
  alarms_tick();
  history_tick();
  n2k_tick();
  ota_set_alarm_active(alarms_unacked() > 0);
  ota_tick();
  otaTickUi();
  clockOfferTick();
  baro_tick();
  sd_tick();
  updateBanner();
}

}  // namespace

// ---------- Opstartscherm ----------
// Kompasster met de naam binnen een dunne ring, in de kleuren van het thema. Over de ring draait een gloeiende
// stip met een boogje erachter (voortgang). Staat er vanaf ui_splash() (na het scherm en de klok) tot ui_begin(),
// en minstens SPLASH_MIN_MS zodat het niet alleen even knippert.
namespace {
constexpr uint32_t SPLASH_MIN_MS = 1500;
constexpr int RING_R = 176;     // straal van de ring (midden van de lijn)
constexpr int TAIL_DEG = 120;   // lengte van het boogje achter de stip
constexpr int HEAD_START = 320; // beginstand van de stip: rechtsboven (0° = rechts, met de klok mee)
constexpr uint32_t SPIN_MS = 3000;  // één rondje
lv_obj_t *splashScr = nullptr, *splashBody = nullptr, *splashStar = nullptr, *splashTail[2];
uint32_t splashSince = 0;

lv_obj_t *splashImg(lv_obj_t *parent, const lv_img_dsc_t *src, lv_color_t color, int y) {
  lv_obj_t *img = lv_img_create(parent);
  lv_img_set_src(img, src);
  lv_obj_set_style_img_recolor(img, color, 0);
  lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_align(img, LV_ALIGN_CENTER, 0, y);
  return img;
}

// Boog over de ring: ring = de hele cirkel (achtergrond), anders het boogje achter de stip
lv_obj_t *splashArc(lv_obj_t *parent, int width, lv_opa_t opa, bool ring) {
  lv_obj_t *a = lv_arc_create(parent);
  lv_obj_remove_style_all(a);
  lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(a, 2 * RING_R + width, 2 * RING_R + width);
  lv_obj_center(a);
  lv_obj_set_style_arc_color(a, COL_ACCENT, ring ? LV_PART_MAIN : LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(a, width, ring ? LV_PART_MAIN : LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(a, opa, ring ? LV_PART_MAIN : LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, ring ? LV_PART_INDICATOR : LV_PART_MAIN);  // het andere deel niet tekenen
  lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
  lv_arc_set_bg_angles(a, 0, 360);
  return a;
}

void splashSpin(void *, int32_t v) {
  int head = (HEAD_START + v) % 360;
  // Alleen de hoeken verzetten: LVGL tekent dan alleen het stukje dat verandert opnieuw
  for (lv_obj_t *t : splashTail) lv_arc_set_angles(t, (head + 360 - TAIL_DEG) % 360, head);
  float r = head * (float)M_PI / 180.0f;
  lv_coord_t half = glow_star.header.w / 2;
  lv_obj_set_pos(splashStar, 240 + lroundf(cosf(r) * RING_R) - half, 240 + lroundf(sinf(r) * RING_R) - half);
}

void splashOpa(void *obj, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)obj, v, 0); }

void splashFade(lv_obj_t *obj, int32_t from, int32_t to, uint32_t ms) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_exec_cb(&a, splashOpa);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_time(&a, ms);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_start(&a);
}
}  // namespace

void buildSplash() {
  const Palette &p = theme();
  splashScr = makeScreen();
  splashBody = lv_obj_create(splashScr);
  lv_obj_remove_style_all(splashBody);
  lv_obj_set_size(splashBody, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(splashBody, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_opa(splashBody, LV_OPA_TRANSP, 0);

  constexpr int MARK_Y = -70, WORD_Y = 18, SUB_Y = 66;
  splashArc(splashBody, 1, LV_OPA_30, true);
  splashTail[0] = splashArc(splashBody, 8, p.dark ? LV_OPA_20 : LV_OPA_TRANSP, false);  // gloed om het boogje
  splashTail[1] = splashArc(splashBody, 2, LV_OPA_COVER, false);
  if (p.dark) {  // zachte gloed: alleen op een donkere achtergrond
    splashImg(splashBody, &splash_mark_glow, COL_ACCENT, MARK_Y);
    splashImg(splashBody, &splash_word_glow, COL_TEXT, WORD_Y);
  }
  splashImg(splashBody, &splash_mark, COL_ACCENT, MARK_Y);
  splashImg(splashBody, &splash_word, COL_TEXT, WORD_Y);
  splashImg(splashBody, &splash_sub, COL_MUTED, SUB_Y);
  lv_obj_t *ver = makeLabel(splashBody, &bm_font_14, COL_MUTED, "v" BOSUN_VERSION);
  lv_obj_set_style_text_letter_space(ver, 2, 0);
  lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -20);

  // Gloeiende stip, zoals de stipjes op het overzicht
  splashStar = lv_img_create(splashBody);
  lv_img_set_src(splashStar, &glow_star);
  lv_obj_set_style_img_recolor(splashStar, p.dark ? COL_TEXT : COL_ACCENT, 0);
  lv_obj_set_style_img_recolor_opa(splashStar, LV_OPA_COVER, 0);
  if (p.glowStar) {
    lv_obj_t *core = splashImg(splashStar, &glow_core, lv_color_white(), 0);
    lv_obj_center(core);
  }
  splashSpin(nullptr, 0);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, splashStar);  // verdwijnt samen met de stip
  lv_anim_set_exec_cb(&a, splashSpin);
  lv_anim_set_values(&a, 0, 359);
  lv_anim_set_time(&a, SPIN_MS);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);

  lv_scr_load(splashScr);
  splashFade(splashBody, LV_OPA_TRANSP, LV_OPA_COVER, 700);
  splashSince = millis();
  lv_timer_handler();
}

void ui_splash() {
  loadThemePos();
  applyTheme(wantedTheme());
  buildSplash();
}

// Tussen de stappen van het opstarten: het opstartscherm laten bewegen (en de verlichting infaden)
void ui_splash_pump() {
  if (splashScr) lv_timer_handler();
}

// Opstartscherm netjes laten verdwijnen; daarna bouwt ui_begin het eerste echte scherm
void splashEnd() {
  if (!splashScr) return;
  while (millis() - splashSince < SPLASH_MIN_MS) {
    lv_timer_handler();
    delay(5);
  }
  lv_anim_del(splashBody, splashOpa);
  splashFade(splashBody, lv_obj_get_style_opa(splashBody, 0), LV_OPA_TRANSP, 250);
  uint32_t t = millis();
  while (millis() - t < 260) {
    lv_timer_handler();
    delay(5);
  }
  splashScr = splashBody = splashStar = nullptr;  // het volgende showScreen ruimt het scherm op
}

static lv_timer_t *alarmTimer = nullptr;  // pauzeerbaar voor de screenshot-build

void ui_begin() {
  splashEnd();
  loadThemePos();
  applyTheme(wantedTheme());
  if (i18n_chosen()) buildDashboard();
  else buildLanguage(true);  // eerste start: eerst de taal kiezen (tot dan Engels)
  buildFrame();
  buildBanner();
  lv_timer_create(tick, 1000, nullptr);
  alarmTimer = lv_timer_create(alarmTick, 100, nullptr);
}

#ifdef UI_SCREENSHOT_HOOKS
// Alleen voor de screenshot-build op de pc
void ui_debug_show(int what, int arg) {
  switch (what) {
    case 1: buildSettings(); break;
    case 20: applyTheme((ThemeId)arg); buildDashboard(); break;  // thema wisselen
    case 21: applyTheme((ThemeId)arg); break;
    case 22: buildDashboard(); break;
    case 2: openAlarmPopup(); break;
    case 3:
      buildEdit(false, arg, nullptr);  // toetsenbord blijft verborgen, zodat het hele formulier te zien is
      break;
    case 4: buildDetail(arg); break;
    case 6: buildClockSet(); break;
    case 7: buildN2kLimits(); break;
    case 8: buildOta(); break;
    case 9: buildWifiSet(); break;
    case 10: closeAlarmPopup(); break;
    case 11: buildDashboard(); break;
    case 12: buildScan(); break;
    case 13: buildLanguage(arg != 0); break;
    case 30:  // opstartscherm in thema arg (zonder balk en melding: die bestaan bij het opstarten nog niet)
      lv_obj_add_flag(frame, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(banner, LV_OBJ_FLAG_HIDDEN);
      lv_timer_pause(alarmTimer);
      applyTheme((ThemeId)arg);
      buildSplash();
      break;  // opstartscherm in thema arg
    case 5:  // historie: arg = weergave * 2 + (7 dagen ? 1 : 0)
      histView = (HistView)(arg / 2);
      histLong = arg % 2;
      buildDashboard();
      break;
  }
}
#endif
