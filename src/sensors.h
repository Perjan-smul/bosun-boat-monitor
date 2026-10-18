#pragma once
#include <Arduino.h>

// Ondersteunde BLE-sensoren
enum class SensorType : uint8_t {
  Unknown = 0,
  VictronBattery,  // SmartShunt, BMV, SmartBatterySense
  VictronSolar,    // SmartSolar MPPT
  VictronDcDc,     // Orion Smart
  VictronOther,    // overige Victron-producten (alleen signaal)
  Ruuvi,           // RuuviTag, dataformaat 5
  AtcThermo,       // Xiaomi LYWSD03MMC met ATC1441- of pvvx-firmware
  BTHome,          // BTHome v2 (onversleuteld)
  VictronAcCharger,  // Blue Smart IP22/IP65, Skylla (walstroomlader); achteraan i.v.m. opgeslagen NVS-waarden
  VictronLithium,    // Smart LiFePO4 met BMS: celspanningen, accuspanning, balancer
  MopekaPro,         // Mopeka Pro Check (NRF52): tankniveau via ultrasoon, bottom-up op de tankwand
  VictronBatteryProtect,  // Smart BatteryProtect: schakelt de verbruikers uit bij lage accuspanning
  VictronLynx,            // Lynx Smart BMS: accu met SoC, stroom en temperatuur (als shunt)
  ThermoBeacon,           // ThermoBeacon-thermometers (ORIA, Brifit, SensorBlue): temperatuur en vocht
  VictronInverter,        // Phoenix-omvormer (Smart / VE.Direct met Bluetooth)
  VictronVeBus,           // MultiPlus / Quattro via VE.Bus (o.a. met de VE.Bus Smart dongle)
  VictronMultiRs,         // Multi RS (Solar): omvormer/lader met MPPT
  VictronOrionXs,         // Orion XS DC-DC-lader
  VictronDcMeter,         // SmartShunt als DC-energiemeter (bv. voor een lader of verbruiker)
};
// Victron-omvormer/laders die walstroom naar de accu kunnen brengen (telt als "Wal" op het overzicht)
inline bool isVictronMulti(SensorType t) { return t == SensorType::VictronVeBus || t == SensorType::VictronMultiRs; }
// Accu-meters met stroom en laadtoestand (SmartShunt, BMV, Lynx); niet de Battery Sense of Smart Lithium
inline bool isShunt(SensorType t) { return t == SensorType::VictronBattery || t == SensorType::VictronLynx; }

// Ruwe advertentiedata van één BLE-pakket
struct AdvData {
  bool mopekaSvc = false;           // advertentie met service-uuid 0xFEE5 (Mopeka Pro)
  bool thermoBeaconSvc = false;     // advertentie met service-uuid 0xFFF0 (ThermoBeacon)
  const uint8_t *mfg = nullptr;     size_t mfgLen = 0;     // manufacturer data (incl. company id)
  const uint8_t *svc181a = nullptr; size_t svc181aLen = 0; // service data UUID 0x181A
  const uint8_t *bthome = nullptr;  size_t bthomeLen = 0;  // service data UUID 0xFCD2
};

constexpr int MAX_VALUES = 8;

struct Value {
  char label[20];
  char text[20];
};

// Ruwe getallen voor overzichten en berekeningen; NAN = niet beschikbaar
struct Metrics {
  float volts;     // accuspanning (of uitgangsspanning bij DC-DC)
  float amps;      // accustroom: + laden, - ontladen (bij MPPT: laadstroom)
  float watts;     // volts * amps
  float soc;       // laadtoestand in %
  float pvWatts;   // zonnepaneelvermogen
  float yieldKwh;  // opbrengst vandaag
  float loadAmps;  // load-uitgang MPPT
  float tempC;
  float humidity;
  int32_t ttgMin;  // resterende tijd in minuten, -1 = onbekend
  uint8_t state;   // Victron laadstatus, 0xFF = onbekend
  uint16_t alarm;  // Victron alarmbits (accumonitor), 0 = geen
  uint8_t error;   // Victron laderfoutcode, 0 = geen
  float distanceMm;  // Mopeka: afstand tot de vloeistof (NAN = geen goede meting)
  float batteryPct;  // Mopeka: batterij in %
  uint8_t quality;   // Mopeka: meetkwaliteit 0-3 (0 = niet bruikbaar)
  float pressureHpa; // luchtdruk (Ruuvi, BTHome); NAN = niet gemeten
  float acInW;       // MultiPlus/Quattro/Multi RS: vermogen van de AC-ingang (walstroom/generator), NAN = onbekend
  float acOutW;      // idem: AC-uitgang (verbruik aan 230 V)
  uint8_t acIn;      // actieve AC-ingang: 0 = AC1, 1 = AC2, 2 = niet aangesloten, 3 = onbekend
};

// Geformatteerde meetwaarden, klaar om te tonen
struct Reading {
  char primary[20];  // grote waarde op de tegel
  uint8_t count;
  Value values[MAX_VALUES];
  Metrics m;
  float cellV[8];    // Smart Lithium: celspanningen, NAN = onbekend
};

enum class ParseResult : uint8_t { None, Ok, NeedKey, BadKey, Unsupported };

// Inhoud van een tank met een Mopeka-sensor (opgeslagen als getal: volgorde niet wijzigen)
enum class TankMedium : uint8_t { Gas, FreshWater, WasteWater, Fuel, Count };
const char *tankMediumName(TankMedium m);  // Nederlandse sleutel, tonen met T()
// Vuilwater: melding als de tank te vol wordt; de rest: als hij bijna leeg is
inline bool tankAlarmWhenFull(uint8_t medium) { return medium == (uint8_t)TankMedium::WasteWater; }

SensorType detectType(const AdvData &adv);
bool isVictron(SensorType t);
// Victron-sleutel uit tekst: 32 hex-tekens; spaties, '-' en ':' (bv. van kopiëren) worden genegeerd
bool parseVictronKey(const char *s, uint8_t out[16]);
const char *typeName(SensorType t);
// Tekst van een meetwaarde in de huidige taal, voor het scherm en de webpagina.
// Labels (values[i].label), typeName() en de Victron-namen zijn Nederlandse sleutels: tonen met T().
void reading_value_text(const Reading &r, int i, char *out, size_t len);
const char *victronStateName(uint8_t state);
const char *victronAlarmName(uint16_t alarmBits);  // eerste actieve alarmreden
const char *victronErrorName(uint8_t code);
// Mopeka: niveau in % tussen "leeg" en "vol" (afstanden in mm, zoals de sensor ze meet)
float mopekaLevelPct(float distanceMm, float fullMm, float emptyMm);
// medium: alleen voor Mopeka (TankMedium), voor de geluidssnelheid
ParseResult parseReading(SensorType t, const AdvData &adv, const uint8_t *key, Reading &out, uint8_t medium = 0);
