#include "log.h"
#include "history.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <math.h>
#include "clock.h"
#include "energy.h"
#include "store.h"

namespace {

constexpr uint32_t MAGIC = 0x48495303;  // "HIS" + versie 3 (tijdindex, dagteller)
constexpr uint32_t SAMPLE_MS = 10000;
constexpr uint32_t PARTIAL_SAVE_MS = 60 * 1000UL;  // lopend blok elke minuut bewaren
constexpr int BLOCKS_PER_HOUR = 3600 / HIST_SHORT_S;
constexpr uint32_t MAX_GAP_BLOCKS = HIST_LONG_N * BLOCKS_PER_HOUR + BLOCKS_PER_HOUR;  // > 7 dagen = alles leeg
const char *FILE_PATH = "/history.bin";
const char *PARTIAL_PATH = "/partial.bin";

// Alles wat bewaard wordt; ± 10 KB, staat in PSRAM
struct Data {
  uint32_t magic;
  uint32_t blockIdx;             // tijd / 5 min van het lopende blok
  uint16_t shortHead, longHead;  // volgende schrijfpositie (ringbuffer)
  int32_t hourSum[H_COUNT];      // lopend uur, opgebouwd uit afgesloten 5-min blokken
  uint8_t hourCnt[H_COUNT];
  int16_t shortBuf[H_COUNT][HIST_SHORT_N];
  int16_t longBuf[H_COUNT][HIST_LONG_N];
  char tempMac[HIST_TEMPS][18];
  char tempName[HIST_TEMPS][24];
  float days[HIST_DAYS_N];        // afgeronde dagen, oudste eerst
  uint32_t dayEnd[HIST_DAYS_N];   // moment van de dagwissel (UTC), 0 = onbekend
  uint8_t dayCount;
  float todayMax;      // hoogste opbrengst vandaag tot nu toe
  uint32_t dayNumber;  // telt elke dagwissel, ook als de lijst vol is
};

Data *data = nullptr;
bool fsOk = false;
uint32_t version = 0;
uint32_t fakeBase = 0;  // zonder klok: tijd = fakeBase + uptime
bool wasValid = false;

// Lopend gemiddelde van het huidige 5-minutenblok
float sum[H_COUNT];
uint16_t cnt[H_COUNT];
uint32_t lastSample = 0, lastPartialSave = 0;

// Het lopende, half gevulde 5-minutenblok; elke minuut bewaard zodat een herstart bijna niets kost
struct Partial {
  uint32_t magic;
  uint32_t blockIdx;
  float sum[H_COUNT];
  uint16_t cnt[H_COUNT];
  float todayMax;
  uint32_t dayNumber;  // alleen terugzetten als het nog dezelfde dag is
};

// CRC-32 om een beschadigd bestand te herkennen
uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xFFFFFFFF;
  while (n--) {
    c ^= *p++;
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320 & -(c & 1));
  }
  return ~c;
}

// Veilig schrijven: eerst naar een tijdelijk bestand, dan in één keer hernoemen.
// Valt de stroom tijdens het schrijven weg, dan blijft het vorige bestand gewoon heel.
bool writeAtomic(const char *path, const void *buf, size_t len) {
  if (!fsOk) return false;
  char tmp[24];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  uint32_t crc = crc32((const uint8_t *)buf, len);
  bool ok = f.write((const uint8_t *)buf, len) == len && f.write((const uint8_t *)&crc, 4) == 4;
  f.close();
  return ok && LittleFS.rename(tmp, path);
}

// Lezen met controle op lengte en CRC
bool readOne(const char *path, void *buf, size_t len);
bool readChecked(const char *path, void *buf, size_t len) {
  // Valt de stroom precies tussen schrijven en hernoemen, dan staat de nieuwste versie nog in .tmp
  char tmp[24];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  return readOne(path, buf, len) || readOne(tmp, buf, len);
}

bool readOne(const char *path, void *buf, size_t len) {
  if (!fsOk) return false;
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  uint32_t crc = 0;
  bool ok = f.size() == len + 4 && f.read((uint8_t *)buf, len) == len && f.read((uint8_t *)&crc, 4) == 4;
  f.close();
  return ok && crc == crc32((const uint8_t *)buf, len);
}

uint32_t nowSec() { return clock_valid() ? clock_now() : fakeBase + millis() / 1000; }

int16_t encode(HistSeries s, float v) {
  if (isnan(v)) return HIST_NONE;
  float scaled = s >= H_TEMP0 ? v * 10 : v;  // temperatuur in tienden
  long r = lroundf(scaled);
  return (int16_t)(r < -32000 ? -32000 : r > 32000 ? 32000 : r);
}

void add(HistSeries s, float v) {
  if (isnan(v)) return;
  sum[s] += v;
  cnt[s]++;
}

// Slot voor een thermometer; nieuwe krijgen een vrij slot
int tempSlot(const DeviceState &d) {
  for (int i = 0; i < HIST_TEMPS; i++)
    if (strcmp(data->tempMac[i], d.cfg.mac) == 0) {
      strlcpy(data->tempName[i], d.cfg.name, sizeof(data->tempName[i]));
      return i;
    }
  for (int i = 0; i < HIST_TEMPS; i++)
    if (!data->tempMac[i][0]) {
      strlcpy(data->tempMac[i], d.cfg.mac, sizeof(data->tempMac[i]));
      strlcpy(data->tempName[i], d.cfg.name, sizeof(data->tempName[i]));
      return i;
    }
  return -1;
}

// Thermometers die niet meer gekoppeld zijn vrijgeven, met hun reeksen
void releaseRemovedTemps(const DeviceState *devs, int n) {
  for (int i = 0; i < HIST_TEMPS; i++) {
    if (!data->tempMac[i][0]) continue;
    bool found = false;
    for (int k = 0; k < n; k++) found |= strcmp(devs[k].cfg.mac, data->tempMac[i]) == 0;
    if (found) continue;
    data->tempMac[i][0] = data->tempName[i][0] = 0;
    for (auto &v : data->shortBuf[H_TEMP0 + i]) v = HIST_NONE;
    for (auto &v : data->longBuf[H_TEMP0 + i]) v = HIST_NONE;
  }
}

bool dayChanged = false;

void sample() {
  // Kopie van de apparatenlijst in PSRAM (intern RAM is schaarser)
  static DeviceState *devs = (DeviceState *)ps_malloc(MAX_DEVICES * sizeof(DeviceState));
  if (!devs) return;
  int n = 0;
  while (n < MAX_DEVICES && store_get(n, devs[n])) n++;
  releaseRemovedTemps(devs, n);

  EnergySummary e = energy_summarize(devs, n);
  add(H_SOC, e.soc);
  if (e.haveShunt) add(H_BATT_W, e.battW);
  if (e.haveSolar) add(H_PV_W, e.pv);
  add(H_LOAD_W, e.load);

  for (int i = 0; i < n; i++) {
    const DeviceState &d = devs[i];
    if (!isFresh(d) || isnan(d.reading.m.tempC)) continue;
    if (isShunt(d.cfg.type)) continue;  // alleen echte thermometers
    int slot = tempSlot(d);
    if (slot >= 0) add((HistSeries)(H_TEMP0 + slot), d.reading.m.tempC);
  }

  // Dagwissel: de MPPT zet zijn dagopbrengst op nul. Een flinke daling = nieuwe dag.
  // Met meer laders: een lader die even buiten bereik is, mag geen daling van het totaal geven,
  // en laders die bij zonsopgang een paar minuten na elkaar op nul gaan, geven samen één dag.
  constexpr uint32_t SETTLE_MS = 6UL * 3600 * 1000;  // zo lang na een dagwissel geen nieuwe
  static struct {
    char mac[18];
    float yield;   // vorige waarde
    float atDay;   // waarde bij de laatste dagwissel
    bool reset;    // sinds die dagwissel op nul gegaan
    bool drop;     // in deze meting op nul gegaan
  } ch[MAX_DEVICES];
  static int chN = 0;
  static uint32_t dayMs = 0;
  bool settling = dayMs && millis() - dayMs < SETTLE_MS;
  int solarAll = 0, solarFresh = 0;
  bool chargerReset = false;
  float total = 0;  // dagopbrengst van vandaag: laders die nog op gisteren staan tellen niet mee
  for (int i = 0; i < n; i++) {
    const DeviceState &d = devs[i];
    if (d.cfg.type != SensorType::VictronSolar) continue;
    solarAll++;
    float y = d.reading.m.yieldKwh;
    if (!isFresh(d) || isnan(y)) continue;
    solarFresh++;
    int k = 0;
    while (k < chN && strcmp(ch[k].mac, d.cfg.mac) != 0) k++;
    if (k == chN) {
      if (chN == MAX_DEVICES) continue;
      strlcpy(ch[k].mac, d.cfg.mac, sizeof(ch[k].mac));
      ch[k].yield = y;
      ch[k].atDay = 0;
      ch[k].reset = true;
      chN++;
    }
    bool dropped = ch[k].yield > 0.05f && y < ch[k].yield * 0.5f;
    ch[k].drop = dropped;
    if (dropped) {
      if (settling) ch[k].reset = true;  // hoort bij de dagwissel van zojuist
      else chargerReset = true;
    }
    ch[k].yield = y;
    if (!settling || ch[k].reset || ch[k].atDay <= 0.05f) total += y;
  }
  if (e.haveSolar) {
    bool totalReset = !settling && solarFresh == solarAll && data->todayMax > 0.05f && total < data->todayMax * 0.5f;
    if (totalReset || chargerReset) {
      if (data->dayCount == HIST_DAYS_N) {
        memmove(data->days, data->days + 1, (HIST_DAYS_N - 1) * sizeof(float));
        memmove(data->dayEnd, data->dayEnd + 1, (HIST_DAYS_N - 1) * sizeof(uint32_t));
        data->dayCount--;
      }
      data->days[data->dayCount] = data->todayMax;
      data->dayEnd[data->dayCount++] = clock_now();
      data->todayMax = 0;
      data->dayNumber++;
      dayChanged = true;  // direct opslaan, niet pas aan het eind van het blok
      dayMs = millis() | 1;
      // Elke lader: is hij al op nul gegaan (hoort bij vandaag), of staat hij nog op gisteren?
      // Een lader die nog op gisteren staat, telt mee zodra hij zelf op nul gaat.
      total = 0;
      for (int k = 0; k < chN; k++) {
        ch[k].atDay = ch[k].yield;
        ch[k].reset = ch[k].drop || ch[k].yield <= 0.05f;
      }
      for (int i = 0; i < n; i++) {
        const DeviceState &d = devs[i];
        if (d.cfg.type != SensorType::VictronSolar || !isFresh(d) || isnan(d.reading.m.yieldKwh)) continue;
        for (int k = 0; k < chN; k++)
          if (strcmp(ch[k].mac, d.cfg.mac) == 0 && ch[k].reset) total += ch[k].yield;
      }
    }
    if (solarFresh == solarAll) data->todayMax = fmaxf(data->todayMax, total);
  }
}

// Eén 5-minutenpunt wegschrijven (values = nullptr: gat). Bij een heel uur ook een uurpunt.
void pushBlock(const int16_t *values, uint32_t idx) {
  for (int s = 0; s < H_COUNT; s++) {
    int16_t v = values ? values[s] : HIST_NONE;
    data->shortBuf[s][data->shortHead] = v;
    if (v != HIST_NONE) {
      data->hourSum[s] += v;
      data->hourCnt[s]++;
    }
  }
  data->shortHead = (data->shortHead + 1) % HIST_SHORT_N;
  if ((idx + 1) % BLOCKS_PER_HOUR == 0) {
    for (int s = 0; s < H_COUNT; s++) {
      data->longBuf[s][data->longHead] = data->hourCnt[s] ? data->hourSum[s] / data->hourCnt[s] : HIST_NONE;
      data->hourSum[s] = 0;
      data->hourCnt[s] = 0;
    }
    data->longHead = (data->longHead + 1) % HIST_LONG_N;
  }
}

// Lopend blok afsluiten en doorschuiven naar newIdx; tussenliggende blokken worden gaten
void advanceTo(uint32_t newIdx) {
  int16_t values[H_COUNT];
  for (int s = 0; s < H_COUNT; s++) {
    values[s] = cnt[s] ? encode((HistSeries)s, sum[s] / cnt[s]) : HIST_NONE;
    sum[s] = 0;
    cnt[s] = 0;
  }
  uint32_t idx = data->blockIdx;
  pushBlock(values, idx++);
  uint32_t gaps = newIdx > idx ? min<uint32_t>(newIdx - idx, MAX_GAP_BLOCKS) : 0;
  for (uint32_t i = 0; i < gaps; i++) pushBlock(nullptr, idx++);
  data->blockIdx = newIdx;
  version++;
  writeAtomic(FILE_PATH, data, sizeof(Data));  // na elk 5-minutenblok
}

void savePartial() {
  Partial p = {MAGIC, data->blockIdx, {}, {}, data->todayMax, data->dayNumber};
  memcpy(p.sum, sum, sizeof(sum));
  memcpy(p.cnt, cnt, sizeof(cnt));
  writeAtomic(PARTIAL_PATH, &p, sizeof(p));
}

void reset() {
  memset(data, 0, sizeof(Data));
  data->magic = MAGIC;
  for (auto &ser : data->shortBuf)
    for (auto &v : ser) v = HIST_NONE;
  for (auto &ser : data->longBuf)
    for (auto &v : ser) v = HIST_NONE;
}

}  // namespace

void history_begin() {
  if (!data) data = (Data *)ps_malloc(sizeof(Data));
  if (!data) data = (Data *)malloc(sizeof(Data));
  memset(sum, 0, sizeof(sum));
  memset(cnt, 0, sizeof(cnt));
  reset();
  fakeBase = 0;
  fsOk = LittleFS.begin(true);  // formatteert de eerste keer
  bool loaded = readChecked(FILE_PATH, data, sizeof(Data)) && data->magic == MAGIC;
  if (!loaded) reset();

  // Half gevuld blok van voor de herstart terugzetten
  Partial p;
  if (loaded && readChecked(PARTIAL_PATH, &p, sizeof(p)) && p.magic == MAGIC && p.blockIdx == data->blockIdx) {
    memcpy(sum, p.sum, sizeof(sum));
    memcpy(cnt, p.cnt, sizeof(cnt));
  }
  if (loaded && readChecked(PARTIAL_PATH, &p, sizeof(p)) && p.magic == MAGIC && p.dayNumber == data->dayNumber)
    data->todayMax = fmaxf(data->todayMax, p.todayMax);

  wasValid = clock_valid();
  if (!loaded) {
    data->blockIdx = nowSec() / HIST_SHORT_S;
    writeAtomic(FILE_PATH, data, sizeof(Data));  // meteen aanmaken, zodat een half blok altijd terug kan
  } else if (!wasValid) {
    // Geen klok: hoe lang de stroom weg was is onbekend. Het bewaarde blok afsluiten en één gat markeren.
    fakeBase = (data->blockIdx + 2) * HIST_SHORT_S - millis() / 1000;
  }
  // Met klok: het bewaarde blok loopt door of wordt afgesloten, met een gat van de juiste lengte
  logf("[historie] %u bytes, opslag %s, klok %s, %s\n", (unsigned)sizeof(Data), fsOk ? "ok" : "niet beschikbaar",
                wasValid ? "ja" : "nee", loaded ? "hersteld" : "nieuw");
  lastPartialSave = millis();
}

void history_tick() {
  if (!data) return;
  uint32_t now = millis();
  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    sample();
    if (dayChanged) {
      dayChanged = false;
      writeAtomic(FILE_PATH, data, sizeof(Data));
    }
  }

  // Klok net ingesteld: tijd verspringt. Huidig blok afsluiten met één gat en verder op de echte tijd.
  bool valid = clock_valid();
  if (valid != wasValid) {
    wasValid = valid;
    advanceTo(data->blockIdx + 2);
    data->blockIdx = nowSec() / HIST_SHORT_S;
  }

  uint32_t idx = nowSec() / HIST_SHORT_S;
  if (idx > data->blockIdx) advanceTo(idx);
  else if (idx + 1 < data->blockIdx) data->blockIdx = idx;  // klok teruggezet: gewoon verder vanaf hier

  if (now - lastPartialSave >= PARTIAL_SAVE_MS) {
    lastPartialSave = now;
    savePartial();
  }
}

int history_get(HistSeries s, bool longRange, int16_t *out) {
  if (!data) return 0;
  int n = longRange ? HIST_LONG_N : HIST_SHORT_N;
  const int16_t *buf = longRange ? data->longBuf[s] : data->shortBuf[s];
  int head = longRange ? data->longHead : data->shortHead;
  for (int i = 0; i < n; i++) out[i] = buf[(head + i) % n];
  return n;
}

const char *history_temp_name(int slot) { return data ? data->tempName[slot] : ""; }

int history_days(float *out, uint32_t *ends) {
  if (!data) return 0;
  memcpy(out, data->days, data->dayCount * sizeof(float));
  out[data->dayCount] = data->todayMax;
  if (ends) {
    memcpy(ends, data->dayEnd, data->dayCount * sizeof(uint32_t));
    ends[data->dayCount] = 0;
  }
  return data->dayCount + 1;
}

uint32_t history_version() { return version; }

StorageInfo history_storage_info() {
  StorageInfo i;
  i.internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  i.psramFree = ESP.getFreePsram();
  i.fsTotal = fsOk ? LittleFS.totalBytes() : 0;
  i.fsUsed = fsOk ? LittleFS.usedBytes() : 0;
  i.histBytes = sizeof(Data);
  return i;
}
