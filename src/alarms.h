#pragma once
#include <stdint.h>

enum class AlarmKind : uint8_t {
  SocLow, VoltLow, TempHigh, TempLow, NoSignal, Victron, ChargerError,
  Depth, TankLow, TankHigh, Coolant, OilPressure,  // NMEA 2000
  MopekaTank, MopekaBatt,                         // Mopeka Pro Check
  Disabled,                                       // apparaat uitgeschakeld na crashes
  Storm                                           // luchtdruk daalt snel
};

// Oranje = waarschuwing (geen zoemer), rood = alarm (zoemer tot "Stil")
enum class AlarmLevel : uint8_t { None, Warning, Alarm };

struct Alarm {
  char mac[18];  // apparaat, of "n2k:..." voor NMEA 2000-meldingen
  AlarmKind kind;
  AlarmLevel level;
  bool acked;  // alleen relevant voor rood: bevestigd = geen zoemer meer
  bool seen;   // deze ronde nog beoordeeld (anders opgeruimd)
  char text[80];
};

constexpr int MAX_ALARMS = 64;

void alarms_tick();                       // vaak aanroepen (bv. elke 100 ms) vanuit de UI
int alarms_active(Alarm *out, int max);   // rood eerst, dan oranje
int alarms_snapshot(Alarm *out, int max); // idem, veilig vanuit een andere taak (bv. de webserver)
int alarms_unacked();                     // aantal rode, nog niet bevestigde alarmen
AlarmLevel alarms_highest();              // hoogste actieve niveau
AlarmLevel alarms_for_device(const char *mac);
void alarms_ack_all();
void alarms_test_buzzer();
