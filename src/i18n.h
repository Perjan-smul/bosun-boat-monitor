#pragma once
#include <stdint.h>

// Talen. De Nederlandse tekst in de code is de sleutel; de vertalingen staan in i18n/vertalingen.tsv
// en worden met scripts/i18n_gen.py omgezet naar src/i18n_table.cpp.
// Volgorde niet wijzigen: het nummer wordt bewaard. Nieuwe talen achteraan.
enum class Lang : uint8_t { NL, EN, FR, DE, ES, IT, PT, SV, NB, DA, FI, COUNT };
constexpr Lang LANG_DEFAULT = Lang::EN;  // tot er een taal gekozen is

void i18n_begin();             // taal uit het geheugen lezen (vóór het scherm)
bool i18n_chosen();            // al een taal gekozen? (anders bij de eerste start vragen)
Lang i18n_lang();
void i18n_set(Lang lang);      // kiezen en bewaren
const char *i18n_name(Lang lang);  // naam in de taal zelf: "Nederlands", "English", ...

// Vertaling van een Nederlandse tekst in de gekozen taal (onbekend: de Nederlandse tekst zelf).
// Ook voor printf-opmaak: de vertaling heeft dezelfde %-codes in dezelfde volgorde (gecontroleerd).
const char *tr(const char *nl);
#define T(s) tr(s)
// Markeert een tekst als te vertalen zonder hem nu te vertalen (bv. in een vaste tabel);
// vertalen gebeurt bij het gebruik met tr().
#define TN(s) (s)
