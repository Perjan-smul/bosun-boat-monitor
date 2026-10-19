#!/usr/bin/env python3
"""Vergelijkt de NMEA 2000-decoder van Bosun met canboat op echte opnames.

  make -C test/host n2k_replay
  python3 scripts/n2k_compare.py <pad/naar/canboat/analyzer> opname.raw [meer opnames ...]

Opnames in het Actisense-tekstformaat of van Yacht Devices (zoals in canboat/samples). Voor elke boodschap die Bosun
verwerkt, worden de waarden vergeleken met die van canboat (analyzer -json -si -empty -nv).
Meldt: verschillende waarden, en waarden die Bosun toont terwijl canboat "niet beschikbaar" zegt.
Uitvoer per PGN: aantal vergeleken boodschappen en afwijkingen, met de eerste voorbeelden.
"""
import json, math, os, re, subprocess, sys
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPLAY = os.path.join(ROOT, "test", "host", "n2k_replay")

# Veld in Bosun -> veld in canboat, per PGN (zelfde naam tenzij hier anders)
RENAME = {
    130310: {"Pressure": "Atmospheric Pressure"},
    130311: {"Pressure": "Atmospheric Pressure"},
}
# Toegestane afwijking (SI-eenheden): afronding in Bosun of in canboat
TOL = {"Epoch": 1.0, "Latitude": 1e-7, "Longitude": 1e-7, "Local Offset": 0.5}


def norm_ts(ts):
    """Tijd zoals canboat hem schrijft: 2011-04-25-10:16:43.004 en 2011-04-25T10:16:43 -> 2011-04-25T10:16:43.004Z / ...43.000Z"""
    m = re.match(r"^(\d{4}-\d\d-\d\d)-(\d\d:\d\d:\d\d(?:\.\d+)?)$", ts)
    if m:
        ts = "%sT%s" % m.groups()
    if "T" not in ts:
        return ts + "Z" if re.match(r"^\d\d:\d\d:\d\d\.\d+$", ts) else ts
    ts = ts.rstrip("Z")
    if "." not in ts[-7:]:
        ts += ".000"
    return ts + "Z"


def canboat_value(v):
    if isinstance(v, dict):
        return v.get("value")
    return v


def epoch(fields):
    d, t = canboat_value(fields.get("Date")), canboat_value(fields.get("Time"))
    if d is None or t is None:
        return None
    return d * 86400 + int(t)


def compare(analyzer, path, stats, examples):
    with open(path, "rb") as f:
        raw = f.read()
    ref = subprocess.run([analyzer, "-json", "-si", "-empty", "-nv"], input=raw, capture_output=True).stdout
    bosun = subprocess.run([REPLAY, path], capture_output=True).stdout
    index = defaultdict(list)
    for line in ref.decode("utf-8", "replace").splitlines():
        try:
            m = json.loads(line)
        except ValueError:
            continue
        if "pgn" in m:
            index[(m["timestamp"], m["pgn"], m["src"])].append(m.get("fields") or {})
            # Yacht Devices-opnames hebben alleen een tijd; canboat zet er een datum voor
            index[(m["timestamp"][11:], m["pgn"], m["src"])].append(m.get("fields") or {})
    used = defaultdict(set)
    for line in bosun.decode().splitlines():
        b = json.loads(line)
        key = (norm_ts(b["timestamp"]), b["pgn"], b["src"])
        cands = index.get(key)
        pgn = b["pgn"]
        if not cands:
            stats[pgn]["unmatched"] += 1
            continue
        # Meerdere boodschappen met dezelfde tijd en afzender (bv. temperaturen van verschillende sensoren):
        # de best passende nog niet gebruikte nemen
        free = [i for i in range(len(cands)) if i not in used[key]] or list(range(len(cands)))
        best = min(free, key=lambda i: len(problems(pgn, b["fields"], cands[i])))
        used[key].add(best)
        stats[pgn]["compared"] += 1
        for name, problem in problems(pgn, b["fields"], cands[best]):
            stats[pgn]["differ"] += 1
            if len(examples[(pgn, name)]) < 3:
                examples[(pgn, name)].append("%s %s: %s" % (os.path.basename(path), b["timestamp"], problem))


def problems(pgn, fields, ref_fields):
    out = []
    for name, val in fields.items():
        cname = RENAME.get(pgn, {}).get(name, name)
        if name == "Epoch":
            rv = epoch(ref_fields)
        elif cname not in ref_fields:
            continue  # canboat kent dit veld anders of niet: niet vergelijken
        else:
            rv = canboat_value(ref_fields.get(cname))
        if rv is None:
            out.append((name, "Bosun %.10g, canboat: niet beschikbaar" % val))
            continue
        tol = TOL.get(name, max(1e-6, abs(rv) * 2e-4))
        diff = abs(val - rv)
        if "Angle" in name or name in ("COG", "Heading"):
            diff = min(diff, abs(diff - 2 * math.pi))  # 0 en 360 graden zijn gelijk
            tol = 2e-4
        if diff > tol:
            out.append((name, "Bosun %.10g, canboat %.10g" % (val, rv)))
    return out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    analyzer, files = sys.argv[1], sys.argv[2:]
    stats = defaultdict(Counter)
    examples = defaultdict(list)
    for path in files:
        compare(analyzer, path, stats, examples)
    bad = 0
    for pgn in sorted(stats):
        s = stats[pgn]
        print("%6d  vergeleken %6d  afwijkend %5d  niet gevonden %4d" % (pgn, s["compared"], s["differ"], s["unmatched"]))
        bad += s["differ"]
    for (pgn, name), ex in sorted(examples.items()):
        print("\n%d %s:" % (pgn, name))
        for e in ex:
            print("  " + e)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
