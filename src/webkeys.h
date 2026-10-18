#pragma once
#include <WebServer.h>

// Pagina's voor Victron-sleutels op de wifi-pagina (zelfde code als de firmware-update)
void webkeys_register(WebServer &web, bool (*allowed)());

// Gedeeld door de wifi-pagina's
String web_esc(const char *s);  // tekst veilig in HTML zetten
const char *web_head();         // begin van een pagina met de opmaak
