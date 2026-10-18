#pragma once
#include "sensors.h"

constexpr int MAX_DEVICES = 24;
constexpr int MAX_DISCOVERED = 24;

// Wordt in NVS opgeslagen. Nieuwe velden alleen achteraan toevoegen (de recordgrootte gaat mee, zie store_begin)
struct __attribute__((packed)) DeviceConfig {
  char mac[18];  // "aa:bb:cc:dd:ee:ff"
  char name[24];
  SensorType type;
  bool hasKey;
  uint8_t key[16];  // Victron AES-sleutel
  // Alarmgrenzen (rood), NAN = uit
  float socLow;    // accu: SoC onder (%)
  float voltLow;   // accu: spanning onder (V)
  float tempHigh;  // temperatuur boven (°C)
  float tempLow;   // temperatuur onder (°C)
  // Waarschuwingsgrenzen (oranje), NAN = uit. Nieuwe velden altijd achteraan (zie store_begin).
  float socWarn;
  float voltWarn;
  float tempHighWarn;
  float tempLowWarn;
  // Mopeka-kalibratie: afstand (mm) die de sensor meet als de tank vol / leeg is
  float tankFullMm;
  float tankEmptyMm;
  // Mopeka-meldingen: tankniveau onder (%) en batterij onder (%), oranje / rood
  float tankWarn, tankAlarm;
  float battWarn, battAlarm;
  // Mopeka: wat er in de tank zit (TankMedium); bepaalt de geluidssnelheid en de richting van de melding
  uint8_t tankMedium;
};

void deviceConfigDefaults(DeviceConfig &cfg);  // zet alarmgrenzen op standaard voor het type

struct DeviceState {
  DeviceConfig cfg;
  Reading reading;
  ParseResult last;
  bool hasData;
  int rssi;
  uint32_t lastSeenMs;   // laatste geldige meting; 0 = nog nooit
  uint32_t lastHeardMs;  // laatste pakket van welke soort ook (ook met een verkeerde sleutel)
};

struct Discovered {
  char mac[18];
  char name[24];
  SensorType type;
  int rssi;
  uint32_t lastSeenMs;
};

void store_begin();

// Geconfigureerde apparaten (thread-safe, geven kopieën terug)
int store_count();
bool store_get(int index, DeviceState &out);
uint32_t store_version();  // verandert bij toevoegen/wijzigen/verwijderen
bool store_add(const DeviceConfig &cfg);
bool store_update(int index, const DeviceConfig &cfg);
void store_remove(int index);
// Naam en/of sleutel van een apparaat wijzigen op MAC-adres (vanuit de webpagina; name/key = nullptr: laten)
bool store_set_name_key(const char *mac, const char *name, const uint8_t *key);

// Gevonden, nog niet toegevoegde apparaten
int store_discovered(Discovered *out, int max);
bool store_find_discovered(const char *mac, Discovered &out);

// Aangeroepen vanuit de BLE-task voor elk ontvangen pakket
void store_on_advert(const char *mac, const char *name, int rssi, const AdvData &adv);
