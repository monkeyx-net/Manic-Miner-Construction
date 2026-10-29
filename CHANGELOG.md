# Manic Miner Construction Kit — Changelog

---
## Changelog

### v0.0.8 (2026-10-29)

#### Levels

- **41 new levels: Deeper and Down** — levels 21–61 are the extra caverns from the Sam Coupé version (*The Deeper Caverns* and *Down, Down, Deeper and Down*), imported from Byron's ZX Spectrum 128K conversion, *Manic Miner: Deeper and Down 128K v1.1*, built with JSWED 2.3.7 (JSW64). Graphics are 2× upscaled (Scale2x) from the Spectrum originals. The campaign is now 61 levels; victory follows level 61, *Up Up and Away*.

#### Game

- **Trampolines** — standing on a trampoline bounces the miner back up, as if jump were pressed.
- **Ramps** — the miner walks up and down ramp tiles, a whole cell at a time.
- **Switches that open walls** — a switch can clear wall cells and change how a guardian moves, or remove it (JSW64 switches, opening walls and triggers). Flipped switches are restored with save states.
- **Safe falls** — levels marked `safe_fall` have no fall-height death (JSW64 "free falls").
- **Open-edged rooms** — the miner can no longer leave the screen through gaps in a room's outer wall.
- **Levels with no items** start with the portal open.
- **High scores for every level** — per-level high scores are saved for all levels, not just the first 20, and the high-score screen shows 20 levels per page.
- **Title ticker** — now says 61 lethal caverns.

#### Display & Command Line

- **Windowed by default** — the game starts in a window at 3× scale; new `-f`/`--fullscreen` switch starts fullscreen. `--help` also lists the F11 / Alt+Enter toggle.
- **Resizable window** — the window can be resized (minimum 256×192); the picture stays integer-scaled.
- **Correct speed on high-refresh monitors** — game speed is now paced from the refresh rate of the monitor the window is on, not always the first monitor, so it no longer runs up to 2.4× too fast on a 120/144 Hz screen.

#### Level Format (`levels.json`)

- `levels[].info` may have 1–16 tile slots (was exactly 10); `gfx/sprites/tiles.png` is 16 slots wide.
- New tile types: `11` trampoline, `12` ramp rising left (`\`), `13` ramp rising right (`/`).
- Optional `levels[].switches` (`cell`, `open`, `npc`) and `levels[].safe_fall`.
- Optional `npcs[][].mirror`: `0` draws a walker's own left-facing frames 4–7 instead of mirroring frames 0–3.
- Up to 96 NPC sprites (was 29).

#### Level Editor

- The tile-slot palette shows as many slots as the level has; trampoline and ramp tile types added.
- NPC and sprite-designer pickers follow the number of sprites in `levels.json`.

#### Tools

- **`tools/jsw64_tap.py`** — reads a JSW64 128K `.tap`: `survey` writes a report and room previews, `dump` prints one room, and `import` appends its rooms to `levels.json` and the sprite sheets.

#### Fixes

- The Kong switch code ran on any level with a switch in cell 6 or 18; it now only runs on the two Kong levels.

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
