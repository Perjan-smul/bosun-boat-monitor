#include "log.h"
#include "store.h"
#include "psram.h"
#include <Preferences.h>
#include <math.h>
#include <algorithm>
using std::min;
#include <freertos/semphr.h>

namespace {

constexpr uint8_t STORE_VERSION = 1;
// Vaste kop van elk record (mac, naam, type, sleutel); alles daarna zijn instellingen met een standaardwaarde
constexpr size_t RECORD_HEAD = 18 + 24 + 1 + 1 + 16;
constexpr uint32_t DISCOVERED_TIMEOUT_MS = 60000;

// Statisch aangemaakt bij het eerste gebruik: zo werkt het slot ook als iemand de gegevens
// opvraagt voordat de module gestart is (de volgorde in setup() maakt dan niet uit).
SemaphoreHandle_t lockHandle() {
  static StaticSemaphore_t buf;
  static SemaphoreHandle_t h = xSemaphoreCreateMutexStatic(&buf);
  return h;
}
// Apparatenlijst in PSRAM (24 x ± 0,6 KB); bij het eerste gebruik aangemaakt, ook als iemand vóór store_begin vraagt
DeviceState *dev() {
  static DeviceState *p = psram_alloc<DeviceState>(MAX_DEVICES);
  return p;
}
int deviceCount = 0;
uint32_t version = 1;
Discovered discovered[MAX_DISCOVERED];
int discoveredCount = 0;

struct Lock {
  Lock() { xSemaphoreTake(lockHandle(), portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(lockHandle()); }
};

void save() {
  static DeviceConfig cfgs[MAX_DEVICES];  // ± 2,8 KB: niet op de stack (altijd onder het slot aangeroepen)
  for (int i = 0; i < deviceCount; i++) cfgs[i] = dev()[i].cfg;
  Preferences prefs;
  prefs.begin("bootmon", false);
  prefs.putUChar("ver", STORE_VERSION);
  prefs.putUShort("rsz", sizeof(DeviceConfig));  // recordgrootte: zo leest elke andere versie de lijst ook
  prefs.putBytes("devs", cfgs, deviceCount * sizeof(DeviceConfig));
  prefs.end();
}

int findDevice(const char *mac) {
  for (int i = 0; i < deviceCount; i++)
    if (strcmp(dev()[i].cfg.mac, mac) == 0) return i;
  return -1;
}

int findDiscovered(const char *mac) {
  for (int i = 0; i < discoveredCount; i++)
    if (strcmp(discovered[i].mac, mac) == 0) return i;
  return -1;
}

void removeDiscovered(int i) {
  memmove(&discovered[i], &discovered[i + 1], (discoveredCount - i - 1) * sizeof(Discovered));
  discoveredCount--;
}

}  // namespace

void deviceConfigDefaults(DeviceConfig &cfg) {
  cfg.socLow = cfg.voltLow = cfg.tempHigh = cfg.tempLow = NAN;
  cfg.socWarn = cfg.voltWarn = cfg.tempHighWarn = cfg.tempLowWarn = NAN;
  cfg.tankFullMm = cfg.tankEmptyMm = NAN;
  cfg.tankWarn = 20;
  cfg.tankAlarm = 10;
  cfg.battWarn = 25;
  cfg.battAlarm = 10;
  cfg.tankMedium = (uint8_t)TankMedium::Gas;  // standaard: gasfles (Pro Check)
  if (isShunt(cfg.type)) {
    cfg.socWarn = 40;
    cfg.socLow = 25;
  }
}

void store_begin() {
  deviceCount = 0;
  Preferences prefs;
  prefs.begin("bootmon", true);
  // Nieuwe velden komen er altijd achteraan bij, en de recordgrootte wordt mee opgeslagen. Zo leest elke
  // versie ook de opslag van een andere: kortere records (ouder) krijgen standaardwaarden voor de nieuwe
  // velden, bij langere (nieuwer, bv. na een terugval van een update) vallen de onbekende velden weg.
  size_t rec = prefs.getUShort("rsz", 0);
  if (rec < RECORD_HEAD || rec > 1024) rec = 0;
  if (rec) {
    size_t len = prefs.getBytesLength("devs");
    uint8_t *raw = (uint8_t *)malloc(len ? len : 1);
    if (raw) len = prefs.getBytes("devs", raw, len);
    else len = 0;
    deviceCount = min<int>(len / rec, MAX_DEVICES);
    for (int i = 0; i < deviceCount; i++) {
      memset(&dev()[i], 0, sizeof(DeviceState));
      DeviceConfig &c = dev()[i].cfg;
      memcpy(&c, raw + i * rec, RECORD_HEAD);  // eerst het type: de standaardwaarden hangen ervan af
      deviceConfigDefaults(c);
      memcpy(&c, raw + i * rec, min(rec, sizeof(DeviceConfig)));
    }
    free(raw);
  }
  prefs.end();
  if (rec && rec < sizeof(DeviceConfig)) save();  // oudere indeling bijwerken; een nieuwere niet overschrijven
  logf("[store] %d apparaten geladen\n", deviceCount);
}

int store_count() {
  Lock l;
  return deviceCount;
}

uint32_t store_version() {
  Lock l;
  return version;
}

bool store_get(int index, DeviceState &out) {
  Lock l;
  if (index < 0 || index >= deviceCount) return false;
  out = dev()[index];
  return true;
}

bool store_add(const DeviceConfig &cfg) {
  Lock l;
  if (deviceCount >= MAX_DEVICES || findDevice(cfg.mac) >= 0) return false;
  memset(&dev()[deviceCount], 0, sizeof(DeviceState));
  dev()[deviceCount].cfg = cfg;
  deviceCount++;
  int d = findDiscovered(cfg.mac);
  if (d >= 0) removeDiscovered(d);
  version++;
  save();
  return true;
}

bool store_update(int index, const DeviceConfig &cfg) {
  Lock l;
  if (index < 0 || index >= deviceCount) return false;
  bool keyChanged = cfg.hasKey != dev()[index].cfg.hasKey ||
                    memcmp(cfg.key, dev()[index].cfg.key, sizeof(cfg.key)) != 0;
  dev()[index].cfg = cfg;
  if (keyChanged) {
    dev()[index].hasData = false;
    dev()[index].last = ParseResult::None;
  }
  version++;
  save();
  return true;
}

bool store_set_name_key(const char *mac, const char *name, const uint8_t *key) {
  Lock l;
  int i = findDevice(mac);
  if (i < 0) return false;
  DeviceConfig &c = dev()[i].cfg;
  if (name && name[0]) strlcpy(c.name, name, sizeof(c.name));
  if (key && (!c.hasKey || memcmp(c.key, key, sizeof(c.key)) != 0)) {
    memcpy(c.key, key, sizeof(c.key));
    c.hasKey = true;
    dev()[i].hasData = false;  // oude metingen hoorden bij de vorige sleutel
    dev()[i].last = ParseResult::None;
  }
  version++;
  save();
  return true;
}

void store_remove(int index) {
  Lock l;
  if (index < 0 || index >= deviceCount) return;
  memmove(&dev()[index], &dev()[index + 1], (deviceCount - index - 1) * sizeof(DeviceState));
  deviceCount--;
  version++;
  save();
}

int store_discovered(Discovered *out, int max) {
  Lock l;
  uint32_t now = millis();
  for (int i = discoveredCount - 1; i >= 0; i--)
    if (now - discovered[i].lastSeenMs > DISCOVERED_TIMEOUT_MS) removeDiscovered(i);
  int n = min(max, discoveredCount);
  memcpy(out, discovered, n * sizeof(Discovered));
  return n;
}

bool store_find_discovered(const char *mac, Discovered &out) {
  Lock l;
  int i = findDiscovered(mac);
  if (i < 0) return false;
  out = discovered[i];
  return true;
}

void store_on_advert(const char *mac, const char *name, int rssi, const AdvData &adv) {
  SensorType type = detectType(adv);
  Lock l;
  int i = findDevice(mac);
  if (i >= 0) {
    DeviceState &d = dev()[i];
    d.rssi = rssi;
    d.lastHeardMs = millis() | 1;
    if (type == SensorType::Unknown) return;  // bv. scan response met alleen de naam
    Reading r;
    ParseResult res = parseReading(type, adv, d.cfg.hasKey ? d.cfg.key : nullptr, r, d.cfg.tankMedium);
    d.last = res;
    if (res == ParseResult::Ok) {
      d.reading = r;
      d.hasData = true;
      // Alleen een geldige meting telt als "vers". Zendt een apparaat wel maar klopt de sleutel
      // niet meer, dan veroudert de meting en volgt de melding "geen signaal".
      d.lastSeenMs = millis() | 1;
    }
    return;
  }

  int k = findDiscovered(mac);
  if (k < 0) {
    if (type == SensorType::Unknown) return;
    if (discoveredCount >= MAX_DISCOVERED) {  // oudste eruit
      int oldest = 0;
      for (int j = 1; j < discoveredCount; j++)
        if (discovered[j].lastSeenMs < discovered[oldest].lastSeenMs) oldest = j;
      removeDiscovered(oldest);
    }
    k = discoveredCount++;
    memset(&discovered[k], 0, sizeof(Discovered));
    strlcpy(discovered[k].mac, mac, sizeof(discovered[k].mac));
  }
  Discovered &d = discovered[k];
  if (type != SensorType::Unknown) d.type = type;
  if (name && name[0]) strlcpy(d.name, name, sizeof(d.name));
  d.rssi = rssi;
  d.lastSeenMs = millis();
}
