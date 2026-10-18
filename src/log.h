#pragma once
#include <stddef.h>
#include <stdint.h>

// Logboek: naar de seriële poort én naar een ringbuffer die op de diagnosepagina staat.
// De buffer staat in RTC-geheugen: na een crash of watchdog-herstart is het logboek van vóór
// de herstart er nog (na stroomuitval niet).
void log_begin();                         // direct na Serial.begin()
void logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));  // regel(s) met \n
void log_line(const char *text);          // één regel, \n wordt toegevoegd
size_t log_copy(char *out, size_t max);   // hele logboek, oudste eerst; geeft de lengte
// Nieuw stuk logboek ophalen (voor de kopie op de SD-kaart): vanaf pos, hooguit max bytes; pos schuift op.
// Is pos al overschreven, dan begint het bij het oudste dat er nog is.
size_t log_read(uint32_t &pos, char *out, size_t max);
uint32_t log_position();    // positie van het eind (alles tot hier is geschreven)
uint32_t log_boot_start();  // positie waar deze start begon (ervoor: het logboek van vóór de herstart)
const char *log_reset_reason();           // waarom het board de laatste keer (her)startte
uint32_t log_boot_count();                // aantal starts sinds de eerste installatie

// Herstartlus herkennen: aantal mislukte starts (crash of waakhond) direct na elkaar.
// Na een tijd goed draaien zet log_running_ok() de telling terug op 0.
uint32_t log_failures();
void log_running_ok();

// Fasen van het opstarten: bij een crash weet het herstelscherm waar het misging
enum : uint8_t {
  STAGE_START, STAGE_SETTINGS, STAGE_OTA, STAGE_STORE, STAGE_DISPLAY, STAGE_CLOCK, STAGE_HISTORY,
  STAGE_UI, STAGE_BLE, STAGE_N2K, STAGE_RUNNING, STAGE_SD, STAGE_COUNT
};
void log_stage(uint8_t stage);
uint8_t log_fail_stage();               // fase waarin de vorige start misging
uint8_t log_fail_kind();                // 0 = geen, 1 = crash, 2 = waakhond
const char *log_stage_name(uint8_t stage);
void log_error_code(char *out, size_t len);  // bv. "C6": crash tijdens de historie
