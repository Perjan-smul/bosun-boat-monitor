#include "energy.h"
#include <Arduino.h>
#include <math.h>

bool isFresh(const DeviceState &d) {
  return d.hasData && d.lastSeenMs && (millis() - d.lastSeenMs) / 1000 < FRESH_S;
}

bool isCharging(uint8_t state) { return (state >= 3 && state <= 7) || state == 245 || state == 247; }

EnergySummary energy_summarize(const DeviceState *devs, int n) {
  EnergySummary e = {0, 0, 0, 0, 0, NAN, 0, NAN, 0, NAN, -1, false, false, false, false, 0xFF};
  for (int i = 0; i < n; i++) {
    const DeviceState &d = devs[i];
    if (!isFresh(d)) continue;
    const Metrics &m = d.reading.m;
    switch (d.cfg.type) {
      case SensorType::VictronSolar:
        e.haveSolar = true;
        if (!isnan(m.pvWatts)) e.pv += m.pvWatts;
        if (!isnan(m.watts)) e.solarOut += m.watts;
        if (!isnan(m.yieldKwh)) e.yield += m.yieldKwh;
        if (isnan(e.battV)) e.battV = m.volts;
        if (isCharging(m.state) && m.pvWatts > 0) e.chargeState = m.state;
        break;
      case SensorType::VictronAcCharger:
        e.haveShore = true;
        e.shoreOut += m.watts;
        if (isCharging(m.state)) e.chargeState = m.state;  // walstroom gaat voor
        break;
      case SensorType::VictronVeBus:
      case SensorType::VictronMultiRs: {
        // MultiPlus/Quattro/Multi RS: wat er bij aangesloten walstroom (of generator) de accu in gaat, telt als "Wal".
        // De Multi RS heeft ook een MPPT: dat deel telt als "Zon".
        float pv = isnan(m.pvWatts) ? 0 : m.pvWatts;
        if (d.cfg.type == SensorType::VictronMultiRs) {
          e.haveSolar = true;
          e.pv += pv;
          e.solarOut += pv;
          if (!isnan(m.yieldKwh)) e.yield += m.yieldKwh;
        }
        bool acConnected = m.acIn == 0 || m.acIn == 1;
        if (acConnected) {
          e.haveShore = true;
          if (!isnan(m.watts)) e.shoreOut += fmaxf(m.watts - pv, 0);
          if (isCharging(m.state)) e.chargeState = m.state;
        }
        if (isnan(e.battV) && !isnan(m.volts)) e.battV = m.volts;
        break;
      }
      case SensorType::VictronOrionXs:  // DC-DC-lader, meestal van de startaccu/dynamo
        if (isnan(m.watts)) break;
        e.haveDcIn = true;
        e.dcIn += fmaxf(m.watts, 0);
        if (isCharging(m.state) && m.watts > 0 && e.chargeState == 0xFF) e.chargeState = m.state;
        break;
      case SensorType::VictronDcMeter: {
        // SmartShunt als DC-energiemeter op een bron: telt mee als lader (een verbruiker zit al in het verbruik)
        if (m.meterType >= 0 || isnan(m.watts)) break;
        float w = fmaxf(m.watts, 0);
        if (m.meterType == -9) {  // zonnelader (bv. een MPPT zonder Bluetooth)
          e.haveSolar = true;
          e.pv += w;
          e.solarOut += w;
        } else if (m.meterType == -2) {  // walstroomlader
          e.haveShore = true;
          e.shoreOut += w;
        } else {
          e.haveDcIn = true;
          e.dcIn += w;
        }
        break;
      }
      case SensorType::VictronBattery:
      case SensorType::VictronLynx:
        if (isnan(m.amps)) break;  // Battery Sense: geen stroom
        e.haveShunt = true;
        e.battW += m.watts;
        e.battA += m.amps;
        e.battV = m.volts;
        if (!isnan(m.soc) && isnan(e.soc)) e.soc = m.soc;
        if (m.ttgMin >= 0 && e.ttg < 0) e.ttg = m.ttgMin;
        break;
      default: break;
    }
  }
  // Verbruik = alles wat binnenkomt - wat de accu in gaat
  if (e.haveShunt) e.load = fmaxf(e.solarOut + e.shoreOut + e.dcIn - e.battW, 0);
  return e;
}
