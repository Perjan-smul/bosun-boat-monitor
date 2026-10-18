#pragma once
#include <stddef.h>
#include <stdint.h>

// Apparaten uitschakelen die het board laten crashen.
// Vlak voordat een Bluetooth-pakket of NMEA 2000-bericht verwerkt wordt, onthoudt het board (in geheugen
// dat een herstart overleeft) van wie het is. Crasht of hangt het board twee keer terwijl het hetzelfde
// apparaat verwerkt, dan wordt dat apparaat genegeerd tot je het weer inschakelt. Zo blijft de rest werken.

enum class QKind : uint8_t { None, Ble, N2k };

struct QEntry {
  QKind kind;
  char mac[18];   // Bluetooth: MAC-adres
  uint32_t pgn;   // NMEA 2000: berichtnummer
  uint8_t src;    // NMEA 2000: adres van het apparaat op de bus
};

constexpr int Q_MAX = 8;
constexpr uint8_t Q_STRIKES = 2;  // na zoveel crashes met hetzelfde apparaat: uitschakelen

// Bij het opstarten, na log_begin(); crashed = de vorige start eindigde in een crash of vastloper.
// Geeft true als er nu een apparaat is uitgeschakeld.
bool quarantine_begin(bool crashed);

// Rond het verwerken van één pakket/bericht (per taak één tegelijk)
void quarantine_enter_ble(const char *mac);
void quarantine_leave_ble();
void quarantine_enter_n2k(uint32_t pgn, uint8_t src);
void quarantine_leave_n2k();

bool quarantine_ble_blocked(const char *mac);
bool quarantine_n2k_blocked(uint32_t pgn, uint8_t src);

int quarantine_list(QEntry *out, int max);  // uitgeschakelde apparaten
void quarantine_release(int index);         // weer inschakelen
void quarantine_describe(const QEntry &e, char *out, size_t len);  // bv. "Huishoudaccu (c0:3b:..)"
