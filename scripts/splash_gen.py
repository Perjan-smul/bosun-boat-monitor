#!/usr/bin/env python3
"""Maakt src/splash.c: het opstartscherm van Bosun (kompasster, naam, ondertitel) als A8-afbeeldingen.
De ring met de draaiende stip tekent LVGL zelf (lv_arc).

Alleen doorzichtigheid: de kleur komt uit het thema (img_recolor), zo past één set afbeeldingen bij elk thema.
Bij donkere thema's komt er een zachte gloed achter (aparte afbeelding; LVGL kan A8 niet vergroten, dus op ware grootte).
Lettertype: Montserrat Medium (zit bij LVGL: lvgl/scripts/built_in_font/Montserrat-Medium.ttf).
Gebruik: python3 scripts/splash_gen.py [pad/naar/Montserrat-Medium.ttf]
"""
import math
import os
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/Arduino/libraries/lvgl/scripts/built_in_font/Montserrat-Medium.ttf")
S = 4  # supersampling voor zachte randen

def down(img, w, h):
    return img.resize((w, h), Image.LANCZOS)

def mark(L):
    """Kleine kompasster: 4 lange en 4 korte punten, elk met een lichte en een donkere helft."""
    n = 2 * L + 6
    m = Image.new("L", (n * S, n * S), 0)
    d = ImageDraw.Draw(m)
    c = n * S / 2
    for ang, l, w in [(a, L, 5) for a in (0, 90, 180, 270)] + [(a, L * 0.45, 3) for a in (45, 135, 225, 315)]:
        r = math.radians(ang)
        tip = (c + math.sin(r) * l * S, c - math.cos(r) * l * S)
        p1 = (c + math.sin(r + math.pi / 2) * w * S, c - math.cos(r + math.pi / 2) * w * S)
        p2 = (c + math.sin(r - math.pi / 2) * w * S, c - math.cos(r - math.pi / 2) * w * S)
        d.polygon([p1, tip, (c, c)], fill=255)
        d.polygon([(c, c), tip, p2], fill=120)
    return down(m, n, n)

def text(s, size, track):
    f = ImageFont.truetype(FONT, size * S)
    tmp = ImageDraw.Draw(Image.new("L", (1, 1)))
    ws = [tmp.textlength(ch, font=f) for ch in s]
    w = int(sum(ws) + track * S * (len(s) - 1)) + 4 * S
    asc, desc = f.getmetrics()
    m = Image.new("L", (w, asc + desc), 0)
    d = ImageDraw.Draw(m)
    x = 2 * S
    for ch, cw in zip(s, ws):
        d.text((x, 0), ch, font=f, fill=255)
        x += cw + track * S
    m = m.crop((0, m.getbbox()[1] - S, w, m.getbbox()[3] + S))
    W, H = (m.width + S - 1) // S, (m.height + S - 1) // S
    return down(m.crop((0, 0, W * S, H * S)), W, H)

def halo(img, pad, radius, strength):
    """Zachte gloed rond een afbeelding (vervaagd)."""
    g = Image.new("L", (img.width + 2 * pad, img.height + 2 * pad), 0)
    g.paste(img, (pad, pad))
    return g.filter(ImageFilter.GaussianBlur(radius)).point(lambda v: min(255, int(v * strength)))

def carr(name, img):
    px = img.tobytes()
    body = ",\n  ".join(", ".join(f"0x{v:02x}" for v in px[i:i + 16]) for i in range(0, len(px), 16))
    return f"""static const uint8_t {name}_map[] = {{
  {body}
}};
const lv_img_dsc_t {name} = {{
  .header = {{.cf = LV_IMG_CF_ALPHA_8BIT, .always_zero = 0, .reserved = 0, .w = {img.width}, .h = {img.height}}},
  .data_size = {img.width * img.height},
  .data = {name}_map,
}};
"""

m = mark(34)
word = text("BOSUN", 52, 20)
sub = text("THE BOAT MONITOR", 13, 6)
parts = [
    ("splash_mark", m),
    ("splash_mark_glow", halo(m, 12, 5, 0.7)),
    ("splash_word", word),
    ("splash_word_glow", halo(word, 16, 7, 0.35)),
    ("splash_sub", sub),
]
out = """// Opstartscherm van Bosun (gemaakt met scripts/splash_gen.py).
// Alleen doorzichtigheid (A8): de kleur komt uit het thema (img_recolor). De *_glow-afbeeldingen
// (zachte gloed) worden alleen bij donkere thema's getoond.
#include <lvgl.h>

""" + "\n".join(carr(n, i) for n, i in parts)
open(os.path.join(HERE, "..", "src", "splash.c"), "w").write(out)
for n, i in parts:
    print(f"{n}: {i.width}x{i.height}")
