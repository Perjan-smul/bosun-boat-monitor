#pragma once
// Initialiseert scherm, touch en LVGL
void display_begin();

// Scherm aanzetten (bv. bij een alarm)
void display_wake();
// true = scherm niet automatisch uitzetten (bv. zolang er een onbevestigd alarm is)
void display_keep_awake(bool keep);

// Hoogste helderheid van de achtergrondverlichting (0-255), bv. gedimd voor het nachtthema
void display_set_max_brightness(uint8_t level);
