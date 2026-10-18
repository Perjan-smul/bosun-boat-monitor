#include "log.h"
#include "quarantine.h"
#include "n2k.h"
#include <Arduino.h>
#include "board.h"
#include "clock.h"
#include "settings.h"

#if defined(CAN_TX_PIN)
#include <driver/twai.h>
#endif

namespace {

N2kDecoder decoder;
// Statisch aangemaakt bij het eerste gebruik: zo werkt het slot ook als iemand de gegevens
// opvraagt voordat de module gestart is (de volgorde in setup() maakt dan niet uit).
SemaphoreHandle_t lockHandle() {
  static StaticSemaphore_t buf;
  static SemaphoreHandle_t h = xSemaphoreCreateMutexStatic(&buf);
  return h;
}
bool running = false;
uint32_t fps = 0, lastFrames = 0, lastStatMs = 0, lastSync = 0;

struct Lock {
  Lock() { xSemaphoreTake(lockHandle(), portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(lockHandle()); }
};

#if defined(CAN_TX_PIN)
void rxTask(void *) {
  twai_message_t m;
  for (;;) {
    if (!running || twai_receive(&m, pdMS_TO_TICKS(200)) != ESP_OK) {
      if (!running) vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    if (!m.extd || m.rtr) continue;  // NMEA 2000 gebruikt alleen 29-bits id's
    uint32_t pgn = n2kPgnFromId(m.identifier);
    uint8_t src = m.identifier & 0xFF;
    if (quarantine_n2k_blocked(pgn, src)) continue;  // uitgeschakeld na crashes
    quarantine_enter_n2k(pgn, src);
    {
      Lock l;
      decoder.onFrame(m.identifier, m.data, m.data_length_code, millis());
    }
    quarantine_leave_n2k();
  }
}

bool start() {
  // Alleen luisteren: geen ACK, geen adresclaim, nooit zenden
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_LISTEN_ONLY);
  g.rx_queue_len = 64;
  twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();  // NMEA 2000
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
  if (twai_start() != ESP_OK) {
    twai_driver_uninstall();
    return false;
  }
  return true;
}

void stop() {
  twai_stop();
  twai_driver_uninstall();
}
#endif

}  // namespace

bool n2k_available() {
#if defined(CAN_TX_PIN)
  return true;
#else
  return false;
#endif
}

void n2k_begin() {
#if defined(CAN_TX_PIN)
  xTaskCreatePinnedToCore(rxTask, "n2k", 4096, nullptr, 5, nullptr, 0);
  n2k_set_enabled(settings().n2k);
#endif
}

void n2k_set_enabled(bool on) {
#if defined(CAN_TX_PIN)
  if (on == running) return;
  if (on) running = start();
  else {
    running = false;
    vTaskDelay(pdMS_TO_TICKS(250));  // ontvangsttaak laten stoppen
    stop();
    Lock l;
    decoder.reset();
  }
  logf("[n2k] %s\n", running ? "luistert op 250 kbit/s" : "uit");
#else
  (void)on;
#endif
}

void n2k_get(N2kState &out) {
  Lock l;
  out = decoder.state;
}

void n2k_tick() {
  uint32_t now = millis();
  uint32_t gpsEpoch, gpsTimeMs, offsetMs;
  int16_t offsetMin;
  {
    Lock l;
    if (now - lastStatMs >= 1000) {
      fps = (decoder.state.frames - lastFrames) * 1000 / max<uint32_t>(1, now - lastStatMs);
      lastFrames = decoder.state.frames;
      lastStatMs = now;
    }
    gpsEpoch = decoder.state.gpsEpoch;
    gpsTimeMs = decoder.state.gpsTimeMs;
    offsetMs = decoder.state.localOffsetMs;
    offsetMin = decoder.state.localOffsetMin;
  }
  if (!settings().gpsClock) return;  // "Klok via GPS" uit: niets van het netwerk overnemen
  // Lokale tijd van de plotter doorgeven; de klok beslist (keuze van de schipper) of hij gebruikt wordt
  if (offsetMs && now - offsetMs < 10000) clock_network_offset(offsetMin);
  // Klok gelijkzetten met GPS: direct als de klok onbekend is, anders hooguit eens per uur bij > 2 s verschil
  if (!gpsTimeMs || now - gpsTimeMs > 3000 || gpsEpoch < 1735689600) return;
  uint32_t gps = gpsEpoch + (now - gpsTimeMs) / 1000;
  int32_t diff = (int32_t)(gps - clock_now());
  bool due = !clock_valid() || (abs(diff) > 2 && (!lastSync || now - lastSync > 3600000UL));
  if (due) {
    clock_set_gps(gps);
    lastSync = now;
    logf("[n2k] klok gelijkgezet met GPS (verschil %ld s)\n", (long)diff);
  }
}

N2kStats n2k_stats() {
  Lock l;
  return {running, fps, decoder.state.sources, lastSync};
}
