# Bosun – bestellijst

*[English version: PARTS.md](PARTS.md)*

Alles wat je nodig hebt om Bosun te bouwen, met een zoekterm voor de webwinkel. Prijzen zijn indicatief (oktober 2026,
in euro, zonder verzendkosten); controleer de prijs in de winkel voordat je bestelt. Hoe je alles aansluit, staat in de
montagehandleiding en in [ASSEMBLY.md](ASSEMBLY.md) (Engels).

## Nodig

| Onderdeel | Aantal | Waarom | Zoekterm | Prijs (€) | Opmerking |
|---|---|---|---|---|---|
| **Waveshare ESP32-S3-Touch-LCD-4**, touchversie | 1 | het scherm met de ESP32-S3; CAN voor NMEA 2000, klok, zoemer en micro-SD zitten erop | Waveshare ESP32-S3-Touch-LCD-4 | 35–55 | Neem de touchversie. Niet de -4B: die heeft geen CAN en geen 12 V-ingang. |
| **Oplaadbare RTC-cel ML2032 of ML2020 met JST-SH-stekker** | 1 | houdt de klok gaande zonder stroom | Raspberry Pi 5 RTC battery ML2032 | 5–8 | **Sterk aangeraden:** zonder cel is de tijd na elke stroomuitval weg. Zoals de RTC-batterij voor de Raspberry Pi 5. Controleer vóór het insteken dat rood bij de + of 1 op het board uitkomt. Nooit een gewone CR2032. |
| Zekering 1 A met houder | 1 | beveiliging van de 12 V-voeding van het board | inline fuse holder 1A blade | 3–6 | Plaats dicht bij de accu. |
| Draad 1 mm², rood en zwart, met krimpkous | 1 set | voeding | draad 1 mm² rood zwart | 5–12 | |
| **Adereindhulzen 0,25–1,5 mm² met krimptang** | 1 set | op elke draad in de schroefklem: blijft vast bij trillingen | adereindhulzen set krimptang | 20–30 | Ook 0,25 mm² voor de dunne sensorkabel en een paar dubbele (twin) hulzen 2 × 0,5 mm². |
| NMEA 2000 drop-kabel, M12 female, één open einde | 1 | verbindt het board met de bus van je boot | NMEA 2000 drop cable M12 female | 15–30 | Lengte: tot aan de bus (max. 6 m). |
| USB-C-datakabel | 1 | eerste firmware en de seriële log | USB-C datakabel | 3–8 | Een kabel die data kan, geen alleen-laadkabel. |
| Multimeter | 1 | spanning, de bus, de afsluitweerstand (60 Ω) en de plus van de RTC-cel controleren | multimeter | 10–25 | Een gewone multimeter is genoeg. |

**Totaal nodig: ongeveer €96–174.**

## Soms nodig

| Onderdeel | Aantal | Waarom | Zoekterm | Prijs (€) | Opmerking |
|---|---|---|---|---|---|
| NMEA 2000 T-stuk | 1 | zodat je drop-kabel op de bus kan | NMEA 2000 T-piece | 10–20 | Alleen als er op de bus geen vrije aansluiting is. Maar één afsluiter per uiteinde van de bus. |

## Optioneel

| Onderdeel | Aantal | Waarom | Zoekterm | Prijs (€) | Opmerking |
|---|---|---|---|---|---|
| Micro-SD-kaart, FAT32, 8–32 GB | 1 | metingen, logboek en backups | micro SD 16GB | 5–10 | |
| **DFRobot Gravity BME280 (SEN0236)** | 1 | barometer en stormwaarschuwing (ook vocht en temperatuur) | DFRobot SEN0236 | 15–20 | Vergrendelende stekker, niets solderen. Dupont-stekkertje van de kabel knippen, adereindhulzen erop, in de I2C-klem. |
| Behuizing (3D-print) | 1 | opbouw tegen een wand, of inbouw in een schot | PETG filament | 0–5 | Ontwerp in [`enclosure/`](enclosure/); print in PETG of ASA (PLA wordt te zacht in de zon). |

## Draadloze sensoren (per stuk, zoveel als je wilt)

| Sensor | Waarom | Zoekterm | Prijs (€) | Opmerking |
|---|---|---|---|---|
| RuuviTag | temperatuur, vocht en luchtdruk (ook als barometer) | RuuviTag | 25–35 | Geen bedrading, geen sleutel. |
| ThermoBeacon-thermometer (ORIA, Brifit, SensorBlue) | temperatuur en vocht | ORIA thermometer hygrometer bluetooth | 8–15 | Werkt zo. |
| Xiaomi LYWSD03MMC | goedkope temperatuur en vocht | LYWSD03MMC | 8–12 | Eerst de pvvx-firmware erop zetten; zie [COMPATIBILITEIT.md](COMPATIBILITEIT.md). |
| Mopeka Pro Check | tankniveau door de tankwand: gas, water, diesel, vuilwater | Mopeka Pro Check | 45–70 | Kalibreren op vol en leeg. |
| Victron-apparaten met Bluetooth | accu, zon, laders | (heb je waarschijnlijk al) | | Sleutel uit VictronConnect. |

Alle ondersteunde sensoren en instrumenten: [COMPATIBILITEIT.md](COMPATIBILITEIT.md).
