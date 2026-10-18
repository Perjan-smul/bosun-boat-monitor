# Compatibility

*[Nederlandse versie: COMPATIBILITEIT.md](COMPATIBILITEIT.md)*

Which boards, sensors and instruments work with Bosun. "Tested" means: the decoder has been checked against real
packets or the open-source reference (Victron, canboat, Home Assistant, ESPHome). Anything not yet seen on real
hardware aboard is marked as such.

**Quick check for a Bluetooth sensor: which app does it come with?**

| Manufacturer's app | Works? |
|---|---|
| VictronConnect | yes, with the key from the app |
| Ruuvi Station | yes |
| ThermoBeacon (also ORIA, Brifit, SensorBlue) | yes |
| Shelly Smart Control (BLU sensors) | yes, with encryption turned off |
| Mopeka Tank Check | yes, the Pro series: gas, water, black water, diesel |
| Mi Home / Xiaomi Home | only with different firmware (see *Xiaomi*) |
| Govee Home, Inkbird, SwitchBot, Tuya / Smart Life | no (not yet) |

## Boards

| Board | Status |
|---|---|
| **Waveshare ESP32-S3-Touch-LCD-4** (V3.0 and V4.0) | supported; everything on the board: CAN, RTC, buzzer, micro-SD |
| LilyGO T-Panel S3 | no longer supported (Waveshare only) |
| Waveshare ESP32-S3-Touch-LCD-4**B** | **no**: no CAN and no 12 V input |
| Guition ESP32-4848S040 | no longer supported |

## Bluetooth

Add Bluetooth sensors via ⚙ Settings → *Add device*. The board recognises the type by itself; only devices in this
list show up when searching.

### Victron (key needed)

The key is in VictronConnect → device → ⚙ → *Product info* → *Instant readout via Bluetooth*.

| Device | What you see |
|---|---|
| SmartShunt, BMV-712 Smart | SoC, voltage, current, power, time remaining, consumed Ah, starter battery or midpoint or temperature, alarms |
| Smart Battery Sense | voltage and temperature |
| SmartSolar and BlueSolar MPPT (with Bluetooth) | PV power, charge current, charge state, yield today, errors |
| Orion Smart DC-DC charger | input and output voltage, state, errors |
| Blue Smart IP22 / IP65 / IP67 shore charger, Smart Skylla | power, charge state, voltage, charge current, temperature, AC current, errors |
| Smart Lithium (LiFePO4 Smart) | battery voltage, lowest and highest cell voltage, temperature, balancer, errors |
| Smart BatteryProtect | output on/off, input and output voltage, state, warnings, off reason |
| Lynx Smart BMS | SoC, voltage, current, power, time remaining, consumed Ah, temperature, BMS errors (counts as battery monitor) |
| MultiPlus, Quattro (VE.Bus, via the VE.Bus Smart dongle) | state, battery voltage and current, active AC input, AC power in and out, temperature, SoC, VE.Bus error. When it charges from shore power, that counts as **Shore** on the overview |
| Multi RS (Solar) | state, battery voltage and current, AC input, AC in/out, PV power and yield today (counts as **Solar** and **Shore** on the overview), errors |
| Phoenix inverter (Smart, or VE.Direct with Bluetooth) | apparent power (VA), state, battery voltage, AC voltage and current, alarms |
| Orion XS DC-DC charger | power, state, input and output voltage and current, errors |
| SmartShunt as DC energy meter (e.g. for a charger or inverter) | power, voltage, current, starter battery or temperature, alarms |
| Other Victron | recognised, signal only |

### Temperature and humidity

| Sensor | Requirement | What you see |
|---|---|---|
| **RuuviTag**, RuuviTag Pro | standard (data format 5) | temperature, humidity, **air pressure** (also as barometer), battery |
| **ThermoBeacon** thermometers: **ORIA**, Brifit, SensorBlue and other brands using the *ThermoBeacon* app | standard | temperature, humidity, battery |
| **Xiaomi** LYWSD03MMC, MHO-C401, MHO-C122; Qingping CGG1, CGDK2 | **different firmware** (pvvx or ATC1441), format *Custom* or *ATC1441* | temperature, humidity, battery |
| **BTHome v2** sensors: Shelly BLU H&T, sensors with pvvx firmware in BTHome mode, ESPHome | **unencrypted** (Shelly: turn encryption off in the app) | temperature, humidity, air pressure, voltage, current and more |

- **ThermoBeacon** (ORIA, Brifit): decoder follows Home Assistant (*thermobeacon-ble*) and is tested on the packets
  from it; not yet seen aboard. Doesn't work with yours? Send in the log.
- **Xiaomi**: with the stock firmware Xiaomi encrypts the readings. The open firmware by pvvx can be flashed in a few
  minutes from the browser (search for *pvvx TelinkFlasher*). Then choose the format *Custom* (pvvx), *ATC1441* or
  *BTHome* in the sensor's settings.

### Tanks

| Sensor | What you see |
|---|---|
| **Mopeka Pro Check** (gas), Pro Check H2O (water), Pro Plus, Pro Check Universal, Lippert BottleCheck | stuck to the bottom of the tank: level in % after calibration, temperature, battery, reading quality |
| **Mopeka Pro-200, TD40, TD200** | on top of the tank (measures through the air to the surface); otherwise the same |

For each sensor you choose the **contents**: gas, fresh water, grey/black water or diesel/petrol. Bosun then uses the
right speed of sound (Mopeka's coefficients, via Home Assistant *mopeka-iot-ble*). For grey/black water the alert
comes when the tank gets **full** (default 75 % / 90 %), for the rest when it is nearly **empty**.
Tested on the packets from Home Assistant; the water and top-mounted types have not yet been seen aboard.

The older Mopeka *Standard* (non-Pro) does not work.

### Does not work (yet)

| Sensor | Why |
|---|---|
| Xiaomi with stock firmware (Mi Home) | encrypted with a *bindkey*; flash pvvx firmware |
| Govee (H5075, H5074, H5179, ...) | own format, no decoder yet |
| Inkbird (IBS-TH1, IBS-TH2, ...) | own format, no decoder yet |
| SwitchBot Meter | own format, no decoder yet |
| BTHome **encrypted** | keys are not supported; turn encryption off |
| Ruuvi Air (data format 6) | newer format, no decoder yet |
| AIS, Bluetooth depth sounders, Bluetooth batteries from other brands | not supported |

Want a sensor added? Open an issue with the brand, the type and the app. With a decoder from Home Assistant
(*Bluetooth-Devices* on GitHub) that is usually little work.

## NMEA 2000

Via CAN (the Waveshare's green terminal). Anything that sends these PGNs works, whatever the brand
(Garmin, Raymarine, B&G/Simrad/Lowrance, Furuno, Victron GX, Yacht Devices, ...).

| PGN | Contents |
|---|---|
| 126992, 129029 | date and time (GPS) |
| 129033 | the plotter's local time (offset from UTC) |
| 129025, 129029 | position |
| 129026 | COG and SOG |
| 127250 | heading (compass) |
| 128259 | speed through water |
| 128267 | depth |
| 130306 | wind (apparent and true) |
| 127488, 127489 | engine: RPM; temperature, oil pressure, alternator, fuel rate, engine hours, load |
| 127505 | tank levels |
| 130312, 130316 | temperatures |
| 130314, 130311, 130310 | air pressure (barometer) |
| 127508, 127506 | batteries: voltage, current, temperature, SoC, time remaining (e.g. from a Victron GX, Mastervolt or a battery monitor on the bus) |
| 127245 | rudder angle |
| 127257 | heel (roll) and pitch |
| 130313 | humidity (inside/outside) |

Tested against *canboat* on seven real boat recordings: about 770,000 messages, of which about 105,000 battery, rudder,
attitude and humidity messages, all identical. Not supported: NMEA 0183, AIS messages, digital switching (127501),
and transmitting: Bosun only listens.

### Which brand uses which system?

Many brands give NMEA 2000 their own name or connector; the signal is the same. An overview (general, not checked per
model: if in doubt, check the manual or the back of the device).

**Instruments, plotters and autopilots**

| Brand | Now (about 2010 and later) | Older | Works with Bosun? |
|---|---|---|---|
| Raymarine | **SeaTalk NG** = NMEA 2000 with its own connectors (i70, p70, Axiom) | **SeaTalk 1** (ST40, ST60, ST4000), NMEA 0183 | NG: yes, with an adapter cable. SeaTalk 1: only via the SeaTalk 1 → SeaTalk NG converter |
| Garmin | NMEA 2000 | NMEA 0183 | NMEA 2000: yes |
| B&G, Simrad, Lowrance | NMEA 2000 (Simrad: **SimNet**, different connector, same signal) | B&G **Fastnet**, LowranceNet, NMEA 0183 | modern: yes (SimNet with an adapter cable). Fastnet: no |
| Furuno | NMEA 2000 (CAN bus) | NMEA 0183 | NMEA 2000: yes |
| NASA Marine (Clipper) | NMEA 0183 | NMEA 0183 | no (NMEA 0183 is not supported) |
| Tacktick (wireless) | via a Raymarine interface to SeaTalk NG or NMEA 0183 | | via SeaTalk NG: yes |
| Humminbird | NMEA 0183, partly NMEA 2000 | NMEA 0183 | only the NMEA 2000 models |

**VHF and AIS** (Icom, Standard Horizon, em-trak, Vesper, Digital Yacht): usually NMEA 0183, AIS often also
NMEA 2000. Bosun only uses their GPS data; not AIS.

**Energy**

| Brand | System | Works? |
|---|---|---|
| Victron | Bluetooth (Instant Readout), and VE.Can (NMEA 2000 compatible) via a GX | yes, both |
| Mastervolt | **MasterBus**: own system, **not** NMEA 2000 | only via the Mastervolt gateway to NMEA 2000 |
| Batteries with their own app (LiTime, JK, Daly, ...) | own Bluetooth with a connection | no |

**Engines**

| Brand | System | Works? |
|---|---|---|
| Volvo Penta | **EVC**, own system | via a Volvo gateway to NMEA 2000 |
| Yanmar | own system | via a gateway to NMEA 2000 |
| Mercury | **SmartCraft**, own system | via the VesselView Link (NMEA 2000) |
| Yamaha, Suzuki, Honda | own system | via a gateway to NMEA 2000 |
| Older diesels (Vetus, Beta, Nanni, ...) | analogue gauges (resistive senders) | no |

**In short:** NMEA 2000 covers almost all modern equipment, also under another name (SeaTalk NG, SimNet, VE.Can) or
via a gateway. Replacing old equipment? Choose something with NMEA 2000.

## Wired (I2C and card)

| Part | Board | Note |
|---|---|---|
| **BME280** / BMP280 (air pressure) | Waveshare | I2C, address 0x76 or 0x77, found automatically; terminal `VCC GND SDA SCL`, jumper on 3V3 |
| PCF85063 clock | Waveshare | on the board; **rechargeable RTC cell (ML1220) strongly recommended** on the *RTC* connector, not a CR2032 |
| micro-SD card | Waveshare | FAT32, up to 32 GB; readings, log and backups |
