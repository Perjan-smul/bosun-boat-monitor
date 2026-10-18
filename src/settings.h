#pragma once
#include <stdint.h>

// Algemene instellingen, aanpasbaar op het scherm en bewaard in NVS
struct Settings {
  bool buzzer;              // zoemer bij alarm
  bool victronAlarms;       // alarmen die Victron-apparaten zelf melden
  uint8_t noSignalMin;      // alarm als een apparaat zo lang niets stuurt, 0 = uit
  uint16_t screenTimeoutS;  // scherm uit na zoveel seconden, 0 = altijd aan
  bool n2k;                 // NMEA 2000 via CAN uitlezen
  bool gpsClock;            // klok gelijkzetten met de GPS-tijd van NMEA 2000
  // NMEA 2000-alarmen (hoofdschakelaar standaard uit). Grenzen: oranje / rood, NAN = dat alarm uit.
  bool n2kAlarms;
  float depthWarn, depthAlarm;      // diepte onder (m)
  float fuelWarn, fuelAlarm;        // diesel onder (%)
  float waterWarn, waterAlarm;      // drinkwater onder (%)
  float wasteWarn, wasteAlarm;      // grijs/vuilwater boven (%)
  float coolantWarn, coolantAlarm;  // koelwater boven (°C)
  float oilWarn, oilAlarm;          // oliedruk onder (bar), alleen als de motor draait
  // Stormwaarschuwing: luchtdruk in 3 uur zoveel hPa gedaald (oranje / rood), NAN = uit
  float stormWarn, stormAlarm;
  // Klimaatsensor voor temperatuur en vochtigheid op het Overzicht (MAC-adres), "" = automatisch
  char overviewSensor[18];
  // Kleurthema (ThemeId: 0 Oceaan, 1 Elegant, 2 Licht, 3 Nacht). Automatisch: na zonsondergang themeNight.
  uint8_t theme, themeNight;
  bool autoTheme;
  // Nieuwe velden altijd achteraan: oudere opgeslagen instellingen worden dan aangevuld met standaardwaarden
};

void settings_begin();
const Settings &settings();
void settings_save(const Settings &s);
