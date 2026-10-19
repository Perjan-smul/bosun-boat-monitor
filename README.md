# Bosun – the boat monitor

*An ESP32-S3 touchscreen for your boat.* [Nederlandse versie: LEESMIJ.md](LEESMIJ.md)

A dashboard for BLE sensors and NMEA 2000 on board, running on an ESP32-S3 with a 4" touchscreen (480×480).
You add and manage sensors directly on the touchscreen, without having to change any code.

## Board

Bosun runs on the **Waveshare ESP32-S3-Touch-LCD-4** (4", 480 × 480): NMEA 2000 (CAN), clock (PCF85063), buzzer,
micro-SD and a 7–36 V power supply are all on the board.

The Waveshare has everything on board: you only connect 12 V and the NMEA 2000 cable. Mind the name: the
**ESP32-S3-Touch-LCD-4B** is a different board (audio, no CAN, no 12 V input) and does not work without extra modules.
Waveshare hardware versions V4.0 (helper chip CH32V003) and V3.0 (TCA9554) are detected automatically.
Bosun has not yet been tested on a real board; the pins come from Waveshare's own examples and the
V4.0 schematic.

**What to buy:** [PARTS.md](PARTS.md) (parts list with search terms and prices). **How to connect and mount it:** [ASSEMBLY.md](ASSEMBLY.md).

## What it does

| Sensor | What you see | Note |
|---|---|---|
| Victron SmartShunt / BMV | SoC, voltage, current, power, time remaining, Ah, aux | key needed |
| Victron SmartSolar MPPT | PV power, charge state, battery voltage/current, yield today | key needed |
| Victron Orion Smart DC-DC | status, input/output voltage | key needed |
| Victron Blue Smart / Skylla shore power charger | power, charge stage, voltage/current, temperature | key needed |
| Victron Smart BatteryProtect | on/off status, battery voltage, output voltage, alarm (low voltage). Alarm bits give a red alert | key needed |
| Victron Lynx Smart BMS | SoC, voltage, current, power, time remaining, consumed Ah, temperature (as a battery monitor, so on the battery page) | key needed |
| Victron MultiPlus / Quattro (VE.Bus) and Multi RS | state, battery, AC input, AC in/out (Multi RS also PV); charging from shore counts as Shore on the overview | key needed |
| Victron Phoenix inverter, Orion XS, DC energy meter | power, voltages, current, state, errors | key needed |
| RuuviTag (format 5) | temperature, humidity, air pressure, battery | |
| Xiaomi LYWSD03MMC (ATC1441 / pvvx firmware) | temperature, humidity, battery | |
| BTHome v2 (unencrypted) | temperature, humidity, pressure, voltage, current, ... | e.g. Shelly BLU, pvvx in BTHome mode |
| ThermoBeacon (ORIA, Brifit, SensorBlue) | temperature, humidity, battery | |

The full list, including what does not work (yet) and how to check it yourself: [COMPATIBILITY.md](COMPATIBILITY.md).

**Operation**

Swipe left/right between the pages. The dots at the bottom show where you are. A page only appears
when devices of that kind are paired:

| Page | When visible | Contents |
|---|---|---|
| **Overview** | MPPT, battery monitor or shore power charger paired | Energy flow with ring gauges: Solar, Shore, Battery and Load around a central hub showing the charge stage (e.g. Absorption) or the time remaining. Moving glowing sparks in the colour of the flow show the direction (faster = more power). At the top a weather line: temperature and humidity from a climate sensor (selectable in the settings), and the air pressure with a weather icon (sun to storm, based on the pressure) and a trend arrow over 3 hours (orange/red on a storm warning) |
| **Solar panels** | MPPT paired | Total + per charger: PV power, coloured status (Bulk/Absorption/Float), battery voltage, yield today |
| **Batteries** | SmartShunt / BMV / Battery Sense | SoC arc, power (green = charging, orange = discharging), time remaining, temperature |
| **Climate** | Ruuvi / ATC / BTHome | Temperature, humidity, battery |
| **Barometer** | BME280, NMEA 2000 air pressure or a Ruuvi | Air pressure, trend over 3 hours, 48-hour history, storm warning |
| **Chargers & other** | Orion DC-DC / other Victron | Status and voltages |
| **Devices** | always | All devices; tap a tile for details, editing or removing |

- **Adding:** ⚙ Settings → at the top *Add device* → **+** to scan (with no devices, the
  button is also on the empty page). Sensors that are found appear live with their signal strength.
  Tap one, give it a name and save. For Victron you also enter the key (see below).
- **Status per device:** green = recently received, orange = more than 30 s ago, red = more than 2 min ago.
- **Load on the overview** is calculated as: charging power of MPPTs + shore power chargers − power into/out of the battery (shunt).
  Without a shunt the load is unknown.
- **Scale of the rings**: full at 400 W solar, 500 W load and 1000 W shore (grows if more is measured).
  Adjust in `src/ui.cpp` (`SCALE_SUN_W` etc.). The battery ring shows the SoC.
- **Screen off after 1 minute** without touch (gentle fade-out). Tap to turn it back on; that tap is not treated as
  a click. You choose the time in ⚙ Settings (never / 30 s / 1, 2, 5 or 10 min).

## Themes (colours)

Four colour schemes, selectable in ⚙ Settings → *Theme* (screenshot: `screenshot_themas.png`):

| Theme | Characteristics |
|---|---|
| **Ocean** | dark blue-black with a teal accent (default) |
| **Elegant** | true black with blue accents; the energy flow on the overview is also in shades of blue |
| **Light** | light background and white cards, for daytime in the sun; crisp dots instead of glowing sparks |
| **Night** | everything in dimmed red on black and the backlight dimmed, so you keep your night vision; a red alarm stays bright |

**Automatic day/night** (switch under *Theme*): the chosen theme during the day, after sunset the *Theme at night*
(default Night). With a GPS on NMEA 2000 it switches on the real position of the sun: dark as soon as the sun is more than 4° below the
horizon (a quarter to half an hour after sunset), light again at sunrise. The last known position is
kept, even when the GPS is off. Without GPS: fixed times, 21:00 to 7:00. Switching only happens on the
dashboard, not in the middle of editing a device.

**Startup screen:** when you switch on, *Bosun – The boat monitor* appears with a compass star inside a ring, around which
a glowing dot spins while the board starts up (screenshot: `screenshot_opstartscherm.png`). It follows the
theme, so at night it is red straight away. It stays for at least 1.5 seconds and then fades into the overview.
You regenerate the images with `scripts/splash_gen.py`.

## Languages

Nederlands, English, Français, Deutsch, Español, Italiano, Português, Svenska, Norsk, Dansk and Suomi: the screen,
the alerts, the WiFi pages and the recovery screen.

- **First start**: the board first asks for the language (English until then). Each language is shown in that language itself.
- **Changing later**: ⚙ Settings → *Language* → **Choose**. The choice is kept and is also included in the backup.
- Names you give yourself (e.g. "Fridge") stay exactly as you type them.
- The log on the diagnostics page stays in Dutch (it is meant for troubleshooting).
- The clock shows the time you set yourself, also outside Central Europe (see *Clock*).

**Adjusting translations or adding a language** (for developers):
- Texts are in the code in Dutch inside `T("...")`; that is the key.
- The translations are in `i18n/vertalingen.tsv` (columns nl, en, fr, de, es, it, pt, sv, nb, da, fi).
- `python3 scripts/i18n_gen.py check` shows what is still missing (in `i18n/ontbreekt.tsv`);
  `python3 scripts/i18n_gen.py gen` creates `src/i18n_table.cpp`. The script rejects translations whose
  %-codes differ from the Dutch, because that could crash the board.
- A new language: add it at the end of `Lang` (`src/i18n.h`), in `i18n_name()` and in `LANGS` of both scripts.
  The font (`src/fonts`, Montserrat) has all Western European characters (Latin-1); other scripts need
  a new font (lv_font_conv).

## Alerts: warning (orange) and alarm (red)

Every limit has two levels. **Orange** is a warning: the screen turns on briefly, but there is no buzzer.
**Red** is an alarm: the buzzer sounds and the screen stays on until you tap **Mute**.

| Alert | Where to set | Default (orange / red) |
|---|---|---|
| Battery: SoC below | per battery, under *Edit* | 40 % / 25 % |
| Battery: voltage below | per battery | off / off |
| Temperature above | per thermometer, e.g. fridge 5 / 8 °C | off / off |
| Temperature below | per thermometer, e.g. cabin 3 / 0 °C (frost) | off / off |
| No signal or no valid reading (e.g. the key is no longer correct) | ⚙ Settings: off / 5 / 10 / 30 / 60 min | orange after 10 min |
| Victron alarm (low voltage, overheated, short circuit, ...) | ⚙ Settings: on/off | red |
| Victron charger error (PV voltage too high, charger too hot, ...) | ⚙ Settings: on/off | orange |

**What you see and hear**
- **Border around the whole screen** in the colour of the most serious alert. With an unacknowledged alarm the red border flashes.
- **Pill in the header bar** with the most important alert (red first) and `(+n)` if there are more. Tap it for the list.
- **Tile** of the device gets an orange or red border. The alert is also shown at the top of the detail screen.
- **Mute** (in the list) acknowledges the red alarms: the buzzer stops, the border stops flashing and the screen may turn off again.
  The alert stays visible as long as the cause is there.
- **Cause gone**: the alert disappears. Limits have hysteresis (SoC 2 %, voltage 0.2 V, temperature 0.5 °C).
  A red alarm that drops back to orange and then becomes red again is unacknowledged again.
- **Buzzer**: only beeps on red, two short beeps every 2.5 s. Switch off in ⚙ Settings → *Buzzer on alarm*.

The buzzer is on the board; there is nothing to connect.
In ⚙ Settings you can see whether the buzzer was found and you can test it. Without a buzzer the alarms only work on screen.

## NMEA 2000

Bosun **only listens** on the bus (250 kbit/s, listen-only). It transmits nothing, claims no address and can therefore
never disturb the bus. On/off and the bus status (messages/s, number of devices) are in ⚙ Settings.

| Page | Appears when ... is received | Contents |
|---|---|---|
| **Navigation** | GPS, depth, log, compass or wind | depth, SOG + speed through water, COG + compass heading, apparent and true wind, position, temperatures (sea water large) |
| **Engine** | engine data (1 or 2 engines) | RPM as a gauge, coolant (orange > 85 °C, red > 95 °C), oil pressure, alternator, fuel consumption, engine hours, load |
| **Tanks** | tank levels | up to 12 tanks as bars: diesel, water, grey water, black water ... with % and litres. Red when nearly empty (diesel/water) or nearly full (waste water) |

**NMEA 2000 alarms** (optional, **off by default**). Turn on in ⚙ Settings → *NMEA 2000 alarms*. With *Limits*
you set an orange and a red limit for each alarm; an empty field turns that alarm off.

| Alarm | Orange / red (pre-filled) |
|---|---|
| Depth below | 3.0 m / 2.0 m |
| Diesel below | 25 % / 10 % |
| Water below | 20 % / 10 % |
| Grey and black water above | 75 % / 90 % |
| Coolant above | 85 °C / 95 °C |
| Oil pressure below (only when the engine is running, > 400 rpm) | empty = off (differs per engine) |

- **Alerts**: work just like the others (bar at the top, border around the screen, buzzer only on red, *Mute*).
- **Colours on the pages**: the depth card, the engine card and the tank percentage take on the colour of the alert.
- **Briefly no data** (e.g. the depth sounder drops out): an existing alert stays, instead of quietly disappearing.
- **Engine off**: the oil pressure alert disappears, because oil pressure is meaningless then.

**Clock from GPS**: as soon as there is GPS time on the bus, the clock is set and the RTC is updated with it
(immediately if the clock is unknown, after that at most once an hour when the difference is more than 2 s). Can be switched off in ⚙ Settings.

| PGN | Contents |
|---|---|
| 126992, 129029 | date and time (GPS) |
| 129033 | local time of the chart plotter (offset from UTC; see *Clock*) |
| 129025, 129029 | position |
| 129026 | COG and SOG |
| 127250 | heading (compass) |
| 128259 | speed through water |
| 128267 | depth |
| 130306 | wind (apparent and true) |
| 127488, 127489 | engine: RPM; temperature, oil pressure, alternator, fuel consumption, engine hours, load |
| 127505 | tank levels |
| 130312, 130316 | temperatures |
| 130314, 130311, 130310 | air pressure (barometer) |
| 127508, 127506 | batteries: voltage, current, temperature, SoC, time remaining (on the Batteries page) |
| 127245 | rudder angle (Rudder card on Navigation) |
| 127257 | heel and pitch (Heel card on Navigation) |
| 130313 | humidity (with the temperatures; also in the weather line of the overview if there is no Bluetooth climate sensor) |

The decoder has been tested against **canboat** (the open-source reference for NMEA 2000), on seven real boat recordings
(about 770,000 messages) and on real CAN frames with fast packets. All values matched.

**Connecting**:
- The green 10-pin screw terminal on the bottom edge (printed `VIN GND | A B | L H | VCC GND SDA SCL`):
  NET-H (white) → **H** (pin 6), NET-L (blue) → **L** (pin 5), NET-C (black) → **GND** in the I2C block (pin 8).
  On the DIP switch **120R**, set **CAN** to off: Bosun hangs on a drop cable, not at the
  end of the backbone.
- Then check, with the backbone connected and the network powered off: **60 Ω** between H and L. If you read about 40 Ω there is a
  third terminator present (often the one on the board); it must be switched off.

## Bluetooth devices: Smart Lithium and Mopeka

- **Victron Smart Lithium** (Smart LiFePO4 with BMS): the battery appears on the **Batteries** page with the voltage,
  temperature, the lowest and highest cell voltage, and the balancer status (balanced / balancing / imbalance).
  As with the other Victron devices, you need the key (VictronConnect → Instant readout).
  A Smart Lithium does not provide a SoC, so the arc shows the voltage.
- **Mopeka tank sensors** (Pro Check, H2O, Pro Plus, Universal underneath the tank; Pro-200, TD40, TD200 on top of the
  tank): appear on the **Tank sensors** page with the level.
  - **Choosing the contents** (tile → *Edit* → *Tank contents*): gas, drinking water, grey/black water or
    diesel/petrol. Sound travels at a different speed in each liquid; Bosun uses Mopeka's coefficients
    for those contents. Sensors on top of the tank measure through the air and always calculate with air.
  - **Calibrating** (after choosing the contents and saving): enter the distance in mm that the tile shows when the tank
    is **full**, and when it is **empty**. Without those two values the tile only shows the distance.
  - The level is linear between those two values. Underneath the tank "full" is the largest distance, on top of the tank the
    smallest; both work automatically.
  - The battery, the temperature and the reading quality come from the sensor itself.
  - For a tank with an unusual shape (narrow at the bottom, wide at the top) the level is in height, not in litres.
- **Alerts** (set per sensor, orange / red, just like the others):
  - **Tank empty below** (default 20 % / 10 %) for gas, drinking water and diesel; **Tank full above** (75 % / 90 %) for
    grey/black water. Only after calibration. Without a reading an existing alert stays.
  - **Battery below** (default 25 % / 10 %): the sensor's battery, a CR2032.
  - Orange is a warning, red an alarm with buzzer; the alert appears in the bar and on the tile.
- **Not yet**: Mopeka Std (older CC2540 version) and the Smart Lithium on the overview page.

## WiFi page: Victron keys and firmware update

⚙ Settings → *WiFi page* → **Open**. Under **Connection**, choose:

**Own hotspot** (default, handy on board)
1. Turn on **WiFi page on**. The board creates a WiFi network `Bosun-XXXX` and shows the password (8 digits) and a QR code.
2. Scan the QR code with your phone's camera, or choose the network in the WiFi settings and type the password.
3. The phone says "Sign in to network" and opens the page by itself. If that doesn't happen: open `http://192.168.4.1`.
   No extra login is needed.
- While your phone is connected to the board, it has no internet over WiFi. Mobile data usually works as normal.
- Uploading firmware: preferably open `http://192.168.4.1` in the normal browser rather than in the sign-in window (that sometimes can't pick a file).

**Existing network** (boat router, marina WiFi or phone hotspot)
1. **Set up WiFi network**: enter the network name and password. These are stored on the board.
2. Turn on **WiFi page on**: the board connects to the network and shows an address (e.g. `http://192.168.4.23`) and an **8-digit code**.
3. Open the address on your phone, which is on the same network. Username `boot`, password = the code.

In both cases you choose **Enter Victron keys** or **Firmware update** on the page. The code is different every time.

### Entering Victron keys

Handy: copy the key in VictronConnect and paste it on the same phone, instead of retyping 32 characters.

- **Added**: the Victron devices that are already on the screen, with their status (*Receiving readings*, *Wrong key*, ...).
  Paste a new key, or change the name. An empty key field leaves the current key in place.
- **Found, not added yet**: Victron devices the board can hear. Enter a name and key and tap **Add**.
- Spaces and dashes in the pasted key are ignored. After a few seconds tap **Refresh** to see the status.
- Alarm limits and removing devices are done on the screen.

### Diagnostics and log

On your phone you can see what the board is doing, without a laptop:
- **Board**: firmware version, how long it has been running, why it (re)started last time (e.g. *power on*,
  *CRASH* or *WATCHDOG*), the number of starts, the free memory, the storage and the clock.
- **NMEA 2000**: messages per second and the number of devices on the bus.
- **Alerts** that are currently active.
- **Bluetooth**: per device the status, the signal strength and when the last reading came in; also the devices found.
- **Log**: the last ~100 lines with time. After a crash or watchdog restart the log from before the
  restart is still there (not after a power cut). With *Download log* you get it as a text file.

### Backing up the settings

- **Download backup**: all devices with keys and limits, the settings and the WiFi settings in one text file.
  The history is not included, and neither is the 8-digit code (that is new every time).
  The file contains the Victron keys and, if you use *Existing network*, the password of that WiFi network:
  keep it safe.
- **Restore**: choose the file, tick that you want to replace everything, and tap *Restore*. The board checks
  the file first (if there is an error, nothing changes) and then restarts. Not possible with an unacknowledged alarm.

### Firmware update

1. Choose the `.bin` file and upload it. That is `.pio/build/waveshare_4/firmware.bin` after `pio run`,
   or the update file from the installer page.
2. **Install new firmware?** appears on the screen. Tap **Install** to restart, or **Reject**.
   With no answer within 60 s it is rejected.

**Watchdog**
- If the program (screen and alerts) hangs for more than 15 s, the board restarts itself.
- Also during startup: if the board hangs in one phase for longer than 60 s (e.g. a stuck I2C bus to the
  clock or the touchscreen), it restarts. Without this the board would hang forever.
  Diagnostics then shows *Last start: WATCHDOG*, plus the log from before the restart.
- **Disabling a device after crashes**: if the board crashes or hangs twice while processing a Bluetooth packet or
  NMEA 2000 message from the same device, that device is ignored. Everything else keeps working.
  You see an orange alert (*… disabled after crashes*) and the status *Disabled (crashes)* on the tile.
  For NMEA 2000 this concerns one message type (PGN) from one device on the bus.
  To re-enable: ⚙ Settings, at the top, **Enable**. This is kept, even after power off/on.
  If the disabling coincides with the third failed start, the board first simply tries to start without
  that device; only if it still crashes then does the recovery screen follow.
- No restart loop: if the board crashes or hangs **3 times in a row** (within 2 minutes of starting),
  it then starts only the **recovery screen**. That shows:
  - an **error code**: `C` = crash, `W` = hung, with the number of the phase in which it went wrong
    (0 startup, 1 settings, 2 firmware update, 3 device list, 4 screen, 5 clock, 6 history,
    7 building the screen, 8 Bluetooth, 9 NMEA 2000, 10 running, 11 SD card). E.g. `C6` = crash during the history;
  - the last lines of the log;
  - the **WiFi page on** button: there you download the log (Diagnostics), restore a backup or install
    new firmware;
  - the **Try again** button: simply start up again.

  The recovery screen also skips the check after a firmware update, in case that update is itself the fault
  (a failed update will already have been rolled back earlier). If the recovery screen itself hangs, it stays frozen with the
  error code on screen instead of restarting again.
  In the recovery screen there are no alerts, no Bluetooth and no NMEA 2000. Switching the power off and on also gives a
  normal start again. If it went wrong in the screen itself, the WiFi page turns on by itself (own hotspot).
  A voltage that is too low (e.g. when starting the engine) does not count as a failed start.
- Every hour the free memory is written to the log, so that a memory leak over a long passage can be seen.

**Security**
- The page only works on the local network and can only be used with the code on the screen
  (with the own hotspot, the code is the WiFi password).
- WiFi is **off** until you turn on the WiFi page, and turns itself off after a period without use: adjustable under **Off after** (5, 10, 30 or 60 min; default 10).
- Nothing is uploaded or installed if there is an unacknowledged alarm.
- A file for a different board or anything other than Bosun firmware is rejected.
- The new version goes into the other slot. Only after **Install** does the board start from it. If the power fails
  before you confirm, the current version simply starts.
- If the new version does not start properly (restarted 2 times within 60 s), the board goes back to the previous version.
  After 60 s without errors the new version counts as good. A version that does start but does not work properly
  (e.g. no picture) is not rolled back: then USB is needed.
- After a rollback the old version simply reads in the settings and devices of the new version.
- The WiFi text is stored in the board without encryption. Do not put the board on a network you do not trust.

**Note**
- WiFi and Bluetooth run at the same time on the board. During an update the Bluetooth scan may be a little slower.
- The board must be in the marina or on the network for the update. Without a network the WiFi page drops out.

## History

The **History** page appears as soon as a battery, MPPT or thermometer is paired.

| Chart | 24 hours | 7 days |
|---|---|---|
| **Battery** | SoC per 5 min | SoC per hour |
| **Energy** | solar, load and battery power (W) | same, per hour |
| **Solar/day** | yield per day (kWh) for the last 7 days, with average | |
| **Temp** | up to 6 thermometers | same, per hour |

- **Readings**: every 10 s, averaged per 5 minutes and per hour.
- **Storage without data loss**:
  - The history (about 9.5 KB) is saved after every 5-minute block and at every change of day.
  - The current, half-filled block is saved every minute and completed after a restart.
  - On a power cut at most the last minute of readings is lost.
  - Each file is first written to a temporary file and then renamed in one go, with a CRC check.
    If the power fails during writing, the previous file stays intact; a damaged file is detected.
  - Flash wear: well over 50 years at this write frequency (wear levelling over 3.4 MB).
- **Devices and settings** are kept in NVS and saved immediately on every change.
- **Time axis**: with a clock (see below) the axes show real times and weekdays. After a power cut
  the gap in the chart gets the correct length. Without a clock the axis counts back from *now*, and after a restart
  there is a gap of unknown length.
- **Change of day**: this comes from the MPPT itself. Victron resets the daily yield to zero in the morning, and that moment counts as a new day.

## Barometer and storm warning

A dedicated **Barometer** page with the air pressure, the trend over 3 hours and the 48-hour history
(screenshot: `screenshot_barometer.png`). The page appears as soon as there is a source:

| Source (the first one available) | How |
|---|---|
| **DFRobot Gravity BME280 (SEN0236)** (about €15–20) | on I2C: the terminal `VCC GND SDA SCL` (I2C jumper on **3V3**). Address 0x77 (or 0x76), found automatically. Locking connector on the sensor side, no soldering; see the assembly guide. A loose GY-BME280 or BMP280 also works, but its push-on pins shake loose on a boat |
| **NMEA 2000** | an instrument that sends air pressure (PGN 130314, 130311 or 130310), e.g. a weather station |
| **Bluetooth** | a sensor with air pressure: RuuviTag or BTHome |

- **Sticking to one source:** sensors differ from each other by a few hPa. Bosun sticks with the chosen source
  as long as it is measuring; if it drops out and another takes over, the history starts again (otherwise a
  difference between sensors would look like a pressure drop).
- **Measuring:** every 30 s; per 10 minutes an average goes into the history. The 48-hour history is kept
  after a restart (saved every 30 minutes). A valid clock is required for this.
- **Trend** according to the WMO classification (difference over 3 hours): steady (< 0.5 hPa), slowly (up to 1.5), rising/falling
  (up to 3.5), quickly (up to 6) and very quickly. The trend appears after 3 hours of measuring.
- **Storm warning:** if the pressure falls by more than the limit in 3 hours, **orange**; 2 hPa more and it becomes **red**
  (with buzzer). Default 3 hPa (red at 5 hPa); adjustable in ⚙ Settings → *Storm warning*
  (off, 2, 3, 4 or 5 hPa). A rule of thumb: 3 hPa in 3 hours often means strong wind, 5 hPa or more a storm.
- **Fitting the BME280:** not directly against the warm screen; put a short length of cable in between. Use wire ferrules in the terminal and secure the cable with a cable tie. At sea the air pressure
  on board is the same as at sea level; on a lake at high altitude the value is lower, but the trend is correct.
- On the micro SD card the air pressure is included in the readings as the column *Barometer hPa*.
- **On the Overview** the air pressure is shown at the top right with a weather icon like on an old-fashioned barometer:
  sun (≥ 1022 hPa), sun with cloud (≥ 1009), cloud (≥ 1000), rain (≥ 990) and storm (lower). An arrow shows the trend
  (↑ rising, → steady, ↓ falling); on a storm warning the value turns orange or red. On the left and in the middle
  are the temperature and humidity of a climate sensor. You choose which one in ⚙ Settings → *Sensor on overview*
  (appears with two or more sensors). *Automatic* takes the first one added that is measuring (no reading in
  10 minutes, then the next one); a chosen sensor that briefly sends nothing shows `--`.

## Micro SD card (Waveshare)

With a micro SD card in the Waveshare's slot, Bosun stores much more than in its own memory.
Without a card everything works as normal; the card is an extra. Screenshot: `screenshot_sdkaart.png`.

| Folder on the card | What | How often |
|---|---|---|
| `bosun/metingen/2026-10-13.csv` | all values from all devices and NMEA 2000, one column per value | one line every minute, one file per day |
| `bosun/logboek/2026-10.txt` | the log, including what happened before a crash | continuously (every 2 s), one file per month |
| `bosun/backup/backup-2026-10-13.txt` | backup of devices, keys and settings | checked every hour, only on a change; the last 60 are kept |

- **Card:** micro SD up to 32 GB, formatted as **FAT32**. Larger cards are usually exFAT: format them as
  FAT32 first. The board never formats by itself. 16 GB is enough for decades of readings (about 0.2–0.6 MB per day, depending on the number of devices).
- **Readings (CSV):** separator `;` and, except in English, a decimal comma: the file opens directly in
  Excel, Numbers or LibreOffice. The header is in the chosen language. A column stays for the whole day (empty if a
  device briefly sends nothing); if a device is added, it continues in a new part (`_2`, `_3` ...).
  Only fresh values: Bluetooth from the last 5 minutes, NMEA 2000 from the last 30 seconds. Without a valid
  clock no readings are written (the file name is the date).
- **Log:** the board's memory only keeps the last 6 KB, and not after a power cut. On the card
  everything is kept. After a crash or hang the log from before the restart is also written to it.
- **WiFi page → Micro SD card:** download the files (newest first), and **restore** a backup from the card
  (tick box *sure*; the board then restarts). The diagnostics page also shows the card.
- **Card full:** if less than 5 % (or 100 MB) remains free, the oldest readings are removed automatically.
- **Only at startup:** the card is looked for when the board starts. Inserted the card while it is running?
  Press **RESET** (the middle button on the right-hand side, seen from the front); Settings points you to it.
  If the card is removed or fails during use, Bosun signs it off (orange in Settings) until the
  next start. Without a card it simply says *No card (not required)*.
- **Note:** the backups contain the Victron keys and the WiFi password. Whoever has the card has those too.
- The history on the screen (24 hours and 7 days) stays as it is; the card is for the long term.

## Clock (RTC)

**Own time**: the clock shows the time you set yourself (⚙ → *Set* at the clock). Summer and winter time change
automatically (European rules: last Sunday of March and October). With **Clock from GPS** the GPS keeps the minutes and
seconds exact, but the hour stays as you set it: if you set the clock to local time in England or Portugal,
the board remembers that difference (in quarter hours) and keeps it after every GPS synchronisation and restart.
Tip: set the clock after the GPS time has come in; then the UTC time from the GPS stays exact.

**Time from chart plotter**: many chart plotters put the local time on the NMEA 2000 network (PGN 129033, field
*Local Offset*: the offset from UTC as set on the plotter). If that gives a different time than Bosun,
it asks once: **Follow** or **Own time** (screenshot: `screenshot_tijd_plotter.png`). With *Follow*
the clock always runs in step with the plotter, even if you change it when crossing a border; after that it never asks again.
You can change this via ⚙ → *Time from chart plotter* (that line only appears once an offset has ever been received), and setting
the time yourself also turns following off.
Systems differ, which is why it is strict:
- Many devices send 129033 without an offset ("not available"; e.g. Furuno autopilots and standalone GPS units). Then
  nothing happens and your own time applies.
- Only with a valid date and time, an offset in whole quarter hours within ±14 hours, and the same value twice in a row.
- Multiple senders: the first one keeps applying as long as it transmits (otherwise the time would jump back and forth); if it is
  silent for 30 s, another one takes over.
- Whether summer and winter time are correct depends on the plotter: some brands do not adjust the offset automatically.
  If Bosun follows the plotter, it follows that error too; in that case choose *Own time*.
- *Clock from GPS* off = also take nothing from the plotter.

**Order the RTC cell too: strongly recommended.** The clock chip (PCF85063, address 0x51) is on the board, but only keeps
the time with a **rechargeable RTC cell** on the
2-pin connector printed **RTC** (next to the DIP switch; BAT1 on the schematic): an **ML2032, ML2020 or ML1220 with a
JST SH (1.0 mm) plug**, like the RTC battery for the Raspberry Pi 5. That plug type is confirmed for other Waveshare
ESP32-S3 boards, not yet for this one. Before plugging in, check that the red wire ends up at the **+** or **1** next to
the connector (pin 1 is plus according to the schematic); reversed, it can damage the clock chip. The **BAT** connector at the top is for a Li-ion battery, not for the clock. The board charges that cell
via a diode from 3.3 V: so do **not use an ordinary CR2032**. Without a cell the time is lost after every power cut, until
you set it again or the GPS time comes in.

- **Setting**: once, via ⚙ Settings → *Set* (scroll wheels for date and time). Dutch time;
  summer and winter time change automatically.
- **Kept**: with the cell the time is kept, even without power; the board recharges the cell.
- **Clock** in the header bar of the dashboard.
- **Cell flat or missing**: the settings page then shows in orange *"RTC battery flat? Set the time again"*.
- **Without the cell**: you can still set the time, but it is lost on a power cut (until the GPS time comes in).
  History and log times after a restart are then wrong too.

## Memory

| | Available | Used |
|---|---|---|
| Program (flash) | 6.25 MB per program slot (2 slots, for updates) | about 1.7 MB (27 %) |
| Files (flash, LittleFS) | 3.3 MB | about 10 KB history |
| Settings (flash, NVS) | 20 KB | about 3 KB: devices, settings, WiFi, 48 hours of barometer |
| PSRAM | 8 MB | frame buffer 0.45 MB + history and buffers; well over 7 MB free |
| Internal RAM | about 320 KB | about 84 KB fixed (device list, alerts and large work buffers are in PSRAM), plus Bluetooth, screen objects (about 15–35 KB, plus about 2–2.5 KB per device) and WiFi when it is on |

In ⚙ Settings → **Memory and storage** you can see on the board itself how much is free.
An SD card is not required; with a card (Waveshare) you also get readings, log and backups (see *Micro SD card*).

Devices are stored in NVS (flash) and are kept after a restart. You can add up to 24.
Each device costs about 2–2.5 KB of internal working memory on the screen; the list itself and the alerts (max. 64) are in PSRAM.

### Finding the Victron key
VictronConnect → open the device → ⚙️ → **Product info** → **turn on** *Instant readout via Bluetooth* →
**Show** at *Instant readout details* → copy the *Encryption key* (32 hex characters).
Paste it on the screen, or more easily: on your phone via the **WiFi page** (see *WiFi page* above).

## Placement

Bosun is meant for **indoors**: in the cabin or at the chart table. The screen is not waterproof and not
bright enough for direct sunlight (like most ESP32 screens). Indoors that is no problem, and Bluetooth there
usually has the best range to the Victron devices and sensors. At night the *Night* theme helps (red, dimmed); an
alarm with buzzer can be heard from your bunk too.

## Connecting (power supply)

- **7–36 V** on **VIN** (pin 1) and **GND** (pin 2) of the green screw terminal. So directly on the
  12 V or 24 V on-board supply, fused: a 1 A fuse, or a breaker on the switch panel if the wire suits it (see ASSEMBLY.md).
- USB-C is only for flashing and the serial log. The [assembly guide (ASSEMBLY.md)](ASSEMBLY.md) covers all connections, wire ferrules, the barometer sensor and mounting.

## Installing from the browser

The easiest way: open **[the Bosun installer](https://perjan-smul.github.io/bosun-boat-monitor/)** in Chrome or Edge on a computer, connect the board
with a USB-C data cable and click **Install Bosun**. No software to install and nothing to compile.
Choose *Erase device* on a first install; do not erase when updating, so your devices, keys and settings stay.
The same page has the update files (`.bin`) for updating over WiFi.

The page lives in `web/` and is published to GitHub Pages by `.github/workflows/installer.yml`, which builds the
firmware on every push to `main`. A tag `v…` also puts the update files on a GitHub release.

**Testing the installer privately:** every run also produces the complete page as a download. Open the run under
*Actions*, download the artifact **bosun-installer**, unzip it, run `python3 -m http.server 8000` in that folder and open
http://localhost:8000 in Chrome or Edge. The page then works exactly as online (browsers allow USB access on `localhost`).
GitHub Pages is only used once the repository is public.

## Building and flashing

With [PlatformIO](https://platformio.org/) (VS Code extension or CLI):

```bash
git clone https://github.com/Perjan-smul/bosun-boat-monitor
cd bosun-boat-monitor
pio run -t upload                          # build and flash
pio device monitor                         # serial log, 115200 baud
```

If flashing fails, hold **BOOT** down while you start the upload.
The Waveshare only has the ESP32-S3's own USB port; the serial log runs over it via USB-CDC.

The dependencies (LVGL 8.3.11, GFX Library for Arduino 1.4.9, NimBLE-Arduino 1.4.2) are downloaded
automatically. Uses the Arduino framework (ESP32 core 2.0.x via `espressif32@6.9.0`).

**Arduino IDE** works too: board *ESP32S3 Dev Module*, PSRAM *OPI PSRAM*, Flash *16MB*, install the three
libraries above, put `lv_conf.h` next to the `lvgl` library folder and copy `src/` into your sketch.
Also set *USB CDC On Boot* to *Enabled* for the serial log.

### Tests on a PC

The Bluetooth and NMEA 2000 decoders are tested on a PC, with real packets from the projects in
[THIRD_PARTY.md](THIRD_PARTY.md) (Victron, Ruuvi, ATC/pvvx, BTHome, ThermoBeacon, Mopeka and NMEA 2000
recordings). Needs a C++ compiler and OpenSSL (`libssl-dev`); GitHub runs them on every push.

```bash
make -C test/host
```

`scripts/n2k_compare.py` compares the NMEA 2000 decoder with [canboat](https://github.com/canboat/canboat) on
whole recordings: on all of canboat's sample recordings (over a million messages) the values match.

## Project structure

```
platformio.ini
include/lv_conf.h     LVGL settings (fonts, memory)
src/board.h           pins of the board + touch orientation
src/display.cpp       ST7701 RGB panel (init via SPI), LVGL binding
src/wsio.cpp          Waveshare helper chip (CH32V003 or TCA9554): resets, backlight, buzzer
src/gt911.h           touch driver
src/ble.cpp           continuous BLE scanning (NimBLE)
src/sensors.cpp       decoders: Victron (AES-CTR), Ruuvi, ATC/pvvx, BTHome
src/store.cpp         device list, found devices, storage in NVS
src/ui.cpp            screens: swipeable themed pages, scanning, add/edit, details, settings, alarm bar
src/alarms.cpp        orange/red alerts, hysteresis, acknowledging, buzzer pattern
src/buzzer.cpp        buzzer on the board (via the helper chip)
src/settings.cpp      settings in NVS
src/energy.cpp        adding up the energy flow (overview and history)
src/baro.cpp          barometer: source selection, 48-hour history, trend, saving
src/bme280.cpp        BME280/BMP280 air pressure sensor on I2C (calculations according to the datasheet)
src/sdcard.cpp        micro SD card: readings (CSV), log and backups, cleaning up when it gets full
src/datalog.cpp       columns and lines of the CSV files (testable on its own)
src/history.cpp       history: blocks aligned to the clock, ring buffers 24 h / 7 d, daily yield, storage on flash
src/clock.cpp         RTC (PCF85063), own time or time from the plotter, setting the time
src/ota.cpp           WiFi page and firmware update: web server, pairing code, confirmation
src/webkeys.cpp       WiFi page: entering Victron keys and adding devices
src/webdiag.cpp       WiFi page: diagnostics, log, backup and restore
src/log.cpp           log: serial port + ring buffer that survives a restart; counts failed starts
src/recovery.cpp      recovery screen after 3 failed starts: error code, log, WiFi page
src/i18n.cpp          languages: T("...") looks up the translation; choice kept in memory
src/i18n_table.cpp    translations (generated by scripts/i18n_gen.py from i18n/vertalingen.tsv)
src/fonts/            Montserrat with Western European characters (lv_font_conv)
src/psram.h           large work buffers in PSRAM instead of the scarce internal RAM
src/glow.c            glowing sparks of the overview (generated with scripts/glow_gen.py)
src/theme.cpp         colour themes (Ocean, Elegant, Light, Night) and the sun position for day/night
src/splash.c          startup screen: compass star, name and subtitle in the theme colour (generated with scripts/splash_gen.py)
src/version.h         firmware version number (startup screen and log)
web/                  browser installer page (ESP Web Tools), published to GitHub Pages
scripts/web_manifest.py  writes the installer manifests (version from src/version.h)
src/quarantine.cpp    disabling a device if the board keeps crashing because of it
src/ota_guard.cpp     rollback to the previous version on failed starts
src/sensors.cpp       decoders: Victron (incl. Smart Lithium, BatteryProtect, Lynx), Mopeka Pro, Ruuvi, ATC, BTHome, ThermoBeacon
src/n2k_decode.cpp    NMEA 2000 decoder (PGNs, fast packets), independent of the hardware
src/n2k.cpp           CAN (TWAI) listen-only, clock from GPS
test/host/            tests of the decoders on a PC (make -C test/host)
scripts/n2k_compare.py  NMEA 2000 decoder compared with canboat on recordings
```

## Troubleshooting

- **Waveshare: screen stays dark or touch does not respond**: look in the serial log for `[board]`. If it says
  *geen hulpchip gevonden* (no helper chip found), it is an older version (V1/V2 with I2C on different pins) that is not yet
  supported. Send in the log.

- **Touch is mirrored or rotated**: adjust `TOUCH_SWAP_XY` / `TOUCH_INVERT_X` / `TOUCH_INVERT_Y` in `src/board.h`.
- **Screen stays black**: check that PSRAM is enabled (`qio_opi`). The framebuffer is in PSRAM.
  Look in the serial log for "gfx->begin() mislukt" (failed) or "touchcontroller niet gevonden" (touch controller not found).
- **Victron shows "Wrong key"**: the key does not belong to this device. After a *reset* in VictronConnect the key changes too.
- **Victron shows "Key needed"**: tap the tile → Edit → enter the key.
- **No sensors found**: Instant readout is off (Victron), or the sensor is too far away or transmits encrypted (BTHome).
- **Adding another sensor**: write a decoder in `sensors.cpp` (`detectType` + `parseReading`) and a
  `SensorType` value. The interface then picks it up automatically.

## License

Bosun is released under the [MIT License](LICENSE): you may use, change and share it, also in your own products,
as long as the copyright notice and the licence text stay with it. It comes **without any warranty**: do not rely on it
as your only alarm for anything that matters for safety on board.
Bosun builds on other open-source work (fonts, LVGL, NimBLE and decoders from victron-ble, mopeka-iot-ble,
thermobeacon-ble and canboat); see [THIRD_PARTY.md](THIRD_PARTY.md).
