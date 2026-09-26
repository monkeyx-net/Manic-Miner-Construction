# Manic Miner Construction Kit — Changelog

---
## Changelog

### v0.0.6 (2026-05-27)

#### Command Line

- **`--help` and `--nosticks` switches** (#26) — `-h`/`--help` prints usage and exits; `-ns`/`--nosticks` disables game controller / joystick support.

#### Level Editor

- **Platform gap validation** (#24) — new "Gap check" toolbar toggle highlights gaps between platform segments wider than the maximum jumpable distance (2 tiles) with a semi-transparent orange-red striped overlay.
- **minerSprite round-trip fix** (#23) — `loadJSON`/`exportJSON` now preserve the `"minerSprite"` field introduced in #21, preventing it from being silently dropped on export.
- **Miner sprite in levels.json** (#21) — miner sprite data moved from a hardcoded array in `miner.c` to `"minerSprite"` in `levels.json`, following the same pattern as NPC sprites and making it fully editable.
