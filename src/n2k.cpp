#include "log.h"
#include "quarantine.h"
#include "n2k.h"
#include <Arduino.h>
#include "board.h"
#include "clock.h"
#include "settings.h"

#if defined(CAN_TX_PIN)
#include <driver/gpio.h>
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
// Diagnose van de bus: ook wat de decoder niet ziet (11-bits berichten, fouten, de RX-lijn zelf)
volatile uint32_t rawFrames = 0, stdFrames = 0;  // alle ontvangen berichten / daarvan niet-NMEA 2000
volatile uint32_t rxSamples = 0, rxLow = 0;      // RX-lijn bemonsterd in de ontvangsttaak
uint32_t rawPerSec = 0, stdPerSec = 0, errPerSec = 0, lastRaw = 0, lastStd = 0, lastErr = 0;
uint8_t rxLowPct = 0;
uint32_t busErrors = 0, rxMissed = 0, rxErrCounter = 0;
uint8_t busState = 0;
uint32_t startMs = 0;
bool reported = false;  // eenmalige melding in het logboek na de eerste 10 s

struct Lock {
  Lock() { xSemaphoreTake(lockHandle(), portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(lockHandle()); }
};

#if defined(CAN_TX_PIN)
void rxTask(void *) {
  twai_message_t m;
  for (;;) {
    if (!running) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    // RX van de CAN-chip bemonsteren: rust (recessief) = hoog, steeds laag = iets mis met de bus of de chip
    rxSamples = rxSamples + 1;
    if (!gpio_get_level((gpio_num_t)CAN_RX_PIN)) rxLow = rxLow + 1;
    if (twai_receive(&m, pdMS_TO_TICKS(10)) != ESP_OK) continue;
    rawFrames = rawFrames + 1;
    if (!m.extd || m.rtr) {  // NMEA 2000 gebruikt alleen 29-bits id's
      stdFrames = stdFrames + 1;
      continue;
    }
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
  if (on) {
    rawFrames = stdFrames = 0;
    lastRaw = lastStd = lastErr = 0;  // de tellers van de CAN-module beginnen ook opnieuw
    running = start();
    startMs = millis();
    reported = false;
  } else {
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
#if defined(CAN_TX_PIN)
      uint32_t dt = max<uint32_t>(1, now - lastStatMs);
      uint32_t raw = rawFrames, oth = stdFrames;
      rawPerSec = (raw - lastRaw) * 1000 / dt;
      stdPerSec = (oth - lastStd) * 1000 / dt;
      lastRaw = raw;
      lastStd = oth;
      twai_status_info_t si;
      if (running && twai_get_status_info(&si) == ESP_OK) {
        busErrors = si.bus_error_count;
        rxMissed = si.rx_missed_count + si.rx_overrun_count;
        rxErrCounter = si.rx_error_counter;
        busState = (uint8_t)si.state;
        errPerSec = (busErrors - lastErr) * 1000 / dt;
        lastErr = busErrors;
      } else {
        errPerSec = 0;
      }
      uint32_t ns = rxSamples, nl = rxLow;
      rxSamples = 0;
      rxLow = 0;
      rxLowPct = ns ? (uint8_t)(nl * 100 / ns) : 0;
#endif
      lastStatMs = now;
    }
    gpsEpoch = decoder.state.gpsEpoch;
    gpsTimeMs = decoder.state.gpsTimeMs;
    offsetMs = decoder.state.localOffsetMs;
    offsetMin = decoder.state.localOffsetMin;
  }
  // Eenmalig na 10 s: wat de CAN-module ziet, zodat een stille bus in het logboek te verklaren is
  if (running && !reported && now - startMs > 10000) {
    reported = true;
    logf("[n2k] na 10 s: %lu berichten (%lu niet-NMEA 2000), busfouten %lu, RX-fouten %lu, gemist %lu, "
         "RX-lijn %u%% laag, toestand %u\n",
         (unsigned long)rawFrames, (unsigned long)stdFrames, (unsigned long)busErrors, (unsigned long)rxErrCounter,
         (unsigned long)rxMissed, rxLowPct, busState);
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
  N2kStats st{};
  st.running = running;
  st.framesPerSec = fps;
  st.sources = decoder.state.sources;
  st.lastClockSync = lastSync;
  st.rawPerSec = rawPerSec;
  st.otherPerSec = stdPerSec;
  st.errorsPerSec = errPerSec;
  st.busErrors = busErrors;
  st.rxErrors = rxErrCounter;
  st.rxMissed = rxMissed;
  st.rxLowPct = rxLowPct;
  return st;
}

N2kDiag n2k_diag(const N2kStats &st) {
  if (!st.running) return N2kDiag::Off;
  if (st.framesPerSec) return N2kDiag::Ok;
  if (st.rxLowPct >= 90) return N2kDiag::RxStuckLow;
  if (st.errorsPerSec) return N2kDiag::Errors;
  if (st.otherPerSec) return N2kDiag::NotN2k;
  return N2kDiag::Silent;
}
