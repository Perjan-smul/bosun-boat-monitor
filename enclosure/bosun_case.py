#!/usr/bin/env python3
"""Behuizing voor Bosun op de Waveshare ESP32-S3-Touch-LCD-4 (CadQuery).

Twee versies met dezelfde achterplaat:
  - opbouw: complete doos tegen een wand (achterplaat met sleutelgaten)
  - inbouw: alleen een afgeronde rand zichtbaar, de doos zit achter een zaaggat in het schot

Alle maten in mm. Breedte en hoogte van het board komen van Waveshare; dikte, hoekradius en de plaats
van de USB-C zijn geschat: meet ze na op je eigen board (zie de README in deze map) en draai daarna:  python3 enclosure/bosun_case.py
Uitvoer: STL's om te printen en een STEP-bestand om verder te bewerken, in enclosure/out/.
"""
import os
import cadquery as cq

# ---------- Board ----------
# Buitenmaat volgens de productpagina van Waveshare (touchversie): 84,2 x 84,2 mm.
# Dikte, hoekradius en de plaats van de USB-C zijn nog geschat: meet ze na op je eigen board.
BOARD_W = 84.2       # breedte van board + glas
BOARD_H = 84.2       # hoogte van board + glas
BOARD_R = 3.0        # hoekradius van board/glas
STACK = 13.0         # dikte van glas tot achterkant hoogste onderdeel (schroefklemmen)
VIEW_DY = 0.0        # verschuiving van de opening t.o.v. het midden (+ = omhoog)
USB_X = 0.0          # positie USB-C langs de onderkant (+ = rechts, van voren gezien)
USB_Z = 9.0          # diepte van het midden van de USB-C, gemeten vanaf de voorkant van het glas

# ---------- Behuizing ----------
CLEAR = 0.4          # speling rond het board
WALL = 3.2           # wanddikte
FRONT = 1.6          # dikte van de lip over het glas
LIP = 2.5            # smalle lip over de rand van het glas; de rest van het glas blijft vrij
DEPTH = 30.0         # totale diepte van de opbouwdoos
DEPTH_FLUSH = 24.0   # diepte achter het schot bij inbouw
R_CORNER = 8.0       # afronding van de hoeken (van voren gezien); groter kan alleen met een dikkere wand
R_FRONT = 4.0        # afronding van de voorrand
R_OPEN = 1.5         # afronding van de beeldopening
PLATE_T = 2.4        # achterplaat
FLANGE = 10.0        # rand rondom bij inbouw
FLANGE_T = 4.0
CABLE_W = 52.0       # grote kabelopening in de achterplaat: de doorvoer in de wand hoeft niet precies
CABLE_H = 30.0       # mee te vallen (handig bij een bestaand gat van een oud scherm)
CABLE_Y = -10.0      # midden van de opening (+ = omhoog)
SCREW_PILOT = 2.5    # M3 zelftappend in de bussen
SCREW_HOLE = 3.4     # M3 door de achterplaat

IN_W = BOARD_W + 2 * CLEAR
IN_H = BOARD_H + 2 * CLEAR
OUT_W = IN_W + 2 * WALL
OUT_H = IN_H + 2 * WALL
OPEN = min(BOARD_W, BOARD_H) - 2 * LIP
RECESS = 1.2                     # sponning voor de achterplaat
BOSS_R = 3.6
BOSS_X = IN_W / 2 - BOSS_R + 0.2
BOSS_Y = IN_H / 2 - BOSS_R + 0.2
BOSSES = [(sx * BOSS_X, sy * BOSS_Y) for sx in (-1, 1) for sy in (-1, 1)]


def shell(depth, flange=False):
    """Voorkant + wanden; voorkant van het glas ligt op z = FRONT, de doos loopt naar +z."""
    body = cq.Workplane("XY").rect(OUT_W, OUT_H).extrude(depth).edges("|Z").fillet(R_CORNER)
    if flange:
        fl = (cq.Workplane("XY").rect(OUT_W + 2 * FLANGE, OUT_H + 2 * FLANGE).extrude(FLANGE_T)
              .edges("|Z").fillet(R_CORNER + FLANGE).faces("<Z").edges().fillet(R_FRONT - 2))
        body = body.union(fl)
    else:
        body = body.faces("<Z").edges().fillet(R_FRONT)
    # Binnenruimte van achteren, met een sponning voor de achterplaat
    body = body.cut(cq.Workplane("XY").workplane(offset=FRONT).rect(IN_W, IN_H).extrude(depth)
                    .edges("|Z").fillet(BOARD_R + CLEAR))
    body = body.cut(cq.Workplane("XY").workplane(offset=depth - PLATE_T).rect(IN_W + 2 * RECESS, IN_H + 2 * RECESS)
                    .extrude(PLATE_T).edges("|Z").fillet(BOARD_R + CLEAR + RECESS))
    # Bussen in de hoeken: drukken het board tegen de voorrand en houden de achterplaat vast
    z0 = FRONT + STACK
    for x, y in BOSSES:
        boss = cq.Workplane("XY").workplane(offset=z0).center(x, y).circle(BOSS_R).extrude(depth - PLATE_T - z0)
        body = body.union(boss)
        body = body.cut(cq.Workplane("XY").workplane(offset=depth - PLATE_T - 10).center(x, y)
                        .circle(SCREW_PILOT / 2).extrude(10))
    # Beeldopening met afgeronde rand
    cutter = (cq.Workplane("XY").workplane(offset=-1).center(0, VIEW_DY).rect(OPEN, OPEN).extrude(FRONT + 2)
              .edges("|Z").fillet(2.5))
    body = body.cut(cutter)
    body = body.faces("<Z").edges(cq.selectors.BoxSelector((-OPEN / 2 - 1, -OPEN / 2 - 1 + VIEW_DY, -0.1),
                                                           (OPEN / 2 + 1, OPEN / 2 + 1 + VIEW_DY, 0.1))).fillet(R_OPEN)
    # USB-C in de onderkant (alleen bij opbouw: bij inbouw zit die achter het schot en is bereikbaar)
    usb = (cq.Workplane("XZ").workplane(offset=OUT_H / 2 + 1).center(USB_X, FRONT + USB_Z)
           .rect(14, 8).extrude(WALL + 3).edges("|Y").fillet(2.5))
    body = body.cut(usb)
    return body


def back_plate(depth, keyholes=True):
    w, h = IN_W + 2 * RECESS - 0.3, IN_H + 2 * RECESS - 0.3
    p = (cq.Workplane("XY").workplane(offset=depth - PLATE_T).rect(w, h).extrude(PLATE_T)
         .edges("|Z").fillet(BOARD_R + CLEAR + RECESS - 0.15).faces(">Z").edges().fillet(0.8))
    pts = [(x, y) for x, y in BOSSES]
    p = p.faces(">Z").workplane().pushPoints(pts).cskHole(SCREW_HOLE, 6.4, 90)
    hole = (cq.Workplane("XY").workplane(offset=depth - PLATE_T - 1).center(0, CABLE_Y).rect(CABLE_W, CABLE_H)
            .extrude(PLATE_T + 2).edges("|Z").fillet(8.0))
    p = p.cut(hole)
    if keyholes:
        for x in (-24, 24):
            key = cq.Workplane("XY").workplane(offset=depth - PLATE_T - 1).center(x, 17).circle(4.2).extrude(PLATE_T + 2)
            slot = cq.Workplane("XY").workplane(offset=depth - PLATE_T - 1).center(x, 21).slot2D(12, 4.4, 90).extrude(PLATE_T + 2)
            p = p.cut(key).cut(slot)
    # Naam in de achterkant gegraveerd
    txt = (cq.Workplane("XY").workplane(offset=depth - 0.6).center(0, CABLE_Y - CABLE_H / 2 - 9)
           .text("BOSUN", 7, 1, kind="regular", halign="center", valign="center"))
    p = p.cut(txt)
    return p


def check():
    """Minimale wanddikte in de hoek (diagonaal): de hoek van het board mag niet door de wand steken."""
    c_out = OUT_W / 2 - R_CORNER
    c_in = IN_W / 2 - (BOARD_R + CLEAR)
    wall = R_CORNER - (BOARD_R + CLEAR + RECESS) - 2 ** 0.5 * (c_in - c_out)
    print(f"dunste wand in de hoek: {wall:.2f} mm")
    assert wall >= 1.2, "hoekradius te groot voor deze wanddikte"


def main():
    check()
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
    os.makedirs(out, exist_ok=True)
    parts = {
        "opbouw_voorkant": shell(DEPTH),
        "opbouw_achterplaat": back_plate(DEPTH),
        "inbouw_voorkant": shell(DEPTH_FLUSH, flange=True),
        "inbouw_achterplaat": back_plate(DEPTH_FLUSH, keyholes=False),
    }
    asm = cq.Assembly()
    for name, part in parts.items():
        cq.exporters.export(part, os.path.join(out, f"bosun_{name}.stl"), tolerance=0.05, angularTolerance=0.1)
        asm.add(part, name=name)
        print(f"bosun_{name}.stl")
    asm.save(os.path.join(out, "bosun_behuizing.step"))
    print(f"buitenmaat opbouw {OUT_W:.1f} x {OUT_H:.1f} x {DEPTH:.0f} mm; "
          f"inbouw: rand {OUT_W + 2 * FLANGE:.1f} mm, zaaggat {OUT_W + 1:.0f} x {OUT_H + 1:.0f} mm (hoeken R{R_CORNER:.0f})")


if __name__ == "__main__":
    main()
