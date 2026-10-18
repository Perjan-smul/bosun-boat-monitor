#!/usr/bin/env python3
"""Maakt src/glow.c: de gloeiende stipjes van het overzicht (ster met stralen + witte kern), als A8-afbeeldingen."""
import os
import math
def sprite(n, f):
    return [[max(0,min(255,int(round(255*f(x-(n-1)/2, y-(n-1)/2))))) for x in range(n)] for y in range(n)]
def glow(x, y):
    r = math.hypot(x, y); a = math.atan2(y, x)
    halo = math.exp(-(r/4.2)**2) * 0.95 + math.exp(-(r/9.0)**2) * 0.35
    # 4 lange stralen (kruis) en 4 korte (diagonaal), uitlopend naar buiten
    ray4 = abs(math.cos(2*a)) ** 40 * math.exp(-r/6.5) * 0.85
    ray8 = abs(math.sin(2*a)) ** 40 * math.exp(-r/4.0) * 0.45
    v = halo + ray4 + ray8
    edge = max(0.0, 1 - max(0, r - 13) / 3)  # zacht tot de rand
    return min(1.0, v) * edge
def core(x, y):
    r = math.hypot(x, y)
    return 1.0 if r < 1.6 else max(0.0, math.exp(-((r-1.6)/1.6)**2))
def dot(x, y):
    r = math.hypot(x, y)
    return 1.0 if r < 3.2 else max(0.0, math.exp(-((r - 3.2) / 2.2) ** 2))
G, C, D = 32, 10, 16
g, c, dd = sprite(G, glow), sprite(C, core), sprite(D, dot)
def carr(name, n, px):
    flat = [v for row in px for v in row]
    body = ",\n  ".join(", ".join(f"0x{v:02x}" for v in flat[i:i+16]) for i in range(0, len(flat), 16))
    return f"""static const uint8_t {name}_map[] = {{
  {body}
}};
const lv_img_dsc_t {name} = {{
  .header = {{.cf = LV_IMG_CF_ALPHA_8BIT, .always_zero = 0, .reserved = 0, .w = {n}, .h = {n}}},
  .data_size = {n*n},
  .data = {name}_map,
}};
"""
out = """// Gloeiende stipjes voor de energiestroom op het overzicht (gemaakt met scripts/glow_gen.py).
// Alleen doorzichtigheid (A8): de kleur komt uit de stijl (img_recolor), zo is één afbeelding genoeg voor alle lijnen.
#include <lvgl.h>

""" + carr("glow_star", G, g) + "\n" + carr("glow_core", C, c) + "\n" + carr("glow_dot", D, dd)
open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "glow.c"), "w").write(out)
