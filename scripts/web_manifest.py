#!/usr/bin/env python3
"""Maakt de ESP Web Tools-manifesten voor de installatiepagina (web/manifest_<board>.json).

De versie komt uit src/version.h. De firmware-onderdelen staan per board in web/firmware/<board>/
(dat zet de GitHub Action daar neer na het bouwen met PlatformIO).
Gebruik: python3 scripts/web_manifest.py
"""
import json
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
BOARDS = {"waveshare_4": "Waveshare ESP32-S3-Touch-LCD-4"}
# Adressen in het flashgeheugen van de ESP32-S3 (Arduino-framework, zelfde indeling als bij pio run -t upload)
PARTS = [("bootloader.bin", 0x0), ("partitions.bin", 0x8000), ("boot_app0.bin", 0xE000), ("firmware.bin", 0x10000)]

version = re.search(r'BOSUN_VERSION\s+"([^"]+)"', open(os.path.join(ROOT, "src", "version.h")).read()).group(1)
for env, name in BOARDS.items():
    manifest = {
        "name": f"Bosun for {name}",
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [{
            "chipFamily": "ESP32-S3",
            "parts": [{"path": f"firmware/{env}/{f}", "offset": o} for f, o in PARTS],
        }],
    }
    with open(os.path.join(ROOT, "web", f"manifest_{env}.json"), "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    print(f"manifest_{env}.json: Bosun {version}")
