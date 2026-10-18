# Bosun enclosure (work in progress)

Parametric 3D-printable enclosure for the Waveshare ESP32-S3-Touch-LCD-4, made with
[CadQuery](https://cadquery.readthedocs.io/): a **surface-mount** box (wall, keyhole slots) and a
**flush-mount** version (only a rounded flange shows in front of the panel cut-out).

> The board dimensions in `bosun_case.py` are still **provisional** (102 × 102 mm). Measure your board and
> adjust the values at the top of the script before printing.

```bash
pip install cadquery
python3 enclosure/bosun_case.py      # writes STL files and a STEP file to enclosure/out/
```
