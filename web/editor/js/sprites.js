import { COLOURS } from './palette.js';

// Miner sprite: 4 frames × 16 rows of u16 (right-facing only; left-facing mirrored at runtime)
// Game-space: 16×16 pixels. Exported as 32×32 PNG (2×2 block per bit) to match hires.c.
// Rendering convention: bit 0 (LSB) = leftmost pixel (see Video_Miner in video.c)
export const MINER_SPRITE = [
  [96,   124,  62,   44,   124,  60,   24,   60,   126,  126,  239,  223,  60,   110,  118,  238],
  [384,  496,  248,  176,  496,  240,  96,   240,  504,  472,  472,  440,  240,  96,   96,   224],
  [1536, 1984, 992,  704,  1984, 960,  384,  960,  2016, 2016, 3824, 3568, 960,  1760, 1888, 3808],
  [6144, 7936, 3968, 2816, 7936, 3840, 1536, 3840, 8064, 16320,32736,28512,7936, 23424,28864,8640],
];

export const MINER_SEQUENCE = [0, 1, 2, 3, 0, 1, 2, 3];

// Render miner sprite (LSB = leftmost pixel convention, mirror for left-facing)
export function drawMiner(ctx, frameIndex, inkColour, x, y, ps, mirror) {
  const frame = MINER_SPRITE[frameIndex] || MINER_SPRITE[0];
  ctx.fillStyle = inkColour;
  for (let row = 0; row < 16; row++) {
    const word = frame[row] || 0;
    for (let col = 0; col < 16; col++) {
      const bit = mirror ? (word >> (15 - col)) & 1 : (word >> col) & 1;
      if (bit) {
        ctx.fillRect(x + col * ps, y + row * ps, ps, ps);
      }
    }
  }
}

// Render npc sprite from JSON sprites array.
// New format: frame is a flat array of 256 palette indices (row-major), 0 = transparent.
// Legacy format (array of 16 u16 values) is auto-detected and rendered as white.
export function drawNpc(ctx, spriteFrames, frameIndex, inkColour, x, y, ps) {
  if (!spriteFrames) return;
  const nf = spriteFrames.length;
  const fi = frameIndex % nf;
  const frame = spriteFrames[fi];
  if (!frame) return;

  if (frame.length === 256) {
    // New palette-indexed format
    for (let row = 0; row < 16; row++) {
      for (let col = 0; col < 16; col++) {
        const idx = frame[row * 16 + col] || 0;
        if (idx === 0) continue;
        ctx.fillStyle = COLOURS[idx & 0xf];
        ctx.fillRect(x + col * ps, y + row * ps, ps, ps);
      }
    }
  } else {
    // Legacy 1-bit u16 format — render in inkColour
    ctx.fillStyle = inkColour;
    for (let row = 0; row < 16; row++) {
      const word = frame[row] || 0;
      for (let col = 0; col < 16; col++) {
        if ((word >> (15 - col)) & 1) {
          ctx.fillRect(x + col * ps, y + row * ps, ps, ps);
        }
      }
    }
  }
}

// Render portal sprite (same MSB-left convention as npcs)
export function drawPortal(ctx, gfx, c0, c1, x, y, ps) {
  if (!gfx) return;
  for (let row = 0; row < 16; row++) {
    const word = gfx[row] || 0;
    for (let col = 0; col < 16; col++) {
      const bit = (word >> (15 - col)) & 1;
      ctx.fillStyle = COLOURS[bit ? c1 : c0];
      ctx.fillRect(x + col * ps, y + row * ps, ps, ps);
    }
  }
}
