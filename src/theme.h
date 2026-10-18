#pragma once
#include <stdint.h>

// Kleurthema's van het scherm. De kleuren staan als 0xRRGGBB; de UI vraagt ze op via theme().
enum class ThemeId : uint8_t { Ocean, Elegant, Light, Night, Count };

struct Palette {
  uint32_t bg, header, card, line;     // achtergrond, kopbalk, kaarten, lijnen
  uint32_t accent;                     // knoppen, titels, schakelaars
  uint32_t text, muted;                // tekst, gedempte tekst
  uint32_t ink;                        // tekst op een gekleurd vlak (labels, gekozen item)
  uint32_t ok, warn, err, errDim;      // status; errDim = knipperstand van een rood alarm
  uint32_t sun, load, battery, shore;  // energiestroom op het overzicht
  uint32_t drop, rain;                 // weerregel: druppel, regenwolk
  uint32_t tank[4];                    // tankbalken: diesel, water, grijs water, vuilwater
  uint32_t hist[6];                    // lijnen in de historie
  bool dark;                           // donker thema (voor de LVGL-standaardstijl)
  bool glowStar;                       // stipjes als gloeiende ster met witte kern; anders een zacht stipje
  uint8_t maxBrightness;               // achtergrondverlichting 0-255
};

const Palette &theme();
ThemeId theme_active();
void theme_set(ThemeId id);
const char *theme_name(ThemeId id);  // Nederlandse sleutel, tonen met T()

// Tekstkleur die goed leesbaar is op een vlak in deze kleur
uint32_t theme_text_on(uint32_t fill);

// Zonnestand in graden boven de horizon (negatief = onder), voor epoch (UTC) op lat/lon
float sun_altitude(uint32_t epoch, double lat, double lon);
// Donker buiten? Met positie: zon meer dan 4° onder de horizon (tussen zonsondergang en burgerlijke schemering).
// Zonder positie: vaste tijden, 21:00 tot 7:00 in de eigen tijd (localMinutes = minuten na middernacht).
bool theme_is_dark_outside(uint32_t epoch, bool havePos, double lat, double lon, int localMinutes);
