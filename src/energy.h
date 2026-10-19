#pragma once
#include "store.h"

// Opgetelde energiestroom over alle verse apparaten (gebruikt door overzicht en historie)
struct EnergySummary {
  float pv;         // zonnepaneelvermogen (W)
  float solarOut;   // laadvermogen van de MPPT's naar de accu (W)
  float yield;      // opbrengst vandaag (kWh)
  float shoreOut;   // laadvermogen walstroomladers (W)
  float battW;      // accuvermogen (+ laden, - ontladen)
  float battV;
  float battA;
  float soc;        // NAN = onbekend
  float dcIn;       // overige laders naar de accu (W): Orion XS, dynamo, generator ... via een DC-energiemeter
  float load;       // DC-verbruik (W), NAN zonder shunt
  int32_t ttg;      // resterende tijd (min), -1 = onbekend
  bool haveSolar, haveShore, haveShunt, haveDcIn;
  uint8_t chargeState;  // laadfase van de lader die nu laadt, 0xFF = geen
};

constexpr uint32_t FRESH_S = 120;  // ouder dan dit telt niet mee
bool isFresh(const DeviceState &d);
EnergySummary energy_summarize(const DeviceState *devs, int n);
bool isCharging(uint8_t state);
