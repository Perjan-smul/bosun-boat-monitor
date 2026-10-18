#pragma once
#include <stddef.h>
#include <stdint.h>
#include <Arduino.h>

// Micro-SD-kaart (Waveshare): metingen per minuut als CSV, een kopie van het logboek die een
// stroomuitval overleeft, en automatische backups. Op boards zonder kaartslot doet dit niets.
//
//   /bosun/metingen/2026-10-13.csv   (nieuw deel _2, _3 ... als er kolommen bijkomen)
//   /bosun/logboek/2026-10.txt
//   /bosun/backup/backup-2026-10-13.txt
bool sd_supported();  // heeft dit board een kaartslot?
void sd_begin();      // na display_begin(): de kaart deelt pinnen met de init van het scherm
void sd_tick();       // vaak aanroepen vanuit de UI-lus; schrijft zelf op het juiste moment
                      // (de kaart wordt alleen bij het opstarten gezocht)

struct SdInfo {
  bool present;
  bool wasPresent;       // deze start al eens een kaart gehad (weg of kapot = melding waard)
  uint64_t totalBytes, usedBytes;
  uint32_t rowsWritten;  // metingen deze start
  char lastError[48];    // Nederlandse sleutel, leeg = geen fout
};
SdInfo sd_info();

// Voor de webpagina (andere taak): alles onder het slot
struct SdFile {
  char path[48];  // relatief, bv. "metingen/2026-10-13.csv"
  uint32_t size;
};
int sd_list(const char *dir, SdFile *out, int max);  // nieuwste eerst
bool sd_valid_path(const char *rel);                 // alleen bestanden in onze eigen mappen
bool sd_read_text(const char *rel, String &out, size_t maxLen);
// Bestand naar de webserver streamen; geeft false als het niet lukt
class WebServer;
bool sd_send_file(WebServer &web, const char *rel);
