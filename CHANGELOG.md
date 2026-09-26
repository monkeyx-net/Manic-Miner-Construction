# Manic Miner Construction Kit — Changelog

---
## Changelog

### v0.0.7 (2026-09-26)

#### Game

- **Title screen key hints** — the title ticker now lists the extra controls: `O` = Options, `L` = Load/Save, `U` = Save State, `R` = Start/Stop Recording.

#### Build & Release

- **CI build and release workflow** — new GitHub Actions workflow (`.github/workflows/build.yml`) checks `levels.json` formatting, builds Linux (x64/arm64), macOS (arm64), Windows (x64) and Web packages, and publishes a GitHub Release tagged from the top CHANGELOG version if that tag doesn't exist yet.
- **Assets in release packages** — Linux, macOS and Windows packages now include the `gfx/` and `sfx/` folders the game loads at runtime.
- **Windows DLL dependencies** — the Windows package now bundles every MinGW DLL `manicminer.exe` depends on (found with `ldd`), not just the four SDL2 DLLs, so it runs on a clean PC.

#### Assets

- **Unused assets moved out** — files the game and tools don't load (old `MANIC/` data, GIMP `.xcf` sources, spare sprite sheets, font project files, and unused music/sound tracks) moved from `gfx/` and `sfx/` to `gfx_none/` and `sfx_none/`, keeping them out of the packages and the web build.

#### Documentation

- **Licence and attribution** — added `LICENCE` (zlib) and a README credit to Steve Clark's SDL2 port ([fawtytoo/ManicMiner](https://github.com/fawtytoo/ManicMiner)) that this project builds on, plus expanded README content.

### v0.0.6 (2026-05-27)

#### Command Line

- **`--help` and `--nosticks` switches** (#26) — `-h`/`--help` prints usage and exits; `-ns`/`--nosticks` disables game controller / joystick support.

#### Level Editor

- **Platform gap validation** (#24) — new "Gap check" toolbar toggle highlights gaps between platform segments wider than the maximum jumpable distance (2 tiles) with a semi-transparent orange-red striped overlay.
- **minerSprite round-trip fix** (#23) — `loadJSON`/`exportJSON` now preserve the `"minerSprite"` field introduced in #21, preventing it from being silently dropped on export.
- **Miner sprite in levels.json** (#21) — miner sprite data moved from a hardcoded array in `miner.c` to `"minerSprite"` in `levels.json`, following the same pattern as NPC sprites and making it fully editable.
