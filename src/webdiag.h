#pragma once
#include <WebServer.h>

// Wifi-pagina: diagnose (wat het board ziet en doet, met logboek) en backup/terugzetten van de instellingen.
// allowed: voor gewone pagina's (vraagt zo nodig om de code); authOk: stille controle tijdens een upload.
void webdiag_register(WebServer &web, bool (*allowed)(), bool (*authOk)());

// Backup als tekst maken en terugzetten (ook los te testen)
String backup_make();
// Controleert en schrijft de backup naar het geheugen. Geeft false met een melding bij een fout;
// bij succes staat in msg wat er is teruggezet.
bool backup_restore(const String &text, String &msg);
