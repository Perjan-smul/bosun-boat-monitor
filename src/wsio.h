#pragma once
#include <stdint.h>

// Hulpchip van de Waveshare ESP32-S3-Touch-LCD-4: resets van scherm en touch, achtergrondverlichting
// en zoemer. Versie V4.0 heeft een CH32V003 (0x24), V3.0 een TCA9554 (0x20); welke er zit, wordt bij
// het opstarten gevonden. Alleen gebruikt op dat board.
enum class WsIo : uint8_t { None, Ch32, Tca9554 };

WsIo wsio_begin();                 // na Wire.begin(): chip zoeken, systeemvoeding aan, scherm en touch resetten
WsIo wsio_type();
// Scherm en touch opnieuw resetten, low_ms lang (ook de systeemvoeding even uit, zoals bij het starten).
// Voor een touchchip die na een eerdere start hangt; alleen vóór het starten van het scherm aanroepen.
void wsio_reset_panel(uint16_t low_ms);
void wsio_backlight(uint8_t level);  // 0-255 (TCA9554: alleen aan of uit)
void wsio_buzzer(bool on);
