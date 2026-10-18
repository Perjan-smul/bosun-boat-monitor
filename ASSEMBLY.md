# Bosun – assembly guide

How to wire and mount Bosun on the **Waveshare ESP32-S3-Touch-LCD-4**. Setting up sensors, alerts and WiFi is in the
[README](README.md); installing the firmware is easiest with the [browser installer](https://perjan-smul.github.io/bosun-boat-monitor/).

Work in this order: connect and test everything on the table first, only then mount it on board.
The drawings and positions are a guide: always follow the print on your own board.

## Overview

Bosun needs only **two wired connections**: 12 V power and the NMEA 2000 bus. Clock, buzzer and CAN are on the
board, and all other sensors are wireless via Bluetooth.

```
 12 V (fused 1 A) ───────┐
                         ├──► green 10-pin terminal ──► Waveshare ESP32-S3-Touch-LCD-4
 NMEA 2000 drop cable ───┘                                   ▲
                                                             │ Bluetooth
           Victron · Mopeka · Ruuvi · ThermoBeacon · Xiaomi ─┘
```

## Parts

Everything you need, with a search term for the web shop and an indicative price (October 2026). The full order list
with totals: [PARTS.md](PARTS.md) (Dutch: [BESTELLIJST.md](BESTELLIJST.md)).

| Part | Qty | Needed | Where it goes | Search term | Price (€) |
|---|---|---|---|---|---|
| **Waveshare ESP32-S3-Touch-LCD-4**, touch version (not the 4B) | 1 | yes | the screen; CAN, clock, buzzer and micro SD are on it | Waveshare ESP32-S3-Touch-LCD-4 | 35–55 |
| Rechargeable RTC cell ML2032 or ML2020 with JST SH plug (see *RTC* below) | 1 | **strongly recommended** | *RTC* connector; keeps the clock running without power | Raspberry Pi 5 RTC battery ML2032 | 5–8 |
| Fuse 1 A with holder | 1 | yes | in the plus wire, close to the battery or distribution panel | inline fuse holder 1A blade | 3–6 |
| Wire 1 mm² (red and black), heat-shrink | 1 set | yes | power | 1 mm² wire red black | 5–12 |
| **Wire ferrules 0.25–1.5 mm² with crimp tool** (set, with a few twin ferrules 2 × 0.5 mm²) | 1 | yes | on every wire in the screw terminal, see below | ferrule crimping tool kit | 20–30 |
| NMEA 2000 drop cable, M12 female, one open end | 1 | yes | from the bus to the board's CAN terminal | NMEA 2000 drop cable M12 female | 15–30 |
| NMEA 2000 T-piece | 1 | sometimes | only if the bus has no free connection | NMEA 2000 T-piece | 10–20 |
| USB-C data cable | 1 | yes | first firmware and serial log | USB-C data cable | 3–8 |
| **DFRobot Gravity BME280 (SEN0236)** | 1 | optional | barometer, on the I2C block (pins 7–10); locking plug, no soldering | DFRobot SEN0236 | 15–20 |
| Micro SD card, FAT32, 8–32 GB | 1 | optional | readings, log and backups | micro SD 16GB | 5–10 |
| 3D-printed enclosure (PETG or ASA) | 1 | optional | surface or flush mount, see [`enclosure/`](enclosure/) | PETG filament | 0–5 |

Bluetooth sensors (Victron, Mopeka, RuuviTag, ThermoBeacon, Xiaomi) are listed in
[COMPATIBILITY.md](COMPATIBILITY.md).

**Tools:** multimeter, small flat screwdriver (2–2.5 mm) for the screw terminal, wire stripper, crimp tool for
ferrules, USB-C data cable and a laptop for the first firmware. No soldering needed.

## The Waveshare: connections

Power and NMEA 2000 go to the **green 10-pin screw terminal** on the bottom edge. Pins count from left to right,
as printed on Rev4.0; check the print on your own board.

| Pin | Signal | Connect |
|---|---|---|
| 1 | VIN | + of the 12 V, via the 1 A fuse (7–36 V allowed) |
| 2 | GND | − of the 12 V |
| 3 | RS485 A | not used |
| 4 | RS485 B | not used |
| 5 | CANL | NMEA 2000 NET-L (blue) |
| 6 | CANH | NMEA 2000 NET-H (white) |
| 7 | I2C VCC | optional: red of the BME280 (SEN0236); I2C jumper on 3V3 |
| 8 | GND | NMEA 2000 NET-C (black), the GND in the I2C block; optionally also black of the BME280 |
| 9 | SDA | optional: green (SDA) of the BME280 |
| 10 | SCL | optional: blue (SCL) of the BME280 |

**Wire ferrules on every wire.** A boat vibrates: without a ferrule the screw flattens the thin copper strands, they
break and the wire comes loose. So crimp a ferrule onto every wire that goes into the terminal, using a crimp tool
(ordinary pliers do not press them properly). Choose a narrow ferrule with a short collar; the terminal is small.

| Wire | Cross-section | Ferrule |
|---|---|---|
| Power red and black | 1 mm² | 1.0 mm² |
| NMEA 2000 white, blue, black | about 0.35–0.5 mm² | 0.5 mm² |
| Gravity cable of the BME280 | about 0.1 mm² | 0.25 mm², fold the stripped wire double |
| Pin 8: black of NMEA 2000 + black of the BME280 | together | one twin ferrule 2 × 0.5 mm² |

**Black NMEA wire (NET-C):** the CAN block only has L and H, no GND of its own. Connect NET-C to the **GND in the I2C
block** (pin 8). The GND next to VIN (pin 2) is already used by the minus of the power supply; this way every terminal
screw holds one wire (or one twin ferrule). According to the schematic both GND pins are connected.

**Barometer (optional):** use the **DFRobot Gravity BME280 (SEN0236)**. The connector on the sensor side locks and does
not shake loose, and nothing needs soldering. Cut the black DuPont plug off the other end of the cable, strip the wires
5 mm and crimp on 0.25 mm² ferrules (fold the wire double). Red to VCC (pin 7), green to SDA (pin 9), blue to SCL
(pin 10), black together with the black NMEA wire to GND (pin 8, in one twin ferrule). Check the colours against the
print on the sensor. Set the *I2C* jumper next to the terminal to **3V3**. Fix the sensor with a small screw or
double-sided tape, a little away from the warm screen, and secure the cable with a cable tie. Do not use a loose
GY-BME280 with push-on pins: those shake loose on a boat. If you already have a RuuviTag or an NMEA 2000 weather
station, you do not need the sensor: they provide air pressure too.

Also on the board:

- **DIP switch 120R (right of the terminal):** set the CAN position to **off**. Bosun hangs on a drop cable, not at the
  end of the backbone.
- **RTC (2-pin, next to the DIP switch; BAT1 on the schematic):** the rechargeable RTC cell goes here. **Strongly
  recommended:** without it the time is lost after every power cut. Use a rechargeable **ML2032, ML2020 or ML1220**
  cell; the board charges it from about 3.0 V. Never use an ordinary CR2032. The connector is expected to be
  **JST SH 1.0 mm, 2-pin**, the same as the RTC battery for the Raspberry Pi 5 (which is an ML2020 or ML2032 with that
  plug); this is confirmed for other Waveshare ESP32-S3 boards but not yet for this one. **Check polarity before
  plugging in:** pin 1 is plus according to the schematic, so the red wire must end up at the **+** or **1** printed
  next to the connector. Reversed, it can damage the clock chip. The **BAT** connector at the top is for a Li-ion
  battery, not for the clock.
- **USB-C and BOOT:** for the first firmware and the serial log.
- **Micro SD slot (recommended):** a card up to 32 GB, formatted as FAT32. Every minute all readings (CSV), the log and
  automatic backups are written to it. Insert the card before mounting the board, or keep the slot reachable. The card
  is only detected at start-up: inserted later, press RESET (the middle button on the right).

Clock, buzzer and CAN are on the board. Hardware versions V4.0 and V3.0 are recognised at start-up. Mind the name: the
*Touch-LCD-4B* is a different board, without CAN and without a 12 V input.

## Power

The board runs directly on the 12 V on-board supply: no converter needed, only a 1 A fuse. The Waveshare accepts
7–36 V.

```
 battery + ── main switch ── fuse 1 A ── red 1 mm² ──► pin 1 VIN
 battery − ─────────────────────────────  black 1 mm² ──► pin 2 GND
```

1. Switch off the on-board supply (main switch, or remove the fuse of the circuit).
2. Measure with the multimeter where you tap off: 12–14.6 V is fine. If you measure 24 V or more, stop: that is not a
   12 V circuit.
3. Fit the fuse holder (1 A) in the plus, as close as possible to where you tap off. That way the wire to the board is
   protected too.
4. Run red (plus) and black (minus), 1 mm², to the board. Crimp ferrules onto them.
5. Red to VIN (pin 1), black to GND (pin 2) of the green terminal. Tighten the screws and give the wire a gentle tug.
6. Do not insert the fuse yet. That happens at the first start.

Preferably tap off from a circuit that is switched off with the main switch, or add a switch of its own, so the screen
is not on when the boat is left. If you want the monitor always on (for example as a watch in the harbour), tap off
after the battery switch and leave the fuse in.

Power from the NMEA 2000 bus (NET-S, red wire) is possible, but only if you know how much current the bus can still
supply. In that case do not also use the 12 V terminal.

## Connecting NMEA 2000

Bosun connects to the bus with a drop cable, like any other device. It only listens: it transmits nothing and cannot
disturb the bus, provided H and L are connected correctly.

An NMEA 2000 network is a continuous backbone with one terminator (120 Ω) at each end. Devices hang on it with a drop
cable of at most 6 m, via a T-piece.

```
 [120 Ω]──┬──────────┬──────────┬──[120 Ω]     backbone
          │          │          │
       plotter   instrument   T-piece
                                │  drop cable (max 6 m)
                                ▼
                        white → pin 6 CANH
                        blue  → pin 5 CANL
                        black → pin 8 GND
```

| M12 pin | Wire | Colour | Waveshare (pin) |
|---|---|---|---|
| 4 | NET-H | white | **CANH** (6) |
| 5 | NET-L | blue | **CANL** (5) |
| 3 | NET-C (minus) | black | **GND** (8) |
| 2 | NET-S (+12 V) | red | do not connect |
| 1 | shield | bare | do not connect; cut short and insulate |

1. Switch off the NMEA 2000 network.
2. Strip the open end of the drop cable, crimp 0.5 mm² ferrules onto white, blue and black, and connect them as in the
   table. Insulate red and the shield separately with heat-shrink.
3. Switch off the terminator on the board: the DIP switch 120R, position CAN. A drop cable must not have a terminator.
4. Plug the M12 connector into a free T-piece on the backbone. Tighten the coupling nut by hand.
5. Now, with the network off, measure between CANH and CANL: about **60 Ω** is right (two 120 Ω terminators).
   About 40 Ω means a third terminator, often the one on the board.

## Bluetooth sensors: placement

Bluetooth sensors need no wire to the monitor, but their range does decide where you can mount the screen. Bosun
reads up to 24 devices at the same time.

- **Test first, mount afterwards.** Hold the screen at the intended spot, tap ⚙ Settings → *Add device* and look at the
  signal strength of each sensor. Better than −80 dBm is plenty; around −90 dBm readings drop out now and then.
- **Metal and water dampen.** A steel bulkhead, an engine room with a metal hatch, a full water tank or a carbon hull
  weaken the signal strongly. Place the Victron devices in the engine room preferably so that they are not behind a
  closed metal hatch.
- **Victron:** turn on *Instant readout via Bluetooth* in VictronConnect. Without it the device sends no readings.
- **Thermometers** (Ruuvi, Xiaomi, ThermoBeacon): do not stick them directly onto metal or in a metal fridge
  compartment; a piece of plastic in between already helps.
- **Mopeka:** on the bottom of the tank, following Mopeka's instructions. The range is smaller than with Victron,
  because the sensor sits low.

## Mounting

The board is not waterproof: mount it indoors, in a dry place you can see from the helm or the chart table.

- **Moisture:** not under a hatch or porthole that might leak. Let the cables come in from below with a drip loop, so
  water running along the cable does not reach the terminals.
- **Heat and sun:** no direct sunlight on the screen and not above a heater or engine-room ventilation. Leave some air
  behind the board.
- **Antenna:** the Bluetooth and WiFi antenna is on the circuit board. Do not mount the board in a metal box or
  directly against a metal bulkhead.
- **Access:** make sure you can still reach the USB-C port and the BOOT button later, or make a small hatch. Updates
  normally go over WiFi, but if something goes wrong you need USB.
- **Cables:** secure the power wire and the drop cable with strain relief (cable tie to the box or bracket), so nothing
  pulls on the screw terminal. Keep the I2C cable short, preferably under 30 cm. A ferrule on every wire in the
  terminal.
- **Enclosure:** a 3D-printable enclosure is in [`enclosure/`](enclosure/): a surface-mount box for a wall, and a
  flush-mount version with a rounded flange for a panel cut-out. The large opening in the back plate means the cable
  entry in the wall does not have to line up exactly. Print in PETG or ASA. The board dimensions in the script are
  partly estimates: check them against your board before printing.

## First start and check

Do this on the table with a 12 V supply first, and once more on board. The firmware must already be installed
(see the [browser installer](https://perjan-smul.github.io/bosun-boat-monitor/)).

- [ ] Power: red on VIN, black on GND, tight, with ferrules.
- [ ] DIP switch 120R, position CAN off; rechargeable cell on the RTC connector, polarity checked.
- [ ] Fuse in: the screen starts within a few seconds.
- [ ] ⚙ Settings: the clock shows *RTC found: time is kept*.
- [ ] ⚙ Settings → *Test buzzer* → **Test**: the buzzer beeps.
- [ ] On board, NMEA 2000 network on: ⚙ Settings → *NMEA 2000* shows messages per second and the number of devices.
- [ ] Resistance between CANH and CANL, network off: about 60 Ω.
- [ ] Every Bluetooth sensor at its final spot: signal better than −80 dBm.
- [ ] Pull the fuse for 5 minutes and put it back: the clock is still right.
- [ ] With a micro SD card: ⚙ Settings → *Micro SD card* shows the free space; after a few minutes the WiFi page shows
  today's readings file under *Micro SD card*.

## Troubleshooting the connections

| What you see | Cause and fix |
|---|---|
| Screen stays black | Fuse, + and − swapped, or no voltage at the input. Measure on the board itself: 12–14.6 V. |
| Screen dark or touch does not work | Look in the serial log (USB-C) for `[board]`. *geen hulpchip gevonden* (no helper chip found) means an older hardware version (V1/V2) that is not supported yet: send in the log. |
| Touch is mirrored | Adjust `TOUCH_INVERT_X` / `TOUCH_INVERT_Y` in `src/board.h`. |
| Screen keeps restarting | Loose power wire or a wire that is too thin or long. Check the reason for the last start on the WiFi page under *Diagnostics*. |
| Clock wrong after a power cut, or *RTC battery flat? Set the time again* | No cell, a flat cell or a reversed plug on the RTC connector. |
| ⚙ Settings shows *No buzzer connected* | The buzzer is on the board, so the helper chip was not found; see above. |
| NMEA 2000: *No traffic: cable or CAN module?* | CANH and CANL swapped, GND not connected, or the network is off. |
| NMEA 2000 drops out now and then, other devices too | Wrong termination: measure between CANH and CANL. 40 Ω = one terminator too many; 120 Ω = one is missing. |
| Bluetooth sensor keeps dropping out | Signal weaker than −85 dBm: move the sensor or the screen, or remove metal in between. |
