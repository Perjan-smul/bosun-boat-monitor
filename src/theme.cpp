#include "theme.h"
#include <math.h>
#include "i18n.h"

namespace {

// Oceaan: donker blauwzwart met een groenblauw accent (het oorspronkelijke ontwerp)
const Palette OCEAN = {
    0x07090D, 0x0E131A, 0x141B24, 0x26313D,
    0x2EC4B6,
    0xE8F1F8, 0x8A97A6,
    0x07090D,
    0x3DDC84, 0xF4A261, 0xE63946, 0x9E2A33,
    0xF4B400, 0x3DDC84, 0x4FA3F7, 0xEF5350,
    0x4FC3F7, 0x4FA3F7,
    {0xF4A261, 0x4FA3F7, 0x9AA5B1, 0x8D6E63},
    {0x2EC4B6, 0xF4B400, 0x4FA3F7, 0xEF5350, 0xB388FF, 0x3DDC84},
    true, true, 255,
};

// Elegant: echt zwart met blauwe accenten; ook de energiestroom in blauwtinten
const Palette ELEGANT = {
    0x000000, 0x0B0C10, 0x13151A, 0x252A33,
    0x4C8DFF,
    0xF4F6FA, 0x8C94A3,
    0x000000,
    0x34C77B, 0xF0A04B, 0xE5484D, 0x8E2A2E,
    0xBFDBFF, 0x4C8DFF, 0x7AB8FF, 0x8C7CFF,  // zon ijsblauw, verbruik blauw, accu hemelsblauw, wal blauwpaars
    0x7AB8FF, 0x4C8DFF,
    {0x9CC3FF, 0x4C8DFF, 0x8C94A3, 0x6F7FB0},
    {0x4C8DFF, 0xBFDBFF, 0x7AB8FF, 0x8C7CFF, 0x2E5FD1, 0x9AA5B8},
    true, true, 255,
};

// Licht: voor overdag in de zon; donkerder kleuren voor contrast op wit
const Palette LIGHT = {
    0xEEF2F6, 0xFFFFFF, 0xFFFFFF, 0xC9D2DC,
    0x0B63CE,
    0x15202B, 0x5E6B78,
    0xFFFFFF,
    0x1F9D55, 0xC96A12, 0xCF2E2E, 0xF0A7A7,
    0xC98F00, 0x1F9D55, 0x1E73E8, 0xD9443F,
    0x1A8FD6, 0x1E73E8,
    {0xC96A12, 0x1E73E8, 0x7A8794, 0x7B5A4E},
    {0x0B63CE, 0xC98F00, 0x1A8FD6, 0xD9443F, 0x7E57C2, 0x1F9D55},
    false, false, 255,
};

// Nacht: alles in gedempt rood op zwart (spaart het nachtzicht); rood alarm blijft fel
const Palette NIGHT = {
    0x000000, 0x070000, 0x110303, 0x2A0909,
    0xA32020,
    0xC83C3C, 0x7A2424,
    0x000000,
    0x8E2B2B, 0xC0502A, 0xFF2A2A, 0x6A1010,
    0xA33A2A, 0x8E2B2B, 0xB23A3A, 0xC44040,
    0x9A2E2E, 0x9A2E2E,
    {0xA33A2A, 0x8E2B2B, 0x6A2020, 0x5A1A1A},
    {0xC83C3C, 0x8E2B2B, 0xA33A2A, 0x6A2020, 0xB23A3A, 0x7A2424},
    true, false, 70,
};

ThemeId active = ThemeId::Ocean;

double rad(double d) { return d * M_PI / 180.0; }
double deg(double r) { return r * 180.0 / M_PI; }

}  // namespace

const Palette &theme() {
  switch (active) {
    case ThemeId::Elegant: return ELEGANT;
    case ThemeId::Light: return LIGHT;
    case ThemeId::Night: return NIGHT;
    default: return OCEAN;
  }
}

ThemeId theme_active() { return active; }
void theme_set(ThemeId id) { active = id < ThemeId::Count ? id : ThemeId::Ocean; }

const char *theme_name(ThemeId id) {
  switch (id) {
    case ThemeId::Elegant: return TN("Elegant");
    case ThemeId::Light: return TN("Licht");
    case ThemeId::Night: return TN("Nacht");
    default: return TN("Oceaan");
  }
}

namespace {
double lin(double c) { return c <= 0.03928 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
double luminance(uint32_t c) {
  return 0.2126 * lin((c >> 16 & 0xFF) / 255.0) + 0.7152 * lin((c >> 8 & 0xFF) / 255.0) + 0.0722 * lin((c & 0xFF) / 255.0);
}
}  // namespace

uint32_t theme_text_on(uint32_t fill) {
  const Palette &p = theme();
  double L = luminance(fill);
  // Nacht: zwarte tekst op rode vlakken (rode tekst op rood is onleesbaar, wit verblindt); op donker de rode tekst
  if (active == ThemeId::Night) return L > 0.03 ? 0x000000 : p.text;
  // Lichte tekst zolang die genoeg contrast geeft (kleurvlakken ogen zo rustiger), anders donkere
  uint32_t light = p.dark ? p.text : 0xFFFFFF, darkInk = p.dark ? p.bg : p.text;
  double contrastLight = (luminance(light) + 0.05) / (L + 0.05);
  return contrastLight >= 2.0 ? light : darkInk;
}

// Zonnestand volgens de benadering van de US Naval Observatory (nauwkeurig tot ± 1°, ruim genoeg om te wisselen)
float sun_altitude(uint32_t epoch, double lat, double lon) {
  double d = epoch / 86400.0 - 10957.5;  // dagen sinds 1 jan 2000 12:00 UTC
  double g = rad(fmod(357.529 + 0.98560028 * d, 360.0));
  double q = fmod(280.459 + 0.98564736 * d, 360.0);
  double L = rad(q + 1.915 * sin(g) + 0.020 * sin(2 * g));
  double e = rad(23.439 - 0.00000036 * d);
  double ra = atan2(cos(e) * sin(L), cos(L));
  double dec = asin(sin(e) * sin(L));
  double gmst = fmod(18.697374558 + 24.06570982441908 * d, 24.0);
  double ha = rad(gmst * 15.0 + lon) - ra;
  double alt = asin(sin(rad(lat)) * sin(dec) + cos(rad(lat)) * cos(dec) * cos(ha));
  return (float)deg(alt);
}

bool theme_is_dark_outside(uint32_t epoch, bool havePos, double lat, double lon, int localMinutes) {
  if (havePos) return sun_altitude(epoch, lat, lon) < -4.0f;
  return localMinutes >= 21 * 60 || localMinutes < 7 * 60;
}
