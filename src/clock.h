#pragma once
#include <stdint.h>
#include <time.h>

// Klok: de PCF85063-RTC op het board (I2C) + systeemtijd van de ESP32.
// De RTC bewaart UTC; tonen gebeurt in Nederlandse tijd (zomer-/wintertijd automatisch).
void clock_begin();          // na Wire.begin()
bool clock_valid();          // tijd bekend?
uint32_t clock_now();        // UTC epoch in seconden, 0 = onbekend
bool clock_local(uint32_t epoch, struct tm &out);  // epoch naar lokale tijd
bool clock_rtc_present();
bool clock_rtc_lost_power();  // RTC meldt dat de knoopcel leeg was: opnieuw instellen
// Tijd instellen in lokale tijd (maand 1-12); zet ook de RTC als die er is
void clock_set_local(int year, int month, int day, int hour, int minute);
// Tijd instellen in UTC (bv. vanuit GPS); zet ook de RTC als die er is
void clock_set_utc(uint32_t epoch);
// Tijd van de GPS (UTC, precies). Daarna past "Instellen" alleen het eigen verschil aan, niet de UTC-tijd.
void clock_set_gps(uint32_t utc);
int32_t clock_offset_min();  // eigen verschil met Midden-Europese tijd in minuten (0 = Midden-Europese tijd)
int32_t clock_utc_offset_min(uint32_t epoch);  // verschil van de getoonde tijd met UTC op dat moment

// Lokale tijd van het NMEA 2000-netwerk (PGN 129033 "Local Offset", meestal van de kaartplotter).
// Niet elk systeem stuurt het, en het klopt alleen als het op de plotter goed staat; daarom kiest de
// schipper één keer: de plotter volgen of de eigen tijd houden.
enum class ClockSource : uint8_t { Ask, Network, Own };  // Ask = nog niet gekozen (eigen tijd tot dan)
void clock_network_offset(int32_t offsetMin);  // ontvangen verschil met UTC (al gecontroleerd en stabiel)
bool clock_network_known();                    // ooit ontvangen (bewaard over herstarts)
int32_t clock_network_offset_min();
ClockSource clock_source();
void clock_set_source(ClockSource src);
// Vraag tonen? Netwerk stuurt deze start een verschil, er is nog niet gekozen, en het geeft een andere tijd
bool clock_network_offer();
