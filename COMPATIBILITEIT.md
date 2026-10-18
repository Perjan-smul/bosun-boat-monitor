# Compatibiliteit

*[English version: COMPATIBILITY.md](COMPATIBILITY.md)*

Welke boards, sensoren en instrumenten werken met Bosun. "Getest" betekent: de decoder is gecontroleerd
tegen echte pakketten of de open-source referentie (Victron, canboat, Home Assistant, ESPHome). Wat nog niet aan
boord op echte hardware is gezien, staat erbij.

**Snelle check voor een Bluetooth-sensor: welke app hoort erbij?**

| App van de fabrikant | Werkt? |
|---|---|
| VictronConnect | ja, met de sleutel uit de app |
| Ruuvi Station | ja |
| ThermoBeacon (ook ORIA, Brifit, SensorBlue) | ja |
| Shelly Smart Control (BLU-sensoren) | ja, als de versleuteling uit staat |
| Mopeka Tank Check | ja, de Pro-serie: gas, water, vuilwater, diesel |
| Mi Home / Xiaomi Home | alleen met andere firmware (zie *Xiaomi*) |
| Govee Home, Inkbird, SwitchBot, Tuya / Smart Life | nee (nog niet) |

## Boards

| Board | Status |
|---|---|
| **Waveshare ESP32-S3-Touch-LCD-4** (V3.0 en V4.0) | ondersteund; alles op het board: CAN, RTC, zoemer, micro-SD |
| LilyGO T-Panel S3 | niet meer ondersteund (alleen de Waveshare) |
| Waveshare ESP32-S3-Touch-LCD-4**B** | **nee**: geen CAN en geen 12 V-ingang |
| Guition ESP32-4848S040 | niet meer ondersteund |

## Bluetooth

Bluetooth-sensoren voeg je toe via ⚙ Instellingen → *Apparaat toevoegen*. Het board herkent het type vanzelf;
alleen apparaten die in deze lijst staan verschijnen bij het zoeken.

### Victron (sleutel nodig)

De sleutel staat in VictronConnect → apparaat → ⚙ → *Product info* → *Instant readout via Bluetooth*.

| Apparaat | Wat je ziet |
|---|---|
| SmartShunt, BMV-712 Smart | SoC, spanning, stroom, vermogen, resterende tijd, verbruikte Ah, startaccu of middenpunt of temperatuur, alarmen |
| Smart Battery Sense | spanning en temperatuur |
| SmartSolar en BlueSolar MPPT (met Bluetooth) | PV-vermogen, laadstroom, laadfase, opbrengst vandaag, fouten |
| Orion Smart DC-DC-lader | in- en uitgangsspanning, toestand, fouten |
| Blue Smart IP22 / IP65 / IP67-walstroomlader, Smart Skylla | vermogen, laadfase, spanning, laadstroom, temperatuur, AC-stroom, fouten |
| Smart Lithium (LiFePO4 Smart) | accuspanning, laagste en hoogste celspanning, temperatuur, balancer, fouten |
| Smart BatteryProtect | uitgang aan/uit, in- en uitgangsspanning, toestand, waarschuwingen, reden uit |
| Lynx Smart BMS | SoC, spanning, stroom, vermogen, resterende tijd, verbruikte Ah, temperatuur, BMS-fouten (telt als accumonitor) |
| MultiPlus, Quattro (VE.Bus, via de VE.Bus Smart dongle) | toestand, accuspanning en -stroom, actieve AC-ingang, AC-vermogen in en uit, temperatuur, SoC, VE.Bus-fout. Laadt hij van de wal, dan telt dat als **Wal** op het overzicht |
| Multi RS (Solar) | toestand, accuspanning en -stroom, AC-ingang, AC in/uit, PV-vermogen en opbrengst vandaag (telt als **Zon** en **Wal** op het overzicht), fouten |
| Phoenix-omvormer (Smart, of VE.Direct met Bluetooth) | schijnbaar vermogen (VA), toestand, accuspanning, AC-spanning en -stroom, alarmen |
| Orion XS DC-DC-lader | vermogen, toestand, in- en uitgangsspanning en -stroom, fouten |
| SmartShunt als DC-energiemeter (bv. voor een lader of omvormer) | vermogen, spanning, stroom, startaccu of temperatuur, alarmen |
| Overige Victron | herkend, alleen het signaal |

### Temperatuur en vochtigheid

| Sensor | Voorwaarde | Wat je ziet |
|---|---|---|
| **RuuviTag**, RuuviTag Pro | standaard (dataformaat 5) | temperatuur, vocht, **luchtdruk** (ook als barometer), batterij |
| **ThermoBeacon**-thermometers: **ORIA**, Brifit, SensorBlue en andere merken met de app *ThermoBeacon* | standaard | temperatuur, vocht, batterij |
| **Xiaomi** LYWSD03MMC, MHO-C401, MHO-C122; Qingping CGG1, CGDK2 | **andere firmware** (pvvx of ATC1441), formaat *Custom* of *ATC1441* | temperatuur, vocht, batterij |
| **BTHome v2**-sensoren: Shelly BLU H&T, sensoren met pvvx-firmware in BTHome-modus, ESPHome | **onversleuteld** (bij Shelly: versleuteling uit in de app) | temperatuur, vocht, luchtdruk, spanning, stroom en meer |

- **ThermoBeacon** (ORIA, Brifit): decoder volgens Home Assistant (*thermobeacon-ble*) en getest op de pakketten
  daaruit; nog niet aan boord gezien. Werkt niet met jouw exemplaar? Stuur het logboek op.
- **Xiaomi**: met de gewone firmware versleutelt Xiaomi de metingen. De open firmware van pvvx zet je er in een paar
  minuten op via de browser (zoek op *pvvx TelinkFlasher*). Daarna in de instellingen van de sensor het formaat
  *Custom* (pvvx), *ATC1441* of *BTHome* kiezen.

### Tanks

| Sensor | Wat je ziet |
|---|---|
| **Mopeka Pro Check** (gas), Pro Check H2O (water), Pro Plus, Pro Check Universal, Lippert BottleCheck | onder tegen de tank geplakt: niveau in % na kalibreren, temperatuur, batterij, meetkwaliteit |
| **Mopeka Pro-200, TD40, TD200** | bovenop de tank (meet door de lucht naar het oppervlak); verder idem |

Per sensor kies je de **inhoud**: gas, drinkwater, grijs-/vuilwater of diesel/benzine. Daarmee rekent Bosun
met de juiste geluidssnelheid (coëfficiënten van Mopeka, via Home Assistant *mopeka-iot-ble*). Bij grijs-/vuilwater
komt de melding als de tank **vol** raakt (standaard 75 % / 90 %), bij de rest als hij bijna **leeg** is.
Getest op de pakketten uit Home Assistant; de typen voor water en bovenop de tank zijn nog niet aan boord gezien.

De oudere Mopeka *Standard* (niet-Pro) werkt niet.

### Werkt niet (nog niet)

| Sensor | Waarom |
|---|---|
| Xiaomi met de gewone firmware (Mi Home) | versleuteld met een *bindkey*; zet pvvx-firmware erop |
| Govee (H5075, H5074, H5179, ...) | eigen formaat, nog geen decoder |
| Inkbird (IBS-TH1, IBS-TH2, ...) | eigen formaat, nog geen decoder |
| SwitchBot Meter | eigen formaat, nog geen decoder |
| BTHome **versleuteld** | sleutel wordt niet ondersteund; zet de versleuteling uit |
| Ruuvi Air (dataformaat 6) | nieuwer formaat, nog geen decoder |
| AIS, Bluetooth-dieptemeters | niet ondersteund |

### Op verzoek: komt erbij als iemand kan testen

Deze kunnen erbij, maar alleen samen met iemand die het apparaat heeft en een testversie wil proberen (zonder de
hardware kan het niet getest worden). Heb je er een? **[Vraag het hier aan](https://github.com/Perjan-smul/bosun-boat-monitor/issues/new?template=device-request.yml)** en vink aan dat je kunt testen.

| Apparaat | Wat Bosun zou tonen | Opmerking |
|---|---|---|
| **LiFePO4-accu's met een eigen BMS en Bluetooth-app**: JBD / Xiaoxiang (ook Overkill Solar en veel merkaccu's), JK BMS, Daly, LiTime / Ampere Time | SoC, spanning, stroom, vermogen, laagste en hoogste celspanning, temperatuur, laad-/ontlaadschakelaar, BMS-alarmen; net als een accumonitor, met dezelfde meldingen | Bosun moet verbinding maken met het BMS (het roept niets rond). Zolang Bosun verbonden is, kan de telefoon-app meestal niet verbinden; Bosun zou daarom elke 10–30 s kort verbinden. JBD is het eenvoudigst en komt het meest voor |
| Calypso ultrasone windmeter (draagbaar, Bluetooth) | windsnelheid en -richting | verbinding nodig, zoals bij het BMS |
| Govee-, Inkbird-, SwitchBot-thermometers | temperatuur, vocht | roepen rond; decoders bestaan in Home Assistant |
| Ruuvi Air | luchtkwaliteit | nieuwer dataformaat |
| Renogy- en Epever-laadregelaars | PV-vermogen, laadfase, opbrengst | via RS485 (het board heeft RS485 op de klem) |

Iets anders? Gebruik hetzelfde formulier: merk, type, de app die je gebruikt en, als je die kent, een open-sourceproject
dat het al uitleest. Met een decoder uit Home Assistant (*Bluetooth-Devices* op GitHub) of ESPHome is dat meestal
weinig werk.

## NMEA 2000

Via CAN (groene klem van de Waveshare). Alles wat deze PGN's stuurt, werkt, ongeacht het merk
(Garmin, Raymarine, B&G/Simrad/Lowrance, Furuno, Victron GX, Yacht Devices, ...).

| PGN | Inhoud |
|---|---|
| 126992, 129029 | datum en tijd (GPS) |
| 129033 | lokale tijd van de plotter (verschil met UTC) |
| 129025, 129029 | positie |
| 129026 | COG en SOG |
| 127250 | koers (kompas) |
| 128259 | snelheid door het water |
| 128267 | diepte |
| 130306 | wind (schijnbaar en waar) |
| 127488, 127489 | motor: toerental; temperatuur, oliedruk, dynamo, verbruik, draaiuren, belasting |
| 127505 | tankniveaus |
| 130312, 130316 | temperaturen |
| 130314, 130311, 130310 | luchtdruk (barometer) |
| 127508, 127506 | accu's: spanning, stroom, temperatuur, SoC, resterende tijd (bv. van een Victron GX, Mastervolt of een accumonitor op de bus) |
| 127245 | roerstand |
| 127257 | helling (rollen) en stampen |
| 130313 | luchtvochtigheid (binnen/buiten) |

Getest tegen *canboat* op zeven echte bootopnames: ± 770.000 berichten, waarvan ± 105.000 van de accu-, roer-, helling- en
vochtberichten, allemaal gelijk. Niet ondersteund: NMEA 0183, AIS-berichten, schakelpanelen (127501), en zenden: de
Bosun luistert alleen.

### Welk merk gebruikt welk systeem?

Veel merken geven NMEA 2000 een eigen naam of stekker; het signaal is hetzelfde. Een overzicht (algemeen, niet per
model gecontroleerd: kijk bij twijfel in de handleiding of op de achterkant van het apparaat).

**Instrumenten, plotters en stuurautomaten**

| Merk | Nu (± 2010 en later) | Ouder | Werkt met Bosun? |
|---|---|---|---|
| Raymarine | **SeaTalk NG** = NMEA 2000 met eigen stekkers (i70, p70, Axiom) | **SeaTalk 1** (ST40, ST60, ST4000), NMEA 0183 | NG: ja, met een verloopkabel. SeaTalk 1: alleen via de omzetter SeaTalk 1 → SeaTalk NG |
| Garmin | NMEA 2000 | NMEA 0183 | NMEA 2000: ja |
| B&G, Simrad, Lowrance | NMEA 2000 (Simrad: **SimNet**, ander stekkertje, zelfde signaal) | B&G **Fastnet**, LowranceNet, NMEA 0183 | modern: ja (SimNet met verloopkabel). Fastnet: nee |
| Furuno | NMEA 2000 (CAN-bus) | NMEA 0183 | NMEA 2000: ja |
| NASA Marine (Clipper) | NMEA 0183 | NMEA 0183 | nee (NMEA 0183 wordt niet ondersteund) |
| Tacktick (draadloos) | via een Raymarine-interface naar SeaTalk NG of NMEA 0183 | | via SeaTalk NG: ja |
| Humminbird | NMEA 0183, deels NMEA 2000 | NMEA 0183 | alleen de NMEA 2000-modellen |

**Marifoon en AIS** (Icom, Standard Horizon, em-trak, Vesper, Digital Yacht): meestal NMEA 0183, bij AIS vaak ook
NMEA 2000. Bosun gebruikt daarvan alleen de GPS-gegevens; AIS niet.

**Energie**

| Merk | Systeem | Werkt? |
|---|---|---|
| Victron | Bluetooth (Instant Readout), en VE.Can (NMEA 2000-compatibel) via een GX | ja, beide |
| Mastervolt | **MasterBus**: eigen systeem, géén NMEA 2000 | alleen via de Mastervolt-omzetter naar NMEA 2000 |
| Accu's met eigen app (JBD, JK, Daly, LiTime, ...) | eigen Bluetooth met verbinding | op verzoek, zie *Op verzoek* hierboven |

**Motoren**

| Merk | Systeem | Werkt? |
|---|---|---|
| Volvo Penta | **EVC**, eigen systeem | via een Volvo-gateway naar NMEA 2000 |
| Yanmar | eigen systeem | via een gateway naar NMEA 2000 |
| Mercury | **SmartCraft**, eigen systeem | via de VesselView Link (NMEA 2000) |
| Yamaha, Suzuki, Honda | eigen systeem | via een gateway naar NMEA 2000 |
| Oudere diesels (Vetus, Beta, Nanni, ...) | analoge meters (weerstandsgevers) | nee |

**Kortom:** NMEA 2000 dekt vrijwel alle moderne apparatuur, ook onder een andere naam (SeaTalk NG, SimNet, VE.Can) of
via een gateway. Ga je oude apparatuur vervangen, kies dan iets met NMEA 2000.

## Bedraad (I2C en kaart)

| Onderdeel | Board | Opmerking |
|---|---|---|
| **BME280** / BMP280 (luchtdruk), aanbevolen: **DFRobot Gravity BME280 (SEN0236)** | Waveshare | I2C, adres 0x76 of 0x77, vanzelf gevonden; klem `VCC GND SDA SCL`, jumper op 3V3. De SEN0236 heeft een vergrendelende stekker: niets solderen, trilt niet los |
| PCF85063-klok | Waveshare | op het board; **oplaadbare RTC-cel sterk aangeraden** (ML2032, ML2020 of ML1220 met JST-SH-stekker, zoals voor de Raspberry Pi 5) op de aansluiting *RTC*, geen CR2032 |
| micro-SD-kaart | Waveshare | FAT32, tot 32 GB; metingen, logboek en backups |
