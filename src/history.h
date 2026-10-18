#pragma once
#include <stdint.h>

// Meetreeksen in de historie
enum HistSeries : uint8_t {
  H_SOC,     // accu SoC (%)
  H_BATT_W,  // accuvermogen (W)
  H_PV_W,    // zonnepaneelvermogen (W)
  H_LOAD_W,  // verbruik (W)
  H_TEMP0,   // temperatuur thermometer 1..6 (°C x10)
  H_COUNT = H_TEMP0 + 6,
};

constexpr int HIST_TEMPS = 6;
constexpr int HIST_SHORT_N = 288;  // 24 uur, per 5 minuten
constexpr int HIST_LONG_N = 168;   // 7 dagen, per uur
constexpr int HIST_DAYS_N = 14;    // dagopbrengst zon
constexpr int16_t HIST_NONE = INT16_MIN;
constexpr uint32_t HIST_SHORT_S = 300;

void history_begin();
void history_tick();  // vaak aanroepen; neemt zelf elke 10 s een meting

// Kopieert een reeks in chronologische volgorde (oudste eerst). longRange = 7 dagen.
int history_get(HistSeries s, bool longRange, int16_t *out);
// Thermometer per tempslot (naam leeg = vrij)
const char *history_temp_name(int slot);
// Dagopbrengst (kWh): oudste eerst, de laatste is vandaag. ends (optioneel): moment van de
// dagwissel per dag (UTC epoch, 0 = onbekend); voor vandaag 0.
int history_days(float *out, uint32_t *ends = nullptr);
uint32_t history_version();  // verandert bij elk nieuw meetpunt

struct StorageInfo {
  uint32_t internalFree, psramFree;  // bytes
  uint32_t fsTotal, fsUsed, histBytes;
};
StorageInfo history_storage_info();
