# Third-party notices

Bosun itself is released under the MIT License (see [LICENSE](LICENSE)). It builds on the work below.
Each part keeps its own licence; all of them allow use in an MIT-licensed project.

## Included in this repository

| Part | Where in Bosun | Source | Licence |
|---|---|---|---|
| Montserrat font | `src/fonts/bm_font_*.c` (converted to LVGL bitmaps), `src/splash.c` (wordmark) | Copyright 2011 The Montserrat Project Authors, shipped with LVGL (`scripts/built_in_font`) | SIL Open Font License 1.1 |
| Font Awesome Free icons | `src/fonts/bm_font_*.c` (symbols such as the weather and arrow icons) | Font Awesome Free by Fonticons, Inc., shipped with LVGL | Font files: SIL Open Font License 1.1; icons: CC BY 4.0 |
| LVGL configuration | `include/lv_conf.h` (based on `lv_conf_template.h`) | Copyright (c) 2021 LVGL Kft | MIT |

The SIL Open Font License allows the fonts to be embedded and redistributed with software; the font
bitmaps in this repository may not be sold on their own.

## Decoders based on other projects

The Bluetooth and NMEA 2000 decoders in `src/sensors.cpp` and `src/n2k_decode.cpp` were written for Bosun in C++,
using the field layouts, scaling factors and coefficients documented by these projects:

| Project | Used for | Licence |
|---|---|---|
| [victron-ble](https://github.com/keshavdv/victron-ble) | Victron Instant Readout record layouts (solar charger, battery monitor, Smart Lithium, Lynx Smart BMS, inverter, VE.Bus, Multi RS, Orion XS, DC energy meter, ...) | The Unlicense (public domain) |
| [mopeka-iot-ble](https://github.com/Bluetooth-Devices/mopeka-iot-ble) | Mopeka Pro packet layout and the per-medium sound-speed coefficients | MIT, Copyright (c) 2022 J. Nick Koston |
| [thermobeacon-ble](https://github.com/Bluetooth-Devices/thermobeacon-ble) | ThermoBeacon (ORIA and similar) packet layout | MIT, Copyright (c) 2022 J. Nick Koston |
| [canboat](https://github.com/canboat/canboat) | NMEA 2000 PGN field definitions, and the reference decoder used in the tests | Apache License 2.0, (C) 2009-2026 Kees Verruijt |

MIT licence text for the two Bluetooth-Devices projects (Copyright (c) 2022 J. Nick Koston): identical to the text in
[LICENSE](LICENSE), with that copyright line.

## Test data

The tests in `test/host/` use real packets and recordings from the test suites and sample files of these
projects, with the values those projects decode as the expected result:

| Project | Test data | Licence |
|---|---|---|
| [victron-ble](https://github.com/keshavdv/victron-ble) | Victron advertisements with their keys | The Unlicense (public domain) |
| [ruuvitag-sensor](https://github.com/ttu/ruuvitag-sensor) and the [Ruuvi data format 5 specification](https://docs.ruuvi.com/communication/bluetooth-advertisements/data-format-5-rawv2) | RuuviTag packets | MIT, Copyright (c) 2016 Tomi Tuhkanen |
| [ble_monitor](https://github.com/custom-components/ble_monitor) | ATC1441 and pvvx thermometer packets | MIT, Copyright (c) 2025 Custom components for Home Assistant |
| [bthome-ble](https://github.com/Bluetooth-Devices/bthome-ble) | BTHome v2 packets | MIT, Copyright (c) 2022 Ernst Klamer |
| [thermobeacon-ble](https://github.com/Bluetooth-Devices/thermobeacon-ble), [mopeka-iot-ble](https://github.com/Bluetooth-Devices/mopeka-iot-ble) | ThermoBeacon and Mopeka packets | MIT, Copyright (c) 2022 J. Nick Koston |
| [canboat](https://github.com/canboat/canboat) | NMEA 2000 messages from the recordings in `samples/` | Apache License 2.0, (C) 2009-2026 Kees Verruijt |

## Libraries downloaded at build time (not included)

| Library | Licence |
|---|---|
| [LVGL](https://github.com/lvgl/lvgl) 8.3 | MIT |
| [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX) 1.4.9 | BSD (Copyright (c) 2012 Adafruit Industries, and contributors) |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) 1.4.2 | Apache License 2.0 |
| [Arduino core for the ESP32](https://github.com/espressif/arduino-esp32) 2.0.x and ESP-IDF | LGPL 2.1 (Arduino core) and Apache License 2.0 (ESP-IDF) |

If you distribute a compiled firmware (`.bin`), include these notices with it.

## Trademarks

Victron Energy, Mopeka, ThermoBeacon, NMEA 2000 and the other brand names mentioned in the documentation are
trademarks of their owners. Bosun is not affiliated with or endorsed by them.
