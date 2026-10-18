# Bosun – the boat monitor

*ESP32-S3 touchscreen voor aan boord.* [English version: README.md](README.md)

Dashboard voor BLE-sensoren en NMEA 2000 aan boord, op een ESP32-S3 met 4" touchscreen (480×480).
Je voegt sensoren toe en beheert ze direct op het touchscreen, zonder dat je de code hoeft aan te passen.

## Boards

| Board | NMEA 2000 | Voeding | Klok | Zoemer | Build |
|---|---|---|---|---|---|
| **Waveshare ESP32-S3-Touch-LCD-4** (aanbevolen) | op het board | 7–36 V | op het board (PCF85063) | op het board | `waveshare_4` (standaard) |
| LilyGO T-Panel S3, CAN-versie H720 | op het board | 7–24 V | losse DS3231-module | via LCD-backpack | `t_panel_s3` |

De Waveshare heeft alles aan boord: je sluit alleen 12 V en de NMEA 2000-kabel aan. Let op de naam: de
**ESP32-S3-Touch-LCD-4B** is een ander board (audio, geen CAN, geen 12 V-ingang) en werkt niet zonder extra modules.
Van de Waveshare worden hardwareversie V4.0 (hulpchip CH32V003) en V3.0 (TCA9554) automatisch herkend.
Alleen de Waveshare is nog niet op een echt board getest; de pinnen komen uit Waveshare's eigen voorbeelden en het
V4.0-schema.

## Wat kan het

| Sensor | Wat je ziet | Opmerking |
|---|---|---|
| Victron SmartShunt / BMV | SoC, spanning, stroom, vermogen, resterende tijd, Ah, aux | sleutel nodig |
| Victron SmartSolar MPPT | PV-vermogen, laadstatus, accuspanning/-stroom, opbrengst vandaag | sleutel nodig |
| Victron Orion Smart DC-DC | status, in-/uitgangsspanning | sleutel nodig |
| Victron Blue Smart / Skylla walstroomlader | vermogen, laadfase, spanning/stroom, temperatuur | sleutel nodig |
| Victron Smart BatteryProtect | aan/uit-status, accuspanning, uitgangsspanning, alarm (lage spanning). Alarmbits geven een rode melding | sleutel nodig |
| Victron Lynx Smart BMS | SoC, spanning, stroom, vermogen, resterende tijd, verbruikte Ah, temperatuur (als accu-meter, dus op de accupagina) | sleutel nodig |
| Victron MultiPlus / Quattro (VE.Bus) en Multi RS | toestand, accu, AC-ingang, AC in/uit (Multi RS ook PV); laden van de wal telt als Wal op het overzicht | sleutel nodig |
| Victron Phoenix-omvormer, Orion XS, DC-energiemeter | vermogen, spanningen, stroom, toestand, fouten | sleutel nodig |
| RuuviTag (formaat 5) | temperatuur, vocht, luchtdruk, batterij | |
| Xiaomi LYWSD03MMC (ATC1441 / pvvx-firmware) | temperatuur, vocht, batterij | |
| BTHome v2 (onversleuteld) | temperatuur, vocht, druk, spanning, stroom, ... | o.a. Shelly BLU, pvvx in BTHome-modus |
| ThermoBeacon (ORIA, Brifit, SensorBlue) | temperatuur, vocht, batterij | |

De volledige lijst, ook wat (nog) niet werkt en hoe je het zelf nagaat: [COMPATIBILITEIT.md](COMPATIBILITEIT.md).

**Bediening**

Swipe links/rechts tussen de pagina's. De stipjes onderin laten zien waar je bent. Een pagina verschijnt
alleen als er apparaten van dat soort gekoppeld zijn:

| Pagina | Wanneer zichtbaar | Inhoud |
|---|---|---|
| **Overzicht** | MPPT, accumonitor of walstroomlader gekoppeld | Energiestroom met ringmeters: Zon, Wal, Accu en Verbruik rond een centrale hub met de laadfase (bv. Absorptie) of de resterende tijd. Bewegende gloeiende sterretjes in de kleur van de stroom tonen de richting (sneller = meer vermogen). Bovenaan een weerregel: temperatuur en vochtigheid van een klimaatsensor (te kiezen in de instellingen), en de luchtdruk met een weericoon (zon tot storm, naar de druk) en een trendpijl over 3 uur (oranje/rood bij stormwaarschuwing) |
| **Zonnepanelen** | MPPT gekoppeld | Totaal + per lader: PV-vermogen, gekleurde status (Bulk/Absorptie/Float), accuspanning, opbrengst vandaag |
| **Accu's** | SmartShunt / BMV / Battery Sense | SoC-boog, vermogen (groen = laden, oranje = ontladen), resterende tijd, temperatuur |
| **Klimaat** | Ruuvi / ATC / BTHome | Temperatuur, vocht, batterij |
| **Barometer** | BME280, NMEA 2000-luchtdruk of een Ruuvi | Luchtdruk, trend over 3 uur, verloop van 48 uur, stormwaarschuwing |
| **Laders & overig** | Orion DC-DC / overige Victron | Status en spanningen |
| **Apparaten** | altijd | Alle apparaten; tik op een tegel voor details, bewerken of verwijderen |

- **Toevoegen:** ⚙ Instellingen → bovenaan *Apparaat toevoegen* → **+** om te scannen (zonder apparaten staat de
  knop ook op de lege pagina). Gevonden sensoren verschijnen live met signaalsterkte.
  Tik er een aan, geef hem een naam en sla op. Bij Victron vul je ook de sleutel in (zie hieronder).
- **Status per apparaat:** groen = recent ontvangen, oranje = langer dan 30 s geleden, rood = langer dan 2 min geleden.
- **Verbruik in het overzicht** wordt berekend als: laadvermogen van MPPT's + walstroomladers − vermogen in/uit de accu (shunt).
  Zonder shunt is het verbruik onbekend.
- **Schaal van de ringen**: vol bij 400 W zon, 500 W verbruik en 1000 W wal (groeit mee als er meer gemeten wordt).
  Aanpassen in `src/ui.cpp` (`SCALE_SUN_W` e.d.). De accuring toont de SoC.
- **Scherm uit na 1 minuut** zonder aanraking (zacht uitfaden). Tik om het weer aan te zetten; die tik wordt niet als
  klik gezien. De tijd kies je in ⚙ Instellingen (nooit / 30 s / 1, 2, 5 of 10 min).

## Thema's (kleuren)

Vier kleurenschema's, te kiezen in ⚙ Instellingen → *Thema* (schermafbeelding: `screenshot_themas.png`):

| Thema | Kenmerk |
|---|---|
| **Oceaan** | donker blauwzwart met een groenblauw accent (standaard) |
| **Elegant** | echt zwart met blauwe accenten; ook de energiestroom op het overzicht in blauwtinten |
| **Licht** | lichte achtergrond en witte kaarten, voor overdag in de zon; strakke stipjes in plaats van gloeiende sterretjes |
| **Nacht** | alles in gedempt rood op zwart en de verlichting gedimd, zodat je nachtzicht behouden blijft; een rood alarm blijft fel |

**Automatisch dag/nacht** (schakelaar onder *Thema*): overdag het gekozen thema, na zonsondergang het *Thema 's nachts*
(standaard Nacht). Met een GPS op NMEA 2000 wisselt het op de echte zonnestand: donker zodra de zon meer dan 4° onder de
horizon staat (een kwartier tot half uur na zonsondergang), weer licht bij zonsopgang. De laatst bekende positie blijft
bewaard, ook als de GPS uit staat. Zonder GPS: vaste tijden, 21:00 tot 7:00. Het wisselen gebeurt alleen op het
dashboard, niet midden in het bewerken van een apparaat.

**Opstartscherm:** bij het aanzetten verschijnt *Bosun – The boat monitor* met een kompasster binnen een ring, waarover
een gloeiende stip draait zolang het board opstart (schermafbeelding: `screenshot_opstartscherm.png`). Het volgt het
thema, dus 's nachts meteen in rood. Het staat er minstens 1,5 seconde en faadt dan over in het overzicht.
De afbeeldingen maak je opnieuw met `scripts/splash_gen.py`.

## Talen

Nederlands, English, Français, Deutsch, Español, Italiano, Português, Svenska, Norsk, Dansk en Suomi: het scherm,
de meldingen, de wifi-pagina's en het herstelscherm.

- **Eerste start**: het board vraagt eerst de taal (tot dan Engels). Elke taal staat er in de taal zelf.
- **Later wijzigen**: ⚙ Instellingen → *Taal* → **Kiezen**. De keuze blijft bewaard en zit ook in de backup.
- Namen die je zelf geeft (bv. "Koelkast") blijven zoals je ze typt.
- Het logboek op de diagnosepagina blijft Nederlands (dat is voor het zoeken naar fouten).
- De klok toont de tijd die je zelf instelt, ook buiten Midden-Europa (zie *Klok*).

**Vertalingen aanpassen of een taal toevoegen** (voor ontwikkelaars):
- Teksten staan in de code in het Nederlands tussen `T("...")`; dat is de sleutel.
- De vertalingen staan in `i18n/vertalingen.tsv` (kolommen nl, en, fr, de, es, it, pt, sv, nb, da, fi).
- `python3 scripts/i18n_gen.py check` toont wat er nog ontbreekt (in `i18n/ontbreekt.tsv`);
  `python3 scripts/i18n_gen.py gen` maakt `src/i18n_table.cpp`. Het script weigert vertalingen waarvan de
  %-codes afwijken van het Nederlands, want dat kan het board laten crashen.
- Een nieuwe taal: achteraan toevoegen in `Lang` (`src/i18n.h`), in `i18n_name()` en in `LANGS` van beide scripts.
  Het lettertype (`src/fonts`, Montserrat) heeft alle West-Europese letters (Latin-1); andere schriften vragen
  een nieuw lettertype (lv_font_conv).

## Meldingen: waarschuwing (oranje) en alarm (rood)

Elke grens heeft twee niveaus. **Oranje** is een waarschuwing: het scherm gaat even aan, maar er is geen zoemer.
**Rood** is een alarm: de zoemer gaat af en het scherm blijft aan tot je op **Stil** tikt.

| Melding | Instellen | Standaard (oranje / rood) |
|---|---|---|
| Accu: SoC onder | per accu, bij *Bewerken* | 40 % / 25 % |
| Accu: spanning onder | per accu | uit / uit |
| Temperatuur boven | per thermometer, bv. koelkast 5 / 8 °C | uit / uit |
| Temperatuur onder | per thermometer, bv. kajuit 3 / 0 °C (vorst) | uit / uit |
| Geen signaal of geen geldige meting (bv. sleutel klopt niet meer) | ⚙ Instellingen: uit / 5 / 10 / 30 / 60 min | oranje na 10 min |
| Victron-alarm (lage spanning, oververhit, kortsluiting, ...) | ⚙ Instellingen: aan/uit | rood |
| Victron-laderfout (PV-spanning te hoog, lader te heet, ...) | ⚙ Instellingen: aan/uit | oranje |

**Wat je ziet en hoort**
- **Rand om het hele scherm** in de kleur van de zwaarste melding. Bij een onbevestigd alarm knippert de rode rand.
- **Pil in de kopbalk** met de belangrijkste melding (rood eerst) en `(+n)` als er meer zijn. Tik erop voor de lijst.
- **Tegel** van het apparaat krijgt een oranje of rode rand. De melding staat ook bovenaan het detailscherm.
- **Stil** (in de lijst) bevestigt de rode alarmen: de zoemer stopt, de rand stopt met knipperen en het scherm mag weer uit.
  De melding blijft zichtbaar zolang de oorzaak er is.
- **Oorzaak weg**: de melding verdwijnt. Grenzen hebben hysterese (SoC 2 %, spanning 0,2 V, temperatuur 0,5 °C).
  Een rood alarm dat terugzakt naar oranje en daarna opnieuw rood wordt, is weer onbevestigd.
- **Zoemer**: piept alleen bij rood, twee korte piepjes elke 2,5 s. Uitschakelen in ⚙ Instellingen → *Zoemer bij alarm*.

### Zoemer aansluiten (optioneel)

- **Waveshare**: de zoemer zit op het board. Niets aan te sluiten.

De T-Panel heeft geen vrije ESP32-S3-pin op een connector, maar wel twee 4-polige **I2C-poorten** (3V3, SDA, SCL, GND).
- **T-Panel**: gebruik een **I2C-LCD-backpack** (de PCF8574-printplaat voor 1602/2004-displays, ± €2). Het LCD zelf heb je niet nodig.
  - Sluit de backpack met 4 draden aan op een I2C-poort: **GND, VCC (3V3), SDA, SCL**.
  - Sluit een **actieve zoemer (3–5 V)** aan op de 16-polige LCD-header: **+ op pin 15** (A, achtergrondverlichting),
    **− op pin 16** (K). De transistor op de backpack schakelt dan de zoemer. Laat de jumper "LED" zitten.
  - Het adres staat standaard op **0x27** (PCF8574) of **0x3F** (PCF8574A). Beide worden **automatisch gevonden**.
    Verander de jumpers A0–A2 niet naar 0x20, want dat adres gebruikt het scherm.
  - Een kale PCF8574-module werkt ook: zoemer + op VCC, − op P0.
- **Ander board met een vrije GPIO**: zet `BUZZER_GPIO` in `src/board.h` (en `BUZZER_ACTIVE_LOW` naar je module).

In ⚙ Instellingen zie je of de zoemer gevonden is en kun je hem testen. Zonder zoemer werken de alarmen alleen op het scherm.

## NMEA 2000 (Waveshare, of de CAN-versie van de T-Panel, H720)

Bosun **luistert alleen** op de bus (250 kbit/s, listen-only). Hij zendt niets, claimt geen adres en kan de bus
dus nooit verstoren. Aan/uit en de busstatus (berichten/s, aantal apparaten) staan in ⚙ Instellingen.

| Pagina | Verschijnt als er ... binnenkomt | Inhoud |
|---|---|---|
| **Navigatie** | GPS, diepte, log, kompas of wind | diepte, SOG + snelheid door het water, COG + kompaskoers, schijnbare en ware wind, positie, temperaturen (zeewater groot) |
| **Motor** | motorgegevens (1 of 2 motoren) | toerental als meter, koelwater (oranje > 85 °C, rood > 95 °C), oliedruk, dynamo, verbruik, draaiuren, belasting |
| **Tanks** | tankniveaus | tot 12 tanks als balken: diesel, water, grijs water, vuilwater ... met % en liters. Rood bij bijna leeg (diesel/water) of bijna vol (afvalwater) |

**NMEA 2000-alarmen** (optioneel, **standaard uit**). Aanzetten in ⚙ Instellingen → *NMEA 2000-alarmen*. Met *Grenzen*
stel je per alarm een oranje en een rode grens in; een leeg veld zet dat alarm uit.

| Alarm | Oranje / rood (vooringevuld) |
|---|---|
| Diepte onder | 3,0 m / 2,0 m |
| Diesel onder | 25 % / 10 % |
| Water onder | 20 % / 10 % |
| Grijs- en vuilwater boven | 75 % / 90 % |
| Koelwater boven | 85 °C / 95 °C |
| Oliedruk onder (alleen als de motor draait, > 400 tpm) | leeg = uit (verschilt per motor) |

- **Meldingen**: werken net als de andere (balk bovenin, rand om het scherm, zoemer alleen bij rood, *Stil*).
- **Kleuren op de pagina's**: de diepte-kaart, de motorkaart en het tankpercentage krijgen de kleur van de melding.
- **Even geen gegevens** (bv. de dieptemeter valt weg): een bestaande melding blijft staan, in plaats van stilletjes te verdwijnen.
- **Motor uit**: de oliedrukmelding verdwijnt, omdat oliedruk dan geen betekenis heeft.

**Klok via GPS**: zodra er GPS-tijd op de bus is, wordt de klok gelijkgezet en de RTC meteen mee
(direct als de klok onbekend is, daarna hooguit eens per uur bij meer dan 2 s verschil). Uit te zetten in ⚙ Instellingen.

| PGN | Inhoud |
|---|---|
| 126992, 129029 | datum en tijd (GPS) |
| 129033 | lokale tijd van de plotter (verschil met UTC; zie *Klok*) |
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
| 127508, 127506 | accu's: spanning, stroom, temperatuur, SoC, resterende tijd (op de pagina Accu's) |
| 127245 | roerstand (kaart Roer op Navigatie) |
| 127257 | helling en stampen (kaart Helling op Navigatie) |
| 130313 | luchtvochtigheid (bij de temperaturen; ook in de weerregel van het overzicht als er geen Bluetooth-klimaatsensor is) |

De decoder is getest tegen **canboat** (de open-source referentie voor NMEA 2000), op zeven echte bootopnames
(± 770.000 berichten) en op echte CAN-frames met fast packets. Alle waarden kwamen overeen.

**Aansluiten**:
- **Waveshare**: de groene 10-polige schroefklem aan de onderrand (opdruk `VIN GND | A B | L H | VCC GND SDA SCL`):
  NET-H (wit) → **H** (pin 6), NET-L (blauw) → **L** (pin 5), NET-C (zwart) → **GND** in het I2C-blok (pin 8).
  Zet op de DIP-schakelaar **120R** de stand **CAN** uit: Bosun hangt aan een drop-kabel, niet aan het
  eind van de backbone.
- **T-Panel**: zie de klem op de CAN-module. NET-H (wit) → H, NET-L (blauw) → L, NET-C (zwart) → GND.
- Controleer daarna met de backbone aangesloten en het netwerk uit: **60 Ω** tussen H en L. Bij ± 40 Ω staat er een
  derde afsluiter bij (vaak die op het board); die moet uit.

## Bluetooth-apparaten: Smart Lithium en Mopeka

- **Victron Smart Lithium** (Smart LiFePO4 met BMS): de accu verschijnt op de pagina **Accu's** met de spanning,
  temperatuur, de laagste en hoogste celspanning, en de balancer-status (in balans / balanceert / onbalans).
  Net als bij de andere Victron-apparaten heb je de sleutel nodig (VictronConnect → Instant readout).
  Een Smart Lithium levert geen SoC, dus de boog toont de spanning.
- **Mopeka-tanksensoren** (Pro Check, H2O, Pro Plus, Universal onder tegen de tank; Pro-200, TD40, TD200 bovenop de
  tank): verschijnen op de pagina **Tanksensoren** met het niveau.
  - **Inhoud kiezen** (tegel → *Bewerken* → *Inhoud van de tank*): gas, drinkwater, grijs-/vuilwater of
    diesel/benzine. Geluid gaat in elke vloeistof anders snel; Bosun rekent met de coëfficiënten van Mopeka
    voor die inhoud. Sensoren bovenop de tank meten door de lucht en rekenen altijd met lucht.
  - **Kalibreren** (na het kiezen van de inhoud en opslaan): vul de afstand in mm in die de tegel toont als de tank
    **vol** is, en als hij **leeg** is. Zonder die twee waarden toont de tegel alleen de afstand.
  - Het niveau is lineair tussen die twee waarden. Onder de tank is "vol" de grootste afstand, bovenop de tank de
    kleinste; beide werken vanzelf.
  - De batterij, de temperatuur en de meetkwaliteit komen van de sensor zelf.
  - Bij een tank met een ongewone vorm (smal onder, breed boven) is het niveau in hoogte, niet in liters.
- **Meldingen** (per sensor in te stellen, oranje / rood, net als de andere):
  - **Tank leeg onder** (standaard 20 % / 10 %) voor gas, drinkwater en diesel; **Tank vol boven** (75 % / 90 %) voor
    grijs-/vuilwater. Alleen na kalibratie. Zonder meting blijft een bestaande melding staan.
  - **Batterij onder** (standaard 25 % / 10 %): de batterij van de sensor, een CR2032.
  - Oranje is een waarschuwing, rood een alarm met zoemer; de melding verschijnt in de balk en op de tegel.
- **Nog niet**: Mopeka Std (oudere CC2540-versie) en de Smart Lithium op de overzichtspagina.

## Wifi-pagina: Victron-sleutels en firmware-update

⚙ Instellingen → *Wifi-pagina* → **Open**. Kies bij **Verbinding**:

**Eigen wifipunt** (standaard, handig aan boord)
1. Zet **Wifi-pagina aan**. Het board maakt een wifi-netwerk `Bosun-XXXX` en toont het wachtwoord (8 cijfers) en een QR-code.
2. Scan de QR-code met de camera van je telefoon, of kies het netwerk in de wifi-instellingen en typ het wachtwoord.
3. De telefoon meldt "Aanmelden bij netwerk" en opent de pagina vanzelf. Gebeurt dat niet: open `http://192.168.4.1`.
   Er is geen extra inlog nodig.
- Zolang je telefoon met het board verbonden is, heeft hij via wifi geen internet. Mobiele data werkt meestal gewoon.
- Firmware uploaden: open `http://192.168.4.1` liever in de gewone browser dan in het aanmeldvenster (dat kan soms geen bestand kiezen).

**Bestaand netwerk** (boot-router, haven-wifi of telefoon-hotspot)
1. **Wifi-netwerk instellen**: vul de netwerknaam en het wachtwoord in. Die worden bewaard op het board.
2. Zet **Wifi-pagina aan**: het board verbindt met het netwerk en toont een adres (bv. `http://192.168.4.23`) en een **code van 8 cijfers**.
3. Open het adres op je telefoon, die op hetzelfde netwerk zit. Gebruikersnaam `boot`, wachtwoord = de code.

In beide gevallen kies je op de pagina **Victron-sleutels invoeren** of **Firmware-update**. De code is elke keer anders.

### Victron-sleutels invoeren

Handig: de sleutel kopiëren in VictronConnect en op dezelfde telefoon plakken, in plaats van 32 tekens overtypen.

- **Toegevoegd**: de Victron-apparaten die al op het scherm staan, met hun status (*Ontvangt metingen*, *Sleutel onjuist*, ...).
  Plak een nieuwe sleutel, of wijzig de naam. Een leeg sleutelveld laat de huidige sleutel staan.
- **Gevonden, nog niet toegevoegd**: Victron-apparaten die het board hoort. Vul naam en sleutel in en tik **Toevoegen**.
- Spaties en streepjes in de geplakte sleutel worden genegeerd. Tik na een paar seconden op **Vernieuwen** om de status te zien.
- Alarmgrenzen en het verwijderen van apparaten doe je op het scherm.

### Diagnose en logboek

Op je telefoon zie je wat het board doet, zonder laptop:
- **Board**: firmwareversie, hoe lang het draait, waarom het de laatste keer (her)startte (bv. *stroom aan*,
  *CRASH* of *WATCHDOG*), het aantal starts, het vrije geheugen, de opslag en de klok.
- **NMEA 2000**: berichten per seconde en het aantal apparaten op de bus.
- **Meldingen** die nu actief zijn.
- **Bluetooth**: per apparaat de status, de signaalsterkte en wanneer de laatste meting binnenkwam; ook de gevonden apparaten.
- **Logboek**: de laatste ± 100 regels met tijd. Na een crash of watchdog-herstart staat het logboek van vóór de
  herstart er nog (na een stroomuitval niet). Met *Logboek downloaden* krijg je het als tekstbestand.

### Backup van de instellingen

- **Backup downloaden**: alle apparaten met sleutels en grenzen, de instellingen en de wifi-instellingen in één tekstbestand.
  De historie zit er niet in, en de code van 8 cijfers ook niet (die is elke keer nieuw).
  Het bestand bevat de Victron-sleutels en, als je *Bestaand netwerk* gebruikt, het wachtwoord van dat wifi-netwerk:
  bewaar het veilig.
- **Terugzetten**: kies het bestand, vink aan dat je alles wilt vervangen, en tik op *Terugzetten*. Het board controleert
  het bestand eerst (bij een fout verandert er niets) en herstart daarna. Niet mogelijk met een onbevestigd alarm.

### Firmware-update

1. Kies het `.bin`-bestand en upload. Dat is `.pio/build/waveshare_4/firmware.bin` na `pio run`
   (T-Panel: `t_panel_s3`).
2. Op het scherm verschijnt **Nieuwe firmware installeren?**. Tik op **Installeer** om te herstarten, of op **Afwijzen**.
   Zonder antwoord binnen 60 s wordt het afgewezen.

**Waakhond**
- Loopt het programma (scherm en meldingen) meer dan 15 s vast, dan herstart het board zichzelf.
- Ook tijdens het opstarten: blijft het board in een fase langer dan 60 s hangen (bv. een vastgelopen I2C-bus naar de
  klok of het touchscreen), dan herstart het. Zonder dit zou het board voor altijd blijven hangen.
  In de diagnose staat dan *Laatste start: WATCHDOG*, en het logboek van vóór de herstart.
- **Apparaat uitschakelen na crashes**: crasht of hangt het board twee keer terwijl het een Bluetooth-pakket of
  NMEA 2000-bericht van hetzelfde apparaat verwerkt, dan wordt dat apparaat genegeerd. De rest werkt gewoon door.
  Je ziet een oranje melding (*… uitgeschakeld na crashes*) en de status *Uitgeschakeld (crashes)* op de tegel.
  Bij NMEA 2000 gaat het om één berichtsoort (PGN) van één apparaat op de bus.
  Weer inschakelen: ⚙ Instellingen, bovenaan, **Inschakelen**. Dit blijft bewaard, ook na stroom uit/aan.
  Valt het uitschakelen samen met de derde mislukte start, dan probeert het board eerst gewoon te starten zonder
  dat apparaat; pas als het dan nog crasht volgt het herstelscherm.
- Geen herstartlus: crasht of loopt het board **3 keer direct na elkaar** vast (binnen 2 minuten na het starten),
  dan start het daarna alleen het **herstelscherm**. Dat toont:
  - een **foutcode**: `C` = crash, `W` = vastgelopen, met het nummer van de fase waarin het misging
    (0 opstarten, 1 instellingen, 2 firmware-update, 3 apparatenlijst, 4 scherm, 5 klok, 6 historie,
    7 schermopbouw, 8 Bluetooth, 9 NMEA 2000, 10 in bedrijf, 11 SD-kaart). Bv. `C6` = crash tijdens de historie;
  - de laatste regels van het logboek;
  - de knop **Wifi-pagina aan**: daar download je het logboek (Diagnose), zet je een backup terug of installeer je
    een nieuwe firmware;
  - de knop **Opnieuw proberen**: gewoon opnieuw opstarten.

  Het herstelscherm slaat ook de controle na een firmware-update over, voor het geval die zelf de fout is
  (een mislukte update is dan al eerder teruggedraaid). Hangt het herstelscherm zelf, dan blijft het stilstaan met de
  foutcode in beeld in plaats van opnieuw te herstarten.
  In het herstelscherm zijn er geen meldingen, geen Bluetooth en geen NMEA 2000. Stroom uit en aan geeft ook weer een
  gewone start. Ging het mis bij het scherm zelf, dan gaat de wifi-pagina vanzelf aan (eigen wifipunt).
  Een te lage spanning (bv. bij het starten van de motor) telt niet als mislukte start.
- Elk uur komt het vrije geheugen in het logboek, zodat een geheugenlek over een lange tocht te zien is.

**Veiligheid**
- De pagina werkt alleen op het lokale netwerk en is alleen te gebruiken met de code op het scherm
  (bij het eigen wifipunt is de code het wifi-wachtwoord).
- Wifi staat **uit** tot je de wifi-pagina aanzet, en gaat zelf uit na een tijd zonder gebruik: instelbaar bij **Uit na** (5, 10, 30 of 60 min; standaard 10).
- Er wordt niets geüpload of geïnstalleerd als er een onbevestigd alarm is.
- Een bestand voor een ander board (bv. de T-Panel-build op een Waveshare) of iets anders dan Bosun-firmware wordt geweigerd.
- De nieuwe versie komt in de andere slot. Pas na **Installeer** start het board daarvan. Valt de stroom weg
  voordat je bevestigt, dan start gewoon de huidige versie.
- Start de nieuwe versie niet goed op (2 keer opnieuw opgestart binnen 60 s), dan gaat het board terug naar de vorige versie.
  Na 60 s zonder fouten geldt de nieuwe versie als goed. Een versie die wel start maar niet goed werkt
  (bv. geen beeld), wordt niet teruggedraaid: dan is USB nodig.
- Na een terugval leest de oude versie de instellingen en apparaten van de nieuwe versie gewoon in.
- De wifi-tekst wordt in het board bewaard zonder versleuteling. Zet het board niet op een netwerk dat je niet vertrouwt.

**Let op**
- Wifi en Bluetooth draaien tegelijk op het board. Tijdens een update kan de Bluetooth-scan even trager zijn.
- Het board moet voor de update in de haven of op het netwerk zijn. Zonder netwerk valt de wifi-pagina weg.

## Historie

De pagina **Historie** verschijnt zodra er een accu, MPPT of thermometer gekoppeld is.

| Grafiek | 24 uur | 7 dagen |
|---|---|---|
| **Accu** | SoC per 5 min | SoC per uur |
| **Energie** | zon, verbruik en accuvermogen (W) | idem per uur |
| **Zon/dag** | opbrengst per dag (kWh) van de laatste 7 dagen, met gemiddelde | |
| **Temp** | tot 6 thermometers | idem per uur |

- **Metingen**: elke 10 s, gemiddeld per 5 minuten en per uur.
- **Opslag zonder dataverlies**:
  - De historie (± 9,5 KB) wordt na elk 5-minutenblok en bij elke dagwissel opgeslagen.
  - Het lopende, half gevulde blok wordt elke minuut bewaard en na een herstart aangevuld.
  - Bij stroomuitval gaat hooguit de laatste minuut aan metingen verloren.
  - Elk bestand wordt eerst naar een tijdelijk bestand geschreven en dan in één keer hernoemd, met een CRC-controle.
    Valt de stroom tijdens het schrijven weg, dan blijft het vorige bestand heel; een beschadigd bestand wordt herkend.
  - Flashslijtage: ruim 50 jaar bij deze schrijffrequentie (wear-leveling over 3,4 MB).
- **Apparaten en instellingen** staan in NVS en worden direct bij elke wijziging opgeslagen.
- **Tijd-as**: met een klok (zie hieronder) staan er echte tijden en weekdagen op de assen. Na een stroomuitval
  krijgt het gat in de grafiek de juiste lengte. Zonder klok telt de as terug vanaf *nu*, en na een herstart
  staat er een gat van onbekende lengte.
- **Dagwissel**: die komt van de MPPT zelf. Victron zet de dagopbrengst 's ochtends op nul, en dat moment telt als nieuwe dag.

## Barometer en stormwaarschuwing

Een eigen pagina **Barometer** met de luchtdruk, de trend over 3 uur en het verloop van 48 uur
(schermafbeelding: `screenshot_barometer.png`). De pagina verschijnt zodra er een bron is:

| Bron (de eerste die er is) | Hoe |
|---|---|
| **BME280- of BMP280-module** (± €5) | op I2C: Waveshare-klem `VCC GND SDA SCL` (jumper I2C op **3V3**), T-Panel: een I2C-poort. Adres 0x76 of 0x77, vanzelf gevonden |
| **NMEA 2000** | een instrument dat luchtdruk stuurt (PGN 130314, 130311 of 130310), bv. een weerstation |
| **Bluetooth** | een sensor met luchtdruk: RuuviTag of BTHome |

- **Bij één bron blijven:** sensoren verschillen onderling een paar hPa. Bosun blijft bij de gekozen bron
  zolang die meet; valt hij weg en neemt een andere het over, dan begint het verloop opnieuw (anders zou een
  verschil tussen sensoren eruitzien als een drukval).
- **Meten:** elke 30 s; per 10 minuten een gemiddelde in het verloop. Het verloop van 48 uur blijft na een herstart
  bewaard (elke 30 minuten opgeslagen). Er is een geldige klok voor nodig.
- **Trend** volgens de WMO-indeling (verschil over 3 uur): stabiel (< 0,5 hPa), langzaam (tot 1,5), stijgt/daalt
  (tot 3,5), snel (tot 6) en zeer snel. De trend verschijnt na 3 uur meten.
- **Stormwaarschuwing:** daalt de druk in 3 uur meer dan de grens, dan **oranje**; 2 hPa meer en het wordt **rood**
  (met zoemer). Standaard 3 hPa (rood bij 5 hPa); in te stellen in ⚙ Instellingen → *Stormwaarschuwing*
  (uit, 2, 3, 4 of 5 hPa). Een vuistregel: 3 hPa in 3 uur betekent vaak harde wind, 5 hPa of meer storm.
- **BME280 inbouwen:** niet direct tegen het warme scherm; een stukje kabel ernaartussen. Op zee is de luchtdruk
  aan boord gelijk aan die op zeeniveau; op een hoog gelegen meer ligt de waarde lager, maar de trend klopt.
- Op de micro-SD-kaart komt de luchtdruk als kolom *Barometer hPa* in de metingen.
- **Op het Overzicht** staat rechtsboven de luchtdruk met een weericoon zoals op een ouderwetse barometer:
  zon (≥ 1022 hPa), zon met wolk (≥ 1009), wolk (≥ 1000), regen (≥ 990) en storm (lager). Een pijl toont de trend
  (↑ stijgt, → stabiel, ↓ daalt); bij een stormwaarschuwing kleurt de waarde oranje of rood. Links en in het midden
  staan temperatuur en vochtigheid van een klimaatsensor. Welke kies je in ⚙ Instellingen → *Sensor op overzicht*
  (verschijnt bij twee of meer sensoren). *Automatisch* neemt de eerst toegevoegde die meet (geen meting in
  10 minuten, dan de volgende); een gekozen sensor die even niets stuurt toont `--`.

## Micro-SD-kaart (Waveshare)

Met een micro-SD-kaart in het slot van de Waveshare bewaart Bosun veel meer dan in het eigen geheugen.
Zonder kaart werkt alles gewoon; de kaart is een aanvulling. Schermafbeelding: `screenshot_sdkaart.png`.

| Map op de kaart | Wat | Hoe vaak |
|---|---|---|
| `bosun/metingen/2026-10-13.csv` | alle waarden van alle apparaten en NMEA 2000, één kolom per waarde | elke minuut een regel, één bestand per dag |
| `bosun/logboek/2026-10.txt` | het logboek, ook wat er vóór een crash gebeurde | doorlopend (elke 2 s), één bestand per maand |
| `bosun/backup/backup-2026-10-13.txt` | backup van apparaten, sleutels en instellingen | elk uur gecontroleerd, alleen bij een wijziging; de laatste 60 blijven |

- **Kaart:** micro-SD tot 32 GB, geformatteerd als **FAT32**. Grotere kaarten zijn meestal exFAT: formatteer die eerst
  als FAT32. Het board formatteert nooit zelf. 16 GB is genoeg voor tientallen jaren aan metingen (± 0,2–0,6 MB per dag, afhankelijk van het aantal apparaten).
- **Metingen (CSV):** scheidingsteken `;` en, behalve in het Engels, een decimale komma: het bestand opent direct in
  Excel, Numbers of LibreOffice. De kop staat in de gekozen taal. Een kolom blijft de hele dag staan (leeg als een
  apparaat even niets stuurt); komt er een apparaat bij, dan gaat het verder in een nieuw deel (`_2`, `_3` ...).
  Alleen verse waarden: Bluetooth van de laatste 5 minuten, NMEA 2000 van de laatste 30 seconden. Zonder geldige
  klok worden er geen metingen geschreven (de bestandsnaam is de datum).
- **Logboek:** het geheugen van het board houdt alleen de laatste 6 KB, en niet na een stroomuitval. Op de kaart
  blijft alles. Na een crash of vastloper komt het logboek van vóór de herstart er ook op.
- **Wifi-pagina → Micro-SD-kaart:** de bestanden downloaden (nieuwste eerst), en een backup van de kaart
  **terugzetten** (vinkje *zeker*; het board herstart daarna). Ook de diagnosepagina toont de kaart.
- **Kaart vol:** blijft er minder dan 5 % (of 100 MB) vrij, dan verdwijnen de oudste metingen vanzelf.
- **Alleen bij het opstarten:** de kaart wordt gezocht als het board start. Kaart erin gedaan terwijl het draait?
  Druk op **RESET** (van voren gezien de middelste knop aan de rechterkant); Instellingen wijst ernaar.
  Gaat de kaart er tijdens gebruik uit of kapot, dan meldt Bosun hem af (oranje in Instellingen) tot de
  volgende start. Zonder kaart staat er gewoon *Geen kaart (niet nodig)*.
- **Let op:** de backups bevatten de Victron-sleutels en het wifi-wachtwoord. Wie de kaart heeft, heeft die ook.
- De historie op het scherm (24 uur en 7 dagen) blijft zoals hij is; de kaart is voor de lange termijn.

## Klok (RTC)

**Eigen tijd**: de klok toont de tijd die je zelf instelt (⚙ → *Instellen* bij de klok). Zomer- en wintertijd gaan
vanzelf (Europese regels: laatste zondag van maart en oktober). Met **Klok via GPS** houdt de GPS de minuten en
seconden precies, maar het uur blijft zoals jij het hebt ingesteld: zet je in Engeland of Portugal de klok op de
eigen tijd, dan onthoudt het board dat verschil (in kwartieren) en blijft het na elke GPS-synchronisatie en herstart.
Tip: stel de klok in nadat de GPS-tijd binnen is; dan blijft de UTC-tijd van de GPS exact.

**Tijd van de plotter**: veel kaartplotters zetten de lokale tijd op het NMEA 2000-netwerk (PGN 129033, veld
*Local Offset*: het verschil met UTC zoals op de plotter ingesteld). Geeft dat een andere tijd dan Bosun,
dan vraagt hij één keer: **Volgen** of **Eigen tijd** (schermafbeelding: `screenshot_tijd_plotter.png`). Bij *Volgen*
loopt de klok altijd gelijk met de plotter, ook als je die bij een grensovergang verzet; daarna nooit meer vragen.
Wijzigen kan via ⚙ → *Tijd van de plotter* (die regel verschijnt pas als er ooit een verschil is ontvangen), en zelf
de tijd instellen zet het volgen ook uit.
Systemen verschillen, daarom is het streng:
- Veel apparaten sturen 129033 zonder verschil ("niet beschikbaar"; bv. Furuno-autopiloten en losse GPS'en). Dan
  gebeurt er niets en geldt je eigen tijd.
- Alleen met geldige datum en tijd, een verschil in hele kwartieren binnen ±14 uur, en twee keer dezelfde waarde na elkaar.
- Meerdere afzenders: de eerste blijft gelden zolang die zendt (anders zou de tijd heen en weer springen); is hij
  30 s stil, dan neemt een andere het over.
- Of zomer- en wintertijd kloppen hangt van de plotter af: sommige merken passen het verschil niet vanzelf aan.
  Volgt Bosun de plotter, dan ook in die fout; kies dan *Eigen tijd*.
- *Klok via GPS* uit = ook niets van de plotter overnemen.

**Waveshare**: de klokchip (PCF85063, adres 0x51) zit op het board en heeft een **oplaadbare RTC-cel** nodig op de
2-polige aansluiting met de opdruk **RTC** (naast de DIP-schakelaar; BAT1 op het schema), als optie bij Waveshare
te bestellen of als ML1220 met draadjes. De aansluiting **BAT** bovenaan is voor een Li-ion-accu, niet voor de klok. Het board laadt die cel
via een diode vanaf 3,3 V: gebruik dus **geen gewone CR2032**. Zonder cel is de tijd na elke stroomuitval weg, tot
je hem opnieuw instelt of de GPS-tijd binnenkomt.

**T-Panel**: sluit een **DS3231-RTC-module** (± €3, met CR2032-knoopcel) aan op de tweede I2C-poort
(3V3, SDA, SCL, GND). Hij wordt automatisch gevonden (adres 0x68) en kan naast de zoemermodule zitten.
Zitten er beide klokchips, dan gaat de DS3231 voor.
- **Instellen**: eenmalig via ⚙ Instellingen → *Instellen* (draaiwieltjes voor datum en tijd). Nederlandse tijd;
  zomer- en wintertijd gaan vanzelf.
- **Bewaard**: de RTC bewaart de tijd jarenlang, ook zonder stroom. Nauwkeurig tot ± 1 minuut per jaar.
- **Klok** in de kopbalk van het dashboard.
- **Knoopcel leeg**: dan meldt de instellingenpagina in oranje *"RTC-batterij leeg? Stel de tijd opnieuw in"*.
- **Zonder RTC**: de tijd instellen kan ook, maar hij gaat verloren bij een stroomuitval.

> Let op: veel goedkope DS3231-modules (ZS-042) hebben een laadcircuit voor een oplaadbare LIR2032.
> Gebruik je een gewone **CR2032**, verwijder dan de diode of de 200 Ω-weerstand naast de batterijhouder.
> Anders probeert de module de niet-oplaadbare cel te laden.

## Geheugen

| | Beschikbaar | Gebruikt |
|---|---|---|
| Programma (flash) | 6,25 MB per programmaslot (2 slots, voor updates) | ± 1,7 MB (27 %) |
| Bestanden (flash, LittleFS) | 3,3 MB | ± 10 KB historie |
| Instellingen (flash, NVS) | 20 KB | ± 3 KB: apparaten, instellingen, wifi, 48 uur barometer |
| PSRAM | 8 MB | beeldbuffer 0,45 MB + historie en buffers; ruim 7 MB vrij |
| Intern RAM | ± 320 KB | ± 84 KB vast (apparatenlijst, meldingen en grote werkbuffers staan in PSRAM), plus Bluetooth, schermobjecten (± 15–35 KB, plus ± 2–2,5 KB per apparaat) en wifi als die aan staat |

In ⚙ Instellingen → **Geheugen en opslag** zie je op het board zelf hoeveel er vrij is.
Een SD-kaart is niet nodig; met een kaart (Waveshare) komen er metingen, logboek en backups bij (zie *Micro-SD-kaart*).

Apparaten worden in NVS (flash) opgeslagen en blijven na een herstart bewaard. Je kunt er maximaal 24 toevoegen.
Elk apparaat kost op het scherm ± 2–2,5 KB intern werkgeheugen; de lijst zelf en de meldingen (max. 64) staan in PSRAM.

### Victron-sleutel vinden
VictronConnect → apparaat openen → ⚙️ → **Product info** → *Instant readout via Bluetooth* **aanzetten** →
**Show** bij *Instant readout details* → kopieer de *Encryption key* (32 hex-tekens).
Plak hem op het scherm, of makkelijker: op je telefoon via de **wifi-pagina** (zie *Wifi-pagina* hierboven).

## Plaatsing

Bosun is bedoeld voor **binnen**: in de kajuit of bij de navigatietafel. Het scherm is niet waterdicht en niet
fel genoeg voor direct zonlicht (zoals de meeste ESP32-schermen). Binnen is dat geen bezwaar, en Bluetooth heeft daar
meestal het beste bereik naar de Victron-apparaten en sensoren. 's Nachts helpt het thema *Nacht* (rood, gedimd); een
alarm met zoemer is ook in de kooi te horen.

## Aansluiten (voeding)

- **Waveshare**: **7–36 V** op **VIN** (pin 1) en **GND** (pin 2) van de groene schroefklem. Dus direct op de
  12 V of 24 V boordspanning, gezekerd (bv. 1 A).
- **T-Panel**: **7–24 V** via de 2-polige schroefklem, gezekerd (bv. 1 A).
- USB-C is alleen voor flashen en de seriële log. De montagehandleiding heeft tekeningen van alle aansluitingen.

## Installeren vanuit de browser

De makkelijkste manier: open **[de Bosun-installatiepagina](https://perjan-smul.github.io/bosun-boat-monitor/)** in Chrome of Edge op een computer, kies je board,
sluit het aan met een USB-C-kabel voor data en klik op **Bosun installeren**. Niets installeren, niets compileren.
Kies bij de eerste installatie *Erase device*; wis bij een update niet, dan blijven je apparaten, sleutels en
instellingen staan. Op dezelfde pagina staan de updatebestanden (`.bin`) om via wifi bij te werken.

De pagina staat in `web/` en komt via `.github/workflows/installer.yml` op GitHub Pages; die bouwt bij elke push
naar `main` de firmware voor beide boards. Bij een tag `v…` komen de updatebestanden ook bij een release op GitHub.

## Bouwen en flashen

Met [PlatformIO](https://platformio.org/) (VS Code-extensie of CLI):

```bash
git clone https://github.com/Perjan-smul/bosun-boat-monitor
cd bosun-boat-monitor
pio run -t upload                          # Waveshare ESP32-S3-Touch-LCD-4 (standaard)
pio run -e t_panel_s3 -t upload            # LilyGO T-Panel S3
pio device monitor                         # seriële log, 115200 baud
```

Lukt flashen niet, houd dan **BOOT** ingedrukt terwijl je het uploaden start.
De Waveshare heeft alleen de USB-poort van de ESP32-S3 zelf; de seriële log loopt daar via USB-CDC.

De afhankelijkheden (LVGL 8.3.11, GFX Library for Arduino 1.4.9, NimBLE-Arduino 1.4.2) worden automatisch
gedownload. Gebruikt het Arduino-framework (ESP32 core 2.0.x via `espressif32@6.9.0`).

**Arduino IDE** kan ook: board *ESP32S3 Dev Module*, PSRAM *OPI PSRAM*, Flash *16MB*, installeer de drie
bibliotheken hierboven, zet `lv_conf.h` naast de `lvgl`-bibliotheekmap en kopieer `src/` naar je sketch.
Zonder extra instelling bouwt hij voor de Waveshare; zet dan ook *USB CDC On Boot* op *Enabled* voor de seriële log.
Voor de T-Panel zet je bovenaan `board.h` een `#define BOARD_T_PANEL_S3`.

## Projectstructuur

```
platformio.ini
include/lv_conf.h     LVGL-instellingen (fonts, geheugen)
src/board.h           pinnen per board + touch-oriëntatie
src/display.cpp       ST7701 RGB-paneel, init via XL9535 (T-Panel) of SPI (Waveshare), LVGL-koppeling
src/wsio.cpp          Waveshare-hulpchip (CH32V003 of TCA9554): resets, achtergrondverlichting, zoemer
src/cst3240.h         touchdriver T-Panel S3
src/gt911.h           touchdriver Waveshare
src/ble.cpp           continu BLE-scannen (NimBLE)
src/sensors.cpp       decoders: Victron (AES-CTR), Ruuvi, ATC/pvvx, BTHome
src/store.cpp         apparatenlijst, gevonden apparaten, opslag in NVS
src/ui.cpp            schermen: swipebare themapagina's, scannen, toevoegen/bewerken, details, instellingen, alarmbalk
src/alarms.cpp        meldingen oranje/rood, hysterese, bevestigen, zoemerpatroon
src/buzzer.cpp        zoemer op het board (Waveshare), via PCF8574 (automatisch gevonden) of GPIO
src/settings.cpp      instellingen in NVS
src/energy.cpp        energiestroom optellen (overzicht en historie)
src/baro.cpp          barometer: bronkeuze, verloop van 48 uur, trend, bewaren
src/bme280.cpp        BME280/BMP280-luchtdruksensor op I2C (rekenwerk volgens de datasheet)
src/sdcard.cpp        micro-SD-kaart: metingen (CSV), logboek en backups, opruimen als hij vol raakt
src/datalog.cpp       kolommen en regels van de CSV-bestanden (los te testen)
src/history.cpp       historie: blokken op de klok, ringbuffers 24 u / 7 d, dagopbrengst, opslag op flash
src/clock.cpp         RTC (DS3231 of PCF85063), eigen tijd of tijd van de plotter, tijd instellen
src/ota.cpp           wifi-pagina en firmware-update: webserver, pairingcode, bevestigen
src/webkeys.cpp       wifi-pagina: Victron-sleutels invoeren en apparaten toevoegen
src/webdiag.cpp       wifi-pagina: diagnose, logboek, backup en terugzetten
src/log.cpp           logboek: seriële poort + ringbuffer die een herstart overleeft; telt mislukte starts
src/recovery.cpp      herstelscherm na 3 mislukte starts: foutcode, logboek, wifi-pagina
src/i18n.cpp          talen: T("...") zoekt de vertaling op; keuze bewaard in het geheugen
src/i18n_table.cpp    vertalingen (gemaakt door scripts/i18n_gen.py uit i18n/vertalingen.tsv)
src/fonts/            Montserrat met West-Europese letters (lv_font_conv)
src/psram.h           grote werkbuffers in PSRAM in plaats van het krappe interne RAM
src/glow.c            gloeiende sterretjes van het overzicht (gemaakt met scripts/glow_gen.py)
src/theme.cpp         kleurthema's (Oceaan, Elegant, Licht, Nacht) en de zonnestand voor dag/nacht
src/splash.c          opstartscherm: kompasster, naam en ondertitel in de themakleur (gemaakt met scripts/splash_gen.py)
src/version.h         versienummer van de firmware (opstartscherm en logboek)
web/                  installatiepagina voor de browser (ESP Web Tools), komt op GitHub Pages
scripts/web_manifest.py  maakt de manifesten voor de installatiepagina (versie uit src/version.h)
src/quarantine.cpp    apparaat uitschakelen als het board er steeds door crasht
src/ota_guard.cpp     terugval naar de vorige versie bij mislukte starts
src/sensors.cpp       decoders: Victron (incl. Smart Lithium, BatteryProtect, Lynx), Mopeka Pro, Ruuvi, ATC, BTHome, ThermoBeacon
src/n2k_decode.cpp    NMEA 2000-decoder (PGN's, fast packets), los van de hardware
src/n2k.cpp           CAN (TWAI) alleen-luisteren, klok via GPS
```

## Problemen oplossen

- **Waveshare: scherm blijft donker of touch reageert niet**: kijk in de seriële log naar `[board]`. Staat er
  *geen hulpchip gevonden*, dan is het een oudere versie (V1/V2 met I2C op andere pinnen) die nog niet wordt
  ondersteund. Stuur de log op.

- **Touch zit gespiegeld of gedraaid**: pas `TOUCH_SWAP_XY` / `TOUCH_INVERT_X` / `TOUCH_INVERT_Y` aan in `src/board.h`.
- **Scherm blijft zwart**: controleer of PSRAM aanstaat (`qio_opi`). Het framebuffer staat in PSRAM.
  Kijk in de seriële log of er "gfx->begin() mislukt" of "touchcontroller niet gevonden" staat.
- **Kleuren kloppen niet (rood en blauw omgewisseld, T-Panel)**: zet in `display.cpp` MADCTL `0x36` op `0x00`.
- **Beeld flikkert of schuift (T-Panel)**: zet in `display.cpp` `pclk_active_neg` op `0`. Dit is de waarde die
  LilyGO's ESP-IDF-driver gebruikt; het Arduino-voorbeeld gebruikt `1`.
- **Victron toont "Sleutel onjuist"**: de sleutel hoort niet bij dit apparaat. Na een *reset* in VictronConnect verandert de sleutel ook.
- **Victron toont "Sleutel nodig"**: tik op de tegel → Bewerken → sleutel invullen.
- **Geen sensoren gevonden**: Instant readout staat uit (Victron), of de sensor is te ver weg of zendt versleuteld uit (BTHome).
- **Een andere sensor toevoegen**: schrijf een decoder in `sensors.cpp` (`detectType` + `parseReading`) en een
  `SensorType`-waarde. De interface pakt hem daarna automatisch op.

## Licentie

Bosun valt onder de [MIT-licentie](LICENSE): je mag het gebruiken, aanpassen en delen, ook in een eigen product,
zolang de copyrightregel en de licentietekst erbij blijven. Het komt **zonder enige garantie**: vertrouw er niet op als
enige alarm voor iets wat aan boord om veiligheid gaat.
Bosun bouwt voort op ander open-sourcewerk (lettertypen, LVGL, NimBLE en decoders van victron-ble, mopeka-iot-ble,
thermobeacon-ble en canboat); zie [THIRD_PARTY.md](THIRD_PARTY.md).
