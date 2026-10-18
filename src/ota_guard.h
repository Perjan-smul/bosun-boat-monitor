#pragma once
#include <stdint.h>

// Bewaking van een nieuwe firmware. Hardware-onafhankelijk, zodat het op de pc te testen is.
// De bootloader van de ESP32-S3 doet zelf ook een terugval, maar alleen als het programma nooit
// "geldig" wordt gemarkeerd. Deze laag vangt ook het geval op dat het programma wel start maar
// steeds opnieuw crasht.
struct OtaRecord {
  bool pending;   // een nieuwe firmware is klaargezet en nog niet bewezen goed
  uint8_t tries;  // aantal starts sinds de update, zonder dat het programma gezond werd
};

enum class BootAction : uint8_t {
  Normal,       // geen update in behandeling
  WaitHealthy,  // nieuwe firmware start: wachten tot hij gezond is
  Revert,       // te vaak mislukt: terug naar de vorige versie
};

constexpr uint8_t OTA_MAX_TRIES = 2;  // na dit aantal mislukte starts volgt de terugval

BootAction otaOnBoot(OtaRecord &r);  // bij elke start, als eerste
void otaStage(OtaRecord &r);         // nieuwe firmware staat klaar
void otaMarkHealthy(OtaRecord &r);   // na een geslaagde start, of na een afwijzing door de gebruiker
