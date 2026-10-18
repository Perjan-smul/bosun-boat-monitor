#pragma once
// Optionele zoemer: PCF8574 op I2C (automatisch gevonden) of een directe GPIO
void buzzer_begin();     // na Wire.begin() aanroepen
bool buzzer_present();
void buzzer_set(bool on);
