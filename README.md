# MANIC MINER

## About

Originally written in 1983 by Matthew Smith. This port has been created to help people enjoy playing the game on modern platforms, and to have fun editing and creating their own levels.

Based on Steve Clark's SDL2 port, [fawtytoo/ManicMiner](https://github.com/fawtytoo/ManicMiner), and distributed under the same zlib licence (see [LICENCE](LICENCE)).

## Tech Specs

| Property | Value |
|----------|-------|
| Language | C (C99), JavaScript (ES modules) |
| Version | v0.0.6 |
| Renderer | SDL2 — hi-res 512×256 playfield (tiles and sprites, including the death, level-transition and pause screens); integer-scaled 256×192 low-res canvas for the title screen, menus and HUD |
| Display | Fullscreen-desktop default; windowed at 3× scale; F11 / Alt+Enter toggles |
| Frame rate | 60 fps tick rate; timer-gated main loop drives action → input → tick → draw |
| Colour palette | 16 colours (ZX Spectrum-inspired); per-pixel colouring eliminates colour clash |
| Audio | SDL_mixer at 22 050 Hz stereo; OGG music tracks; OGG SFX; synthesised square-wave miner notes (128 pitches, equal temperament) |
| Fonts | SDL_ttf — pixeldroid Console (small), PressStart2P (large), MANICMINER-Regular (title/extra-large) |
| Sprite format | 32×32 px hi-res sprites loaded from PNG (miner, NPCs, portals, boot); collisions use the 16×16 bitmaps from `levels.json` (16 rows of u16). Tiles are drawn from `gfx/sprites/tiles.png`: a row per level, a 16×16 cell per gfx slot; opaque pixels take the tile's ink, the rest its paper. the editor shows the same sheet (`make web` copies it to `web/tiles.png`) but does not edit it |
| Level format | JSON (`levels.json`) — tile grid, tile info, NPCs, miner start, portal data |
| Save format | Plain text key=value / CSV (`savestate_N.dat`, `gameconfig.dat`, `replay.dat`) |
| Platforms | Linux (x64, arm64), macOS (arm64), Windows x64 (MSYS2 MINGW64), WebAssembly (Emscripten) |
| Build | `make` (Linux/macOS), `make web` (Emscripten); CI via GitHub Actions |

See [CHANGELOG.md](CHANGELOG.md) for the history of changes.

---

## Controls

### Keyboard — In Game

| Key | Action |
|-----|--------|
| `←` / `→` | Move left / right |
| `Space` | Jump |
| `Pause` / `Tab` | Pause / unpause |
| `Alt` | Toggle in-game music on/off |
| `U` | Quick-save to next slot (cycles slots 1–5) — saves miner, level tiles, NPCs, scores |
| `R` | Toggle replay recording on/off |
| `Escape` | Abandon current game and return to title screen |
| `F11` / `Alt+Enter` | Toggle fullscreen / windowed |

### Keyboard — Title Screen

| Key | Action |
|-----|--------|
| `Enter` | Start new game |
| `L` | Open load-game slot menu |
| `S` | View per-level high scores |
| `O` | Open options menu |
| `Escape` | Quit |

### Keyboard — Options Menu

| Key | Action |
|-----|--------|
| `↑` / `↓` | Move between settings |
| `←` / `→` | Change selected value (lives / starting level) |
| `Enter` | Confirm and return to title |
| `Escape` | Cancel and return to title |

### Keyboard — Load Game Menu

| Key | Action |
|-----|--------|
| `↑` / `↓` | Move between save slots |
| `Enter` | Load selected slot |
| `Delete` | Erase selected slot |
| `Escape` | Return to title |

### Game Controller

Controllers are opened automatically on connect and fall back gracefully on disconnect.

| Button | Action |
|--------|--------|
| D-pad `←` / `→` | Move left / right |
| Left stick | Move (deadzone ±8 000) |
| D-pad `↑` / `↓` | Menu navigation |
| `A` / `B` | Jump |
| `A` / `Start` | Confirm / start game |
| `B` / `Back` | Escape / back |
| `X` / `LB` | Pause |
| `Y` / `RB` | Toggle music |

---

## Features

### High Scores

- A global high score is tracked for the current session and shown in the HUD.
- A **per-level high score** is stored for each of the 20 levels, persisted to `gameconfig.dat` between sessions.
- The per-level high score table is viewable from the title screen with `S`.
- The global high score updates live whenever the current score exceeds it.

### Save States

- Up to **5 save slots** (`savestate_1.dat` – `savestate_5.dat`).
- Press `U` during gameplay to save. The slot cycles automatically (1 → 2 → 3 → 4 → 5 → 1). A brief on-screen flash confirms the slot written.
- Saves capture: level number, lives, score, hi-score, air, all 512 tile states, miner physics state, all 8 NPC states, portal state, and Kong-fallen flag.
- Load a save from the title screen with `L`. Selecting a slot and pressing `Enter` restores the exact mid-level game state.
- Slots can be erased individually with `Delete` in the load menu.
- Starting level and number of lives are also persisted to `gameconfig.dat` via the Options menu.

### Replay Recording

- Press `R` during gameplay to start recording. A red **REC** indicator pulses in the HUD.
- Press `R` again to stop and save the replay to `replay.dat`.
- The replay captures full input (left/right/jump) every tick, plus a snapshot of the game state at the moment recording began (tiles, miner, NPCs, portal, scores, air).
- Replays can be played back from the Options menu (`O` on the title screen) if a replay file exists.
- Playback restores the exact starting snapshot and drives the miner entirely from the recorded input buffer; player input is ignored during playback.
- During playback the `R` key stops playback and discards the buffer.
- Leaving the game during recording (`Escape`) stops recording and saves the file automatically.

### Options Menu

Accessible from the title screen with `O`.

| Setting | Range | Notes |
|---------|-------|-------|
| Lives | 1–9 | Persisted to `gameconfig.dat` |
| Starting level | 0–(numLevels−1) | Level name shown below the selector |
| Play Replay | — | Visible only if a replay buffer is loaded |
| Delete Replay | — | Visible only if `replay.dat` exists on disk |

### Demo Mode

When the title screen is left idle, the game enters demo mode after the ticker text scrolls past. In demo mode the miner moves on its own and pressing any key returns to the title.

### Level Transition

When all items are collected and the portal is entered, remaining air is converted to score (8 points per unit, played back with the air-conversion sound). The per-level high score is updated and the next level begins.

### Game Over

When lives reach zero the game-over sequence plays: a boot falls and kicks the miner off the plinth with colour-cycling text, then returns to the title screen.

### Victory

Entering the portal on the final level triggers the victory sequence (swordfish animation), then wraps back to level 1.

---

## Command Line

```
manicminer [options]

  -h,  --help             Show help and exit
  -w,  --window           Start in windowed mode (default: fullscreen)
  -s,  --scale N          Set window scale factor 1–8 (default: 3, implies --window)
       --screenshot PATH  Save a PNG screenshot after 400 frames and exit (implies --window)
  -ns, --nosticks         Disable game controller / joystick support
```

---

## Level Editor

A browser-based level editor lives in `web/editor/`. Load it by serving the directory with `make serve` (starts `python3 -m http.server 8080`).

Features:
- Paint, erase, and flood-fill tile slots across the 32×16 tile grid.
- Per-slot tile type, paper and ink, previewed with the tile art from `tiles.png` (edit the art itself in an image editor).
- Tile type assignment (space, solid, floor, item, collapse, conveyor, harm, void, switch).
- NPC placement with patrol range, sprite, speed, and ink editing.
- Portal and miner-start placement.
- Sprite designer for NPC sprites (palette-indexed 16×16, 8 frames per type, 29 types).
- Import / export `levels.json`.
- Export `miner.png` and `npcs.png` sprite sheets for the hi-res renderer.

---

## Build

```
# Native
make

# Web (Emscripten)
make web
make serve

# The game checks levels.json and all of its image, font and sound files at start-up,
# and will not run if any are missing or invalid; the error names each problem.

# Headless regression check: plays the title/demo and every level with seeded
# input and prints the game state each frame. Diff two runs to spot changes.
make sim
./sim > before.txt
```

CI builds run on Ubuntu x64, Ubuntu arm64, macOS arm64, Windows x64 (MSYS2 MINGW64), and Emscripten via GitHub Actions on push to `main`.
