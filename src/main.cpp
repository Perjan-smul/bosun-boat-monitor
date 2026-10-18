#include <Arduino.h>
#include <esp_task_wdt.h>
#include <lvgl.h>
#include "log.h"
#include "baro.h"
#include "ble.h"
#include "ota.h"
#include "clock.h"
#include "display.h"
#include "history.h"
#include "i18n.h"
#include "n2k.h"
#include "quarantine.h"
#include "recovery.h"
#include "sdcard.h"
#include "settings.h"
#include "store.h"
#include "ui.h"
#include "version.h"

constexpr uint32_t WATCHDOG_S = 15;         // tijdens gebruik
constexpr uint32_t WATCHDOG_STARTUP_S = 60;  // per fase tijdens het opstarten (bv. opslag voor het eerst klaarmaken)
constexpr uint32_t MAX_FAILURES = 3;         // zoveel mislukte starts na elkaar: herstelscherm
constexpr uint32_t RUNNING_OK_MS = 120000;   // na 2 min goed draaien begint de telling opnieuw
constexpr uint32_t MEM_LOG_MS = 3600000;     // geheugen in het logboek: elk uur

bool recoveryMode = false;

// Volgende fase van het opstarten: onthouden (voor de foutcode) en de waakhond geruststellen
void stage(uint8_t s) {
  log_stage(s);
  feedLoopWDT();
  ui_splash_pump();
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
  // Log via de USB-poort van de ESP32-S3 zelf (Waveshare): zonder computer niet op de log wachten
  Serial.setTxTimeoutMs(0);
#endif
  log_begin();   // meldt ook waarom het board (her)startte
  i18n_begin();  // taal: ook het herstelscherm gebruikt die

  // Waakhond al tijdens het opstarten: blijft het board ergens hangen (bv. een vastgelopen I2C-bus),
  // dan herstart het na 60 s en telt dat als mislukte start. Elke fase zet de tijd opnieuw in.
  esp_task_wdt_init(WATCHDOG_STARTUP_S, true);
  enableLoopWDT();

  // Herstartlus: crashte of liep het board een paar keer direct na elkaar vast, dan heeft gewoon
  // opstarten geen zin. Dan alleen het herstelscherm met foutcode, logboek en wifi-pagina.
  // De controle na een firmware-update (ota_boot_check) wordt dan ook overgeslagen: als die zelf
  // de fout is, zou het board anders nooit bij het herstelscherm komen. Een terugval na een mislukte
  // update gebeurt al eerder (na 2 mislukte starts), dus die gaat hierdoor niet verloren.
  // Crashte het board steeds bij hetzelfde apparaat (Bluetooth of NMEA 2000), dan wordt dat apparaat nu
  // uitgeschakeld. In dat geval nog een gewone start proberen: de rest werkt dan waarschijnlijk wel.
  bool deviceDisabled = quarantine_begin(log_fail_kind() != 0);
  if (log_failures() >= MAX_FAILURES && !deviceDisabled) {
    recoveryMode = true;
    recovery_begin();
    // Hangt het herstelscherm zelf, dan liever een stilstaand scherm met de foutcode dan weer een herstartlus
    disableLoopWDT();
    return;
  }

  ota_boot_check();  // kan terugvallen op de vorige versie
  stage(STAGE_SETTINGS);
  settings_begin();
  stage(STAGE_OTA);
  ota_begin();
  stage(STAGE_STORE);
  store_begin();
  stage(STAGE_DISPLAY);
  display_begin();  // start ook de I2C-bus
  stage(STAGE_CLOCK);
  clock_begin();
  ui_splash();   // opstartscherm in het gekozen thema (dag/nacht volgt uit de klok)
  baro_begin();  // luchtdruksensor op dezelfde I2C-bus, historie uit het geheugen
  stage(STAGE_HISTORY);
  history_begin();
  stage(STAGE_SD);
  sd_begin();  // na het scherm: de kaart deelt pinnen met de init van het scherm
  stage(STAGE_UI);
  ui_begin();
  stage(STAGE_BLE);
  ble_begin();
  stage(STAGE_N2K);
  n2k_begin();
  stage(STAGE_RUNNING);
  // Opstarttijd vanaf het starten van het programma (de bootloader ervoor telt niet mee, ± 0,3 s)
  logf("[start] Bosun %s klaar na %lu ms\n", BOSUN_VERSION, (unsigned long)millis());

  // Waakhond tijdens gebruik: loopt het scherm/de alarmlus meer dan 15 s vast, dan herstart het board.
  // De reden staat daarna in het logboek (diagnosepagina).
  esp_task_wdt_init(WATCHDOG_S, true);
}

void loop() {
  if (recoveryMode) {
    recovery_loop();
    return;
  }
  lv_timer_handler();
  delay(5);
  // Elk uur het geheugen in het logboek: zo is een lek over een lange tocht te zien op de diagnosepagina
  static uint32_t lastMem = 0;
  if (millis() - lastMem >= MEM_LOG_MS) {
    lastMem = millis();
    logf("[geheugen] vrij %lu KB, laagste %lu KB, grootste blok %lu KB, PSRAM vrij %lu KB\n",
         (unsigned long)(ESP.getFreeHeap() / 1024), (unsigned long)(ESP.getMinFreeHeap() / 1024),
         (unsigned long)(ESP.getMaxAllocHeap() / 1024), (unsigned long)(ESP.getFreePsram() / 1024));
  }
  static bool runningOk = false;
  if (!runningOk && millis() > RUNNING_OK_MS) {
    runningOk = true;
    log_running_ok();  // een volgende crash of vastloper telt weer als de eerste
  }
}
