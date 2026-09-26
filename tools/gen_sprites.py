#!/usr/bin/env python3
"""Generate RGBA PNG sprite sheets from levels.json sprite data.

Output:
  sprites/miner.png    64×16  — 4 frames × 16×16 px, copied from sprites/16x16.png frames 0-3
  sprites/npcs.png  128×464 — 29 types × 8 frames × 16×16 px, MSB=leftmost
"""

import json, struct, zlib, os, sys

# ---------------------------------------------------------------------------
# Minimal PNG writer (RGBA, no external deps)
# ---------------------------------------------------------------------------

def _chunk(tag, data):
    c = tag + data
    return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)

def write_png(path, width, height, pixels):
    """pixels: flat list of (r,g,b,a) tuples, row-major."""
    raw = b''
    for y in range(height):
        raw += b'\x00'  # filter = None
        for x in range(width):
            r, g, b, a = pixels[y * width + x]
            raw += bytes([r, g, b, a])
    png = b'\x89PNG\r\n\x1a\n'
    png += _chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    png += _chunk(b'IDAT', zlib.compress(raw, 9))
    png += _chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(png)

WHITE = (255, 255, 255, 255)
CLEAR = (0, 0, 0, 0)

# ---------------------------------------------------------------------------
# Miner sprite — first 8 frames from sprites/16x16.png (full colour, RGBA)
# ---------------------------------------------------------------------------

def gen_miner_sheet(src_path, out_path):
    """64×16 RGBA: frames 0-3 copied directly from src_path row 0."""
    import zlib as _zlib

    # Minimal PNG reader (no external deps)
    with open(src_path, 'rb') as f:
        raw = f.read()

    def get_chunks(data):
        i = 8  # skip PNG sig
        while i < len(data):
            length = int.from_bytes(data[i:i+4], 'big')
            tag    = data[i+4:i+8]
            chunk  = data[i+8:i+8+length]
            yield tag, chunk
            i += 12 + length

    W_src = H_src = bpp = 0
    idat = b''
    for tag, chunk in get_chunks(raw):
        if tag == b'IHDR':
            W_src = int.from_bytes(chunk[0:4], 'big')
            H_src = int.from_bytes(chunk[4:8], 'big')
            bpp   = {'2': 3, '6': 4}.get(str(chunk[9]), 4)
        elif tag == b'IDAT':
            idat += chunk

    scanline = W_src * bpp + 1
    decoded  = _zlib.decompress(idat)
    src_px   = []
    for y in range(H_src):
        row = decoded[y * scanline + 1: y * scanline + 1 + W_src * bpp]
        for x in range(W_src):
            o = x * bpp
            r, g, b = row[o], row[o+1], row[o+2]
            a = row[o+3] if bpp == 4 else 255
            src_px.append((r, g, b, a))

    FW, FH = 16, 16
    W, H = FW * 4, FH
    pixels = []
    for y in range(FH):
        for fi in range(4):
            for x in range(FW):
                pixels.append(src_px[y * W_src + fi * FW + x])

    write_png(out_path, W, H, pixels)
    print(f'wrote {out_path}  ({W}×{H})')

# ---------------------------------------------------------------------------
# Npc sprites — MSB = leftmost pixel (bit 15 = column 0)
# Loaded from levels.json "sprites" array: [29 types][8 frames][16 rows]
# ---------------------------------------------------------------------------

def gen_npc_sheet(sprites, out_path):
    """128×(n_types*16) RGBA: 8 frames per row, one type per row-group."""
    n_types = len(sprites)
    W, H = 128, n_types * 16
    pixels = [CLEAR] * (W * H)
    for ti, sprite_frames in enumerate(sprites):
        for fi, frame in enumerate(sprite_frames):
            ox = fi * 16
            oy = ti * 16
            for row, word in enumerate(frame):
                for col in range(16):
                    if (word >> (15 - col)) & 1:   # MSB = leftmost
                        pixels[(oy + row) * W + (ox + col)] = WHITE
    write_png(out_path, W, H, pixels)
    print(f'wrote {out_path}  ({W}×{H})')

def gen_portal_sheet(portals, out_path):
    """16×(n_levels*16) RGBA: one 16x16 portal per level, white-on-transparent.
    White = set bit (foreground colour applied at runtime), transparent = clear bit (background)."""
    n_levels = len(portals)
    W, H = 16, n_levels * 16
    pixels = [CLEAR] * (W * H)
    for li, portal in enumerate(portals):
        gfx = portal.get('gfx', [])
        oy = li * 16
        for row, word in enumerate(gfx):
            for col in range(16):
                if (word >> (15 - col)) & 1:   # MSB = leftmost
                    pixels[(oy + row) * W + col] = WHITE
    write_png(out_path, W, H, pixels)
    print(f'wrote {out_path}  ({W}×{H})')

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    levels_path = os.path.join(repo, 'levels.json')
    sprites_dir = os.path.join(repo, 'sprites')
    os.makedirs(sprites_dir, exist_ok=True)

    if not os.path.exists(levels_path):
        print(f'ERROR: {levels_path} not found', file=sys.stderr)
        sys.exit(1)

    with open(levels_path) as f:
        data = json.load(f)

    sprites = data.get('sprites', [])
    if not sprites:
        print('WARNING: no sprites in levels.json, npcs sheet will be empty')

    portals = data.get('portal', [])
    if not portals:
        print('WARNING: no portal data in levels.json, portal sheet will be empty')

    src_16x16 = os.path.join(sprites_dir, '16x16.png')
    gen_miner_sheet(src_16x16, os.path.join(sprites_dir, 'miner.png'))
    gen_npc_sheet(sprites, os.path.join(sprites_dir, 'npcs.png'))
    gen_portal_sheet(portals, os.path.join(sprites_dir, 'portals.png'))

if __name__ == '__main__':
    main()
