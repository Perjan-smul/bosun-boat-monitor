#include "i18n.h"
#include <Preferences.h>
#include <string.h>

// Gegenereerd door scripts/i18n_gen.py (i18n_table.cpp): sleutels gesorteerd op bytes
extern const char *const I18N_KEYS[];
extern const char *const *const I18N_LANGS[];  // per taal (EN, FR, DE, ES, IT, PT, SV, NB, DA, FI): vertaling per sleutel, of nullptr
extern const int I18N_COUNT;

namespace {
Lang current = LANG_DEFAULT;
bool chosen = false;
}  // namespace

void i18n_begin() {
  Preferences p;
  p.begin("ui", true);
  uint8_t v = p.getUChar("lang", 0xFF);
  p.end();
  chosen = v < (uint8_t)Lang::COUNT;
  current = chosen ? (Lang)v : LANG_DEFAULT;
}

bool i18n_chosen() { return chosen; }
Lang i18n_lang() { return current; }

void i18n_set(Lang lang) {
  if ((uint8_t)lang >= (uint8_t)Lang::COUNT) return;
  current = lang;
  chosen = true;
  Preferences p;
  p.begin("ui", false);
  p.putUChar("lang", (uint8_t)lang);
  p.end();
}

const char *i18n_name(Lang lang) {
  static const char *const N[] = {"Nederlands", "English", "Français", "Deutsch", "Español", "Italiano", "Português",
                                  "Svenska", "Norsk", "Dansk", "Suomi"};
  return (uint8_t)lang < (uint8_t)Lang::COUNT ? N[(uint8_t)lang] : "";
}

const char *tr(const char *nl) {
  Lang l = current;
  if (l == Lang::NL || !nl) return nl;
  // Binair zoeken op de Nederlandse tekst
  int lo = 0, hi = I18N_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    int c = strcmp(nl, I18N_KEYS[mid]);
    if (c == 0) {
      const char *t = I18N_LANGS[(int)l - 1][mid];
      return t ? t : nl;
    }
    if (c < 0) hi = mid - 1;
    else lo = mid + 1;
  }
  return nl;
}
