# Bosun – parts list

*[Nederlandse versie: BESTELLIJST.md](BESTELLIJST.md)*

Everything you need to build Bosun, with a search term for the web shop. Prices are indicative (October 2026, in euros,
shipping not included); check the price in the shop before ordering. How everything is connected is in
[ASSEMBLY.md](ASSEMBLY.md).

## Needed

| Part | Qty | Why | Search term | Price (€) | Note |
|---|---|---|---|---|---|
| **Waveshare ESP32-S3-Touch-LCD-4**, touch version | 1 | the screen with the ESP32-S3; CAN for NMEA 2000, clock, buzzer and micro SD are on it | Waveshare ESP32-S3-Touch-LCD-4 | 35–55 | Take the touch version. Not the -4B: it has no CAN and no 12 V input. |
| **Rechargeable RTC cell ML2032 or ML2020 with JST SH plug** | 1 | keeps the clock running without power | Raspberry Pi 5 RTC battery ML2032 | 5–8 | **Strongly recommended:** without it the time is lost after every power cut. Like the RTC battery for the Raspberry Pi 5. Check that red ends up at the + or 1 on the board before plugging in. Never an ordinary CR2032. |
| Fuse 1 A with holder | 1 | protects the board's 12 V supply | inline fuse holder 1A blade | 3–6 | Place it close to the battery. Not needed behind a breaker on the switch panel if the wire suits the breaker (see ASSEMBLY.md). |
| Wire 1 mm², red and black, with heat-shrink | 1 set | power | 1 mm² wire red black | 5–12 | |
| **Wire ferrules 0.25–1.5 mm² with crimp tool** | 1 set | on every wire in the screw terminal: stays put when the boat vibrates | ferrule crimping tool kit | 20–30 | Including 0.25 mm² for the thin sensor cable and a few twin ferrules 2 × 0.5 mm². |
| NMEA 2000 drop cable, M12 female, one open end | 1 | connects the board to the boat's bus | NMEA 2000 drop cable M12 female | 15–30 | Length: up to the bus (max 6 m). |
| USB-C data cable | 1 | first firmware and the serial log | USB-C data cable | 3–8 | A cable that carries data, not a charge-only cable. |
| Multimeter | 1 | checking voltage, the bus and the termination (60 Ω), polarity of the RTC cell | multimeter | 10–25 | An ordinary multimeter is enough. |

**Total needed: about €96–174.**

## Sometimes needed

| Part | Qty | Why | Search term | Price (€) | Note |
|---|---|---|---|---|---|
| NMEA 2000 T-piece | 1 | so your drop cable can connect to the bus | NMEA 2000 T-piece | 10–20 | Only if the bus has no free connection. Only one terminator at each end of the bus. |

## Optional

| Part | Qty | Why | Search term | Price (€) | Note |
|---|---|---|---|---|---|
| Micro SD card, FAT32, 8–32 GB | 1 | readings, log and backups | micro SD 16GB | 5–10 | |
| **DFRobot Gravity BME280 (SEN0236)** | 1 | barometer and storm warning (also humidity and temperature) | DFRobot SEN0236 | 15–20 | Locking plug, no soldering. Cut the DuPont plug off the cable, crimp ferrules on, into the I2C terminal. |
| 3D-printed enclosure | 1 | surface mount on a wall, or flush mount in a panel | PETG filament | 0–5 | Design in [`enclosure/`](enclosure/); print in PETG or ASA (PLA gets too soft in the sun). |

## Wireless sensors (per piece, as many as you like)

| Sensor | Why | Search term | Price (€) | Note |
|---|---|---|---|---|
| RuuviTag | temperature, humidity and air pressure (also as barometer) | RuuviTag | 25–35 | No wiring, no key. |
| ThermoBeacon thermometer (ORIA, Brifit, SensorBlue) | temperature and humidity | ORIA thermometer hygrometer bluetooth | 8–15 | Works as is. |
| Xiaomi LYWSD03MMC | cheap temperature and humidity | LYWSD03MMC | 8–12 | Flash the pvvx firmware first; see [COMPATIBILITY.md](COMPATIBILITY.md). |
| Mopeka Pro Check | tank level through the tank wall: gas, water, diesel, black water | Mopeka Pro Check | 45–70 | Calibrate at full and empty. |
| Victron devices with Bluetooth | battery, solar, chargers | (you probably have them already) | | Key from VictronConnect. |

All supported sensors and instruments: [COMPATIBILITY.md](COMPATIBILITY.md).
