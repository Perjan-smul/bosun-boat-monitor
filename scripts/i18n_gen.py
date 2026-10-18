#!/usr/bin/env python3
"""Vertalingen voor Bosun.

  python3 scripts/i18n_gen.py check   -> lijst van teksten zonder vertaling (i18n/ontbreekt.tsv)
  python3 scripts/i18n_gen.py gen     -> src/i18n_table.cpp maken uit i18n/vertalingen.tsv

Teksten in de code staan tussen T("...") of TN("..."), in het Nederlands; dat is de sleutel.
i18n/vertalingen.tsv: kolommen nl, en, fr, de, es, it, pt, sv, nb, da, fi (tab-gescheiden; \n en \t als tekens geschreven).
De %-codes (printf) moeten in elke vertaling gelijk zijn en in dezelfde volgorde staan: dat wordt
gecontroleerd, want een verkeerde %-code kan het board laten crashen.
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")
TSV = os.path.join(ROOT, "i18n", "vertalingen.tsv")
MISSING = os.path.join(ROOT, "i18n", "ontbreekt.tsv")
OUT = os.path.join(SRC, "i18n_table.cpp")
LANGS = ["en", "fr", "de", "es", "it", "pt", "sv", "nb", "da", "fi"]  # zelfde volgorde als Lang in i18n.h (na NL)

# LVGL 8.3 symbolen (lv_symbol_def.h), voor teksten als T(LV_SYMBOL_OK "  Opslaan")
SYMBOLS = {
    "LV_SYMBOL_AUDIO": "\xEF\x80\x81",
    "LV_SYMBOL_BACKSPACE": "\xEF\x95\x9A",
    "LV_SYMBOL_BARS": "\xEF\x83\x89",
    "LV_SYMBOL_BATTERY_1": "\xEF\x89\x83",
    "LV_SYMBOL_BATTERY_2": "\xEF\x89\x82",
    "LV_SYMBOL_BATTERY_3": "\xEF\x89\x81",
    "LV_SYMBOL_BATTERY_EMPTY": "\xEF\x89\x84",
    "LV_SYMBOL_BATTERY_FULL": "\xEF\x89\x80",
    "LV_SYMBOL_BELL": "\xEF\x83\xB3",
    "LV_SYMBOL_BLUETOOTH": "\xEF\x8a\x93",
    "LV_SYMBOL_BULLET": "\xE2\x80\xA2",
    "LV_SYMBOL_CALL": "\xEF\x82\x95",
    "LV_SYMBOL_CHARGE": "\xEF\x83\xA7",
    "LV_SYMBOL_CLOSE": "\xEF\x80\x8D",
    "LV_SYMBOL_COPY": "\xEF\x83\x85",
    "LV_SYMBOL_CUT": "\xEF\x83\x84",
    "LV_SYMBOL_DIRECTORY": "\xEF\x81\xBB",
    "LV_SYMBOL_DOWN": "\xEF\x81\xB8",
    "LV_SYMBOL_DOWNLOAD": "\xEF\x80\x99",
    "LV_SYMBOL_DRIVE": "\xEF\x80\x9C",
    "LV_SYMBOL_DUMMY": "\xEF\xA3\xBF",
    "LV_SYMBOL_EDIT": "\xEF\x8C\x84",
    "LV_SYMBOL_EJECT": "\xEF\x81\x92",
    "LV_SYMBOL_ENVELOPE": "\xEF\x83\xA0",
    "LV_SYMBOL_EYE_CLOSE": "\xEF\x81\xB0",
    "LV_SYMBOL_EYE_OPEN": "\xEF\x81\xAE",
    "LV_SYMBOL_FILE": "\xEF\x85\x9B",
    "LV_SYMBOL_GPS": "\xEF\x84\xA4",
    "LV_SYMBOL_HOME": "\xEF\x80\x95",
    "LV_SYMBOL_IMAGE": "\xEF\x80\xBE",
    "LV_SYMBOL_KEYBOARD": "\xEF\x84\x9C",
    "LV_SYMBOL_LEFT": "\xEF\x81\x93",
    "LV_SYMBOL_LIST": "\xEF\x80\x8B",
    "LV_SYMBOL_LOOP": "\xEF\x81\xB9",
    "LV_SYMBOL_MINUS": "\xEF\x81\xA8",
    "LV_SYMBOL_MUTE": "\xEF\x80\xA6",
    "LV_SYMBOL_NEW_LINE": "\xEF\xA2\xA2",
    "LV_SYMBOL_NEXT": "\xEF\x81\x91",
    "LV_SYMBOL_OK": "\xEF\x80\x8C",
    "LV_SYMBOL_PASTE": "\xEF\x83\xAA",
    "LV_SYMBOL_PAUSE": "\xEF\x81\x8C",
    "LV_SYMBOL_PLAY": "\xEF\x81\x8B",
    "LV_SYMBOL_PLUS": "\xEF\x81\xA7",
    "LV_SYMBOL_POWER": "\xEF\x80\x91",
    "LV_SYMBOL_PREV": "\xEF\x81\x88",
    "LV_SYMBOL_REFRESH": "\xEF\x80\xA1",
    "LV_SYMBOL_RIGHT": "\xEF\x81\x94",
    "LV_SYMBOL_SAVE": "\xEF\x83\x87",
    "LV_SYMBOL_SD_CARD": "\xEF\x9F\x82",
    "LV_SYMBOL_SETTINGS": "\xEF\x80\x93",
    "LV_SYMBOL_SHUFFLE": "\xEF\x81\xB4",
    "LV_SYMBOL_STOP": "\xEF\x81\x8D",
    "LV_SYMBOL_TINT": "\xEF\x81\x83",
    "LV_SYMBOL_TRASH": "\xEF\x8B\xAD",
    "LV_SYMBOL_UP": "\xEF\x81\xB7",
    "LV_SYMBOL_UPLOAD": "\xEF\x82\x93",
    "LV_SYMBOL_USB": "\xEF\x8a\x87",
    "LV_SYMBOL_VIDEO": "\xEF\x80\x88",
    "LV_SYMBOL_VOLUME_MAX": "\xEF\x80\xA8",
    "LV_SYMBOL_VOLUME_MID": "\xEF\x80\xA7",
    "LV_SYMBOL_WARNING": "\xEF\x81\xB1",
    "LV_SYMBOL_WIFI": "\xEF\x87\xAB"
}

LIT = r'"(?:[^"\\]|\\.)*"'
PART = r'(?:' + LIT + r'|LV_SYMBOL_[A-Z_0-9]+)'
CALL = re.compile(r'\b(?:T|TN)\(\s*(' + PART + r'(?:\s*' + PART + r')*)\s*\)')

def c_unescape(s):
    out = bytearray()
    i = 0
    b = s.encode("utf-8")
    while i < len(b):
        c = b[i]
        if c == 0x5C:  # backslash
            n = chr(b[i + 1])
            if n == "x":
                j = i + 2
                while j < len(b) and chr(b[j]) in "0123456789abcdefABCDEF" and j < i + 4:
                    j += 1
                out.append(int(b[i + 2:j], 16)); i = j; continue
            out += {"n": b"\n", "t": b"\t", "\\": b"\\", '"': b'"', "'": b"'", "0": b"\0"}[n]
            i += 2
        else:
            out.append(c); i += 1
    return bytes(out)

def eval_parts(expr):
    res = b""
    for tok in re.findall(PART, expr):
        if tok.startswith("LV_SYMBOL_"):
            res += SYMBOLS[tok].encode("latin-1")  # "\xEF\x80\x8C" hierboven = losse bytes
        else:
            res += c_unescape(tok[1:-1])
    return res

def extract():
    keys = {}
    for f in sorted(os.listdir(SRC)):
        if not f.endswith((".cpp", ".h")) or f == "i18n_table.cpp" or f == "i18n.h":
            continue
        text = open(os.path.join(SRC, f), encoding="utf-8").read()
        for m in CALL.finditer(text):
            k = eval_parts(m.group(1))
            keys.setdefault(k, f)
    return keys

def tsv_unescape(s):
    return s.replace("\\n", "\n").replace("\\t", "\t").encode("utf-8")

def tsv_escape(b):
    return b.decode("utf-8").replace("\t", "\\t").replace("\n", "\\n")

def load_tsv():
    rows = {}
    if not os.path.exists(TSV):
        return rows
    for n, line in enumerate(open(TSV, encoding="utf-8")):
        line = line.rstrip("\n")
        if n == 0 or not line.strip():
            continue
        cols = line.split("\t")
        cols += [""] * (1 + len(LANGS) - len(cols))
        rows[tsv_unescape(cols[0])] = [tsv_unescape(c) if c else None for c in cols[1:1 + len(LANGS)]]
    return rows

SPEC = re.compile(rb'%(?:%|[-+ #0]*\d*(?:\.\d+)?(?:hh|h|ll|l|z)?[diouxXfFeEgGcsp])')
def specs(b):
    return [s for s in SPEC.findall(b) if s != b"%%"]

def c_literal(b):
    out = '"'
    for ch in b:
        if ch == 0x22: out += '\\"'
        elif ch == 0x5C: out += "\\\\"
        elif ch == 0x0A: out += "\\n"
        elif ch < 0x20 or ch >= 0x7F: out += '\\x%02X""' % ch  # "" voorkomt dat een volgend hexteken meetelt
        else: out += chr(ch)
    return out + '"'

def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "check"
    keys = extract()
    rows = load_tsv()
    missing = [k for k in keys if k not in rows or any(v is None for v in rows[k])]
    errors = []
    for k, vals in rows.items():
        for lang, v in zip(LANGS, vals):
            if v is not None and specs(v) != specs(k):
                errors.append("%%-codes verschillen (%s): %r -> %r" % (lang, k.decode(), v.decode()))
    unused = [k for k in rows if k not in keys]
    print("teksten in de code: %d, vertaald: %d, ontbreekt: %d, niet meer gebruikt: %d" %
          (len(keys), len(keys) - len(missing), len(missing), len(unused)))
    with open(MISSING, "w", encoding="utf-8") as f:
        f.write("nl\tbestand\n")
        for k in missing:
            f.write("%s\t%s\n" % (tsv_escape(k), keys[k]))
    for e in errors:
        print("FOUT:", e)
    if mode != "gen":
        return 1 if errors else 0
    if errors:
        print("niet gegenereerd: eerst de fouten oplossen")
        return 1
    used = sorted(k for k in keys if k in rows)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("// Gegenereerd door scripts/i18n_gen.py uit i18n/vertalingen.tsv: niet met de hand wijzigen.\n")
        f.write("#include <stddef.h>\n\n")
        f.write("extern const int I18N_COUNT = %d;\n" % len(used))
        f.write("extern const char *const I18N_KEYS[] = {\n")
        for k in used:
            f.write("    %s,\n" % c_literal(k))
        f.write("    nullptr,  // afsluiter (ook als er nog niets vertaald is)\n};\n")
        for i, lang in enumerate(LANGS):
            f.write("static const char *const L_%s[] = {\n" % lang.upper())
            for k in used:
                v = rows[k][i]
                f.write("    %s,\n" % (c_literal(v) if v is not None else "nullptr"))
            f.write("    nullptr,\n};\n")
        f.write("extern const char *const *const I18N_LANGS[] = {%s};\n" % ", ".join("L_" + l.upper() for l in LANGS))
    print("geschreven:", OUT)
    return 0

sys.exit(main())
