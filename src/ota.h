#pragma once
#include <stdint.h>

// Firmware-update via wifi: het board draait een kleine webpagina op het lokale netwerk.
// Uploaden gaat met een pairingcode die op het scherm staat. Na de upload bevestig je op het scherm.

enum class OtaPhase : uint8_t { Off, Connecting, Serving, Uploading, Staged, Rejected };

void ota_boot_check();   // als allereerste in setup(): kan terugvallen naar de vorige versie
void ota_begin();        // na de instellingen
void ota_tick();         // vanuit de UI-lus (± 100 ms)

void ota_start();        // wifi aan en webpagina starten
void ota_stop();         // alles uit
void ota_set_alarm_active(bool active);  // uploads en herstart worden geweigerd bij een alarm (vanuit de UI)
void ota_confirm(bool install);          // antwoord op de bevestiging op het scherm

OtaPhase ota_phase();
const char *ota_reject_reason();  // bij Rejected: waarom
bool ota_alarm_active();           // onbevestigd rood alarm (vanuit de UI doorgegeven)
void ota_request_restart(uint32_t afterMs);  // herstart vanuit de UI-lus, na deze tijd
const char *ota_ap_name();        // naam van het eigen wifipunt als dat aan is, anders ""
bool wifi_ap_mode();              // true = eigen wifipunt (standaard), false = bestaand netwerk
void wifi_set_ap_mode(bool ap);
uint8_t wifi_idle_min();           // wifi uit na zoveel minuten zonder gebruik (standaard 10)
void wifi_set_idle_min(uint8_t minutes);
const char *ota_address();  // "192.168.1.20" of leeg
const char *ota_code();     // pairingcode
bool wifi_credentials(char *ssid, unsigned ssidLen, char *pass, unsigned passLen);
void wifi_save_credentials(const char *ssid, const char *pass);
