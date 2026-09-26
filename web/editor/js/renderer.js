import { COLOURS } from "./palette.js";
import { drawMiner, drawNpc, drawPortal } from "./sprites.js";
import { TITLE_PNG_SRC } from "./title_data.js";

// Title screen image used as the backdrop for level 19 (The Final Barrier)
export const titleImg = new Image();
titleImg.src = TITLE_PNG_SRC;

export const GRID_W = 32;
export const GRID_H = 16;

// Tile types (from game.h) that form walkable platform surfaces
const PLATFORM_TYPES = new Set([5, 6, 7, 8]); // Floor, Collapse, Conveyor L/R

// Maximum horizontal distance in pixels that the miner can jump.
// Derived from src/miner.c: a jump lasts 18 frames, and MoveLeftRight() is called every frame.
// Every 4 frames, the miner moves 8 pixels. 18 / 4 = 4.5 -> 4 moves = 32 pixels.
export const MAX_JUMP_PX = 32;

// Returns [{col, row, width}, ...] for each platform gap wider than MAX_JUMP_PX.
export function computeGapWarnings(level) {
  if (!level || !level.data || !level.info) return [];
  const warnings = [];
  for (let row = 0; row < GRID_H; row++) {
    const segments = [];
    let segStart = -1;
    for (let col = 0; col < GRID_W; col++) {
      const gi = level.data[row * GRID_W + col] || 0;
      const info = level.info[gi];
      const isPlatform = info && PLATFORM_TYPES.has(info.type);
      if (isPlatform && segStart < 0) {
        segStart = col;
      } else if (!isPlatform && segStart >= 0) {
        segments.push({ start: segStart, end: col - 1 });
        segStart = -1;
      }
    }
    if (segStart >= 0) segments.push({ start: segStart, end: GRID_W - 1 });

    for (let i = 1; i < segments.length; i++) {
      const gapTiles = segments[i].start - segments[i - 1].end - 1;
      const gapPx = gapTiles * 8;
      if (gapPx > MAX_JUMP_PX) {
        warnings.push({ col: segments[i - 1].end + 1, row, width: gapTiles });
      }
    }
  }
  return warnings;
}

// Tile size in editor pixels = 8 * zoom
export function tileSize(zoom) {
  return 8 * zoom;
}

// Convert canvas mouse position to tile grid coordinates
export function canvasToTile(canvas, clientX, clientY, zoom) {
  const rect = canvas.getBoundingClientRect();
  const scaleX = canvas.width / rect.width;
  const scaleY = canvas.height / rect.height;
  const cx = (clientX - rect.left) * scaleX;
  const cy = (clientY - rect.top) * scaleY;
  const ts = tileSize(zoom);
  const col = Math.max(0, Math.min(GRID_W - 1, Math.floor(cx / ts)));
  const row = Math.max(0, Math.min(GRID_H - 1, Math.floor(cy / ts)));
  return { col, row, index: row * GRID_W + col };
}

// Draw a single tile: paper, then the 16×16 tiles.png mask (1 = ink) on top, as the game does.
// A missing mask draws paper only.
export function drawTile(ctx, mask, colour, tx, ty, ts) {
  const ps = ts / 16;
  ctx.fillStyle = COLOURS[(colour >> 4) & 0xf];
  ctx.fillRect(tx, ty, ts, ts);
  if (!mask) return;
  ctx.fillStyle = COLOURS[colour & 0xf];
  for (let i = 0; i < 256; i++)
    if (mask[i]) ctx.fillRect(tx + (i % 16) * ps, ty + Math.floor(i / 16) * ps, ps, ps);
}

// The colour a slot is shown in. Items with black ink and all-black space would be invisible;
// the game colours items itself, so these defaults are display-only and never saved.
export function displayColour(info) {
  if (!info) return 0;
  if (info.type === 0 && (info.colour & 0xf) === 0) return info.colour | 0x02;
  if (info.type === 3 && info.colour === 0) return 0x07;
  return info.colour;
}

// Draw the full level onto a canvas element
export function renderLevel(canvas, state) {
  const {
    levels,
    sprites,
    npcs,
    miner,
    portal,
    currentLevel,
    zoom,
    showGrid,
    showSprites,
    showGapWarnings,
    currentTool,
    selectedNpc,
  } = state;
  const level = levels[currentLevel];
  if (!level) {
    const ctx = canvas.getContext("2d");
    canvas.width = 512;
    canvas.height = 256;
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, 512, 256);
    ctx.fillStyle = "#555";
    ctx.font = "14px monospace";
    ctx.fillText("Import a levels.json to begin", 20, 130);
    return;
  }

  const ts = tileSize(zoom);
  canvas.width = GRID_W * ts;
  canvas.height = GRID_H * ts;
  const ctx = canvas.getContext("2d");
  ctx.imageSmoothingEnabled = false;

  // Level 19 (The Final Barrier) uses the title screen as backdrop for T_VOID tiles
  if (
    state.currentLevel === 19 &&
    titleImg.complete &&
    titleImg.naturalWidth > 0
  ) {
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(
      titleImg,
      0,
      0,
      (titleImg.naturalWidth * ts) / 8,
      (titleImg.naturalHeight * ts) / 8,
    );
  }

  // Tiles — skip T_VOID (type 10) on level 19 so the title backdrop shows through
  const skipVoid = state.currentLevel === 19;
  for (let i = 0; i < 512; i++) {
    const col = i % GRID_W;
    const row = Math.floor(i / GRID_W);
    const gi = level.data[i] || 0;
    const info = level.info[gi];
    if (skipVoid && info && info.type === 10) continue;
    const mask = state.tileMasks[currentLevel] && state.tileMasks[currentLevel][gi];
    let col_ = displayColour(info);
    drawTile(ctx, mask, col_, col * ts, row * ts, ts);
    // Item tiles (type 0): draw a red dot indicator so they're always visible
    if (info && info.type === 0) {
      ctx.fillStyle = "rgba(255,60,60,0.85)";
      const dot = Math.max(2, ts * 0.35);
      const ox = col * ts + (ts - dot) / 2;
      const oy = row * ts + (ts - dot) / 2;
      ctx.fillRect(ox, oy, dot, dot);
    }
  }

  if (showGapWarnings) {
    const gaps = computeGapWarnings(level);
    if (gaps.length) {
      ctx.save();
      gaps.forEach(({ col, row, width }) => {
        const x = col * ts;
        const y = row * ts;
        const w = width * ts;
        // Solid tinted fill
        ctx.fillStyle = "rgba(255, 80, 0, 0.28)";
        ctx.fillRect(x, y, w, ts);
        // Diagonal stripe overlay
        ctx.beginPath();
        ctx.rect(x, y, w, ts);
        ctx.clip();
        ctx.strokeStyle = "rgba(255, 140, 0, 0.45)";
        ctx.lineWidth = Math.max(1, ts / 8);
        const step = Math.max(4, ts / 2);
        for (let d = -ts; d < w + ts; d += step) {
          ctx.beginPath();
          ctx.moveTo(x + d, y);
          ctx.lineTo(x + d + ts, y + ts);
          ctx.stroke();
        }
        ctx.restore();
        ctx.save();
        // Border
        ctx.strokeStyle = "rgba(255, 80, 0, 0.85)";
        ctx.lineWidth = 1.5;
        ctx.strokeRect(x + 0.75, y + 0.75, w - 1.5, ts - 1.5);
      });
      ctx.restore();
    }
  }

  if (showSprites) {
    const ps = zoom; // 1 game pixel = zoom editor pixels

    // Portal
    const p = portal && portal[currentLevel];
    if (p && p.gfx) {
      drawPortal(ctx, p.gfx, p.colour[0], p.colour[1], p.x * ts, p.y * ts, ps);
    }

    // Npcs
    const lvlNpcs = (npcs && npcs[currentLevel]) || [];
    lvlNpcs.forEach((npc, i) => {
      if (!npc) return;
      const sp = sprites && sprites[npc.gfx];
      const ink = COLOURS[npc.ink & 0xf] || "#ff0000";
      drawNpc(ctx, sp, npc.frame || 0, ink, npc.x * ts, npc.y * ts, ps);
      // Highlight selected npc
      if (currentTool === "npc" && i === selectedNpc) {
        ctx.strokeStyle = "#58a6ff";
        ctx.lineWidth = 2;
        ctx.strokeRect(
          npc.x * ts + 1,
          npc.y * ts + 1,
          16 * ps - 2,
          16 * ps - 2,
        );
      }
    });

    // Npc patrol ranges when in npc tool
    if (currentTool === "npc") {
      ctx.setLineDash([3, 3]);
      ctx.lineWidth = 1;
      lvlNpcs.forEach((npc, i) => {
        if (!npc) return;
        ctx.strokeStyle =
          i === selectedNpc ? "#58a6ff" : "rgba(88,166,255,0.35)";
        const minX = (npc.min / 8) * ts;
        const maxX = (npc.max / 8) * ts;
        const ry = npc.y * ts;
        ctx.strokeRect(minX, ry, maxX - minX, ts * 2);
      });
      ctx.setLineDash([]);
    }

    // Miner start
    const m = miner && miner[currentLevel];
    if (m) {
      const ink = COLOURS[m.ink & 0xf] || "#ffffff";
      drawMiner(ctx, m.frame || 0, ink, m.x * ts, m.y * ts, ps, m.dir);
      // Label
      if (currentTool === "start") {
        ctx.fillStyle = "#58a6ff";
        ctx.font = `${Math.max(8, zoom * 2)}px sans-serif`;
        ctx.fillText("START", m.x * ts, m.y * ts - 2);
      }
    }

    // Portal highlight in portal tool
    if (currentTool === "portal" && p) {
      ctx.strokeStyle = "#3fb950";
      ctx.lineWidth = 2;
      ctx.setLineDash([3, 3]);
      ctx.strokeRect(p.x * ts, p.y * ts, 16 * ps, 16 * ps);
      ctx.setLineDash([]);
    }
  }

  // Grid
  if (showGrid) {
    ctx.strokeStyle = "rgba(255,255,255,0.1)";
    ctx.lineWidth = 0.5;
    for (let c = 0; c <= GRID_W; c++) {
      ctx.beginPath();
      ctx.moveTo(c * ts, 0);
      ctx.lineTo(c * ts, canvas.height);
      ctx.stroke();
    }
    for (let r = 0; r <= GRID_H; r++) {
      ctx.beginPath();
      ctx.moveTo(0, r * ts);
      ctx.lineTo(canvas.width, r * ts);
      ctx.stroke();
    }
  }
}

// Draw small tile preview (×8 scale in 64×64 canvas)
export function renderPreview(canvas, mask, colour) {
  const ctx = canvas.getContext("2d");
  ctx.imageSmoothingEnabled = false;
  drawTile(ctx, mask, colour, 0, 0, canvas.width);
}

// Draw a 16×16 NPC sprite frame in the sprite designer canvas.
// data: flat array of 256 palette indices (row-major), 0 = transparent.
export function renderSpriteDesigner(canvas, data) {
  const ctx = canvas.getContext("2d");
  const W = canvas.width;
  const ps = W / 16;
  ctx.fillStyle = "#111";
  ctx.fillRect(0, 0, W, W);
  for (let row = 0; row < 16; row++) {
    for (let col = 0; col < 16; col++) {
      const idx = (data && data[row * 16 + col]) || 0;
      if (idx === 0) continue;
      ctx.fillStyle = COLOURS[idx & 0xf];
      ctx.fillRect(col * ps, row * ps, ps, ps);
    }
  }
  ctx.strokeStyle = "rgba(255,255,255,0.12)";
  ctx.lineWidth = 0.5;
  for (let i = 0; i <= 16; i++) {
    ctx.beginPath();
    ctx.moveTo(i * ps, 0);
    ctx.lineTo(i * ps, W);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(0, i * ps);
    ctx.lineTo(W, i * ps);
    ctx.stroke();
  }
}

// Draw a tiny gfx slot thumbnail (for the palette row)
export function renderSlot(canvas, mask, colour) {
  canvas.width = 32;
  canvas.height = 32;
  const ctx = canvas.getContext("2d");
  ctx.imageSmoothingEnabled = false;
  drawTile(ctx, mask, colour, 0, 0, 32);
}
