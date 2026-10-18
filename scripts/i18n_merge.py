#!/usr/bin/env python3
"""Vertalingen per taal (i18n/out_<taal>.tsv: nl + vertaling) samenvoegen in i18n/vertalingen.tsv.
Bestaande vertalingen blijven staan; nieuwe vullen aan. Daarna: python3 scripts/i18n_gen.py gen"""
import os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LANGS = ["en", "fr", "de", "es", "it", "pt", "sv", "nb", "da", "fi"]
TSV = os.path.join(ROOT, "i18n", "vertalingen.tsv")
rows, order = {}, []
if os.path.exists(TSV):
    for n, line in enumerate(open(TSV, encoding="utf-8")):
        if n == 0 or not line.strip(): continue
        c = line.rstrip("\n").split("\t"); c += [""] * (1 + len(LANGS) - len(c))
        rows[c[0]] = c[1:1 + len(LANGS)]; order.append(c[0])
for i, lang in enumerate(LANGS):
    p = os.path.join(ROOT, "i18n", "out_%s.tsv" % lang)
    if not os.path.exists(p): print("geen", p); continue
    n = 0
    for k, line in enumerate(open(p, encoding="utf-8")):
        if k == 0 or not line.strip(): continue
        c = line.rstrip("\n").split("\t")
        if len(c) < 2 or not c[1]: continue
        if c[0] not in rows: rows[c[0]] = [""] * len(LANGS); order.append(c[0])
        rows[c[0]][i] = c[1]; n += 1
    print("%s: %d vertalingen" % (lang, n))
with open(TSV, "w", encoding="utf-8") as f:
    f.write("nl\t" + "\t".join(LANGS) + "\n")
    for k in sorted(order):
        f.write(k + "\t" + "\t".join(rows[k]) + "\n")
print("geschreven:", TSV, len(order), "regels")
