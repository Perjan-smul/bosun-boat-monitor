#pragma once
// Zoemer op het board (via de hulpchip van de Waveshare)
void buzzer_begin();     // na wsio_begin() aanroepen
bool buzzer_present();
void buzzer_set(bool on);
