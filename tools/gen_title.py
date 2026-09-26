#!/usr/bin/env python3
"""Generate sprites/title.png from titlePixels / titleColour in src/title.c."""

import os
import re
import struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PALETTE = [
    (0x00, 0x00, 0x00),  # 0  black
    (0x00, 0x00, 0xff),  # 1  blue
    (0xff, 0x00, 0x00),  # 2  red
    (0xff, 0x00, 0xff),  # 3  magenta
    (0x00, 0xff, 0x00),  # 4  green
    (0x00, 0xaa, 0xff),  # 5  light blue
    (0xff, 0xff, 0x00),  # 6  yellow
    (0xff, 0xff, 0xff),  # 7  white
    (0x80, 0x80, 0x80),  # 8  mid grey
    (0x00, 0x55, 0xff),  # 9  mid blue
    (0xaa, 0x00, 0x00),  # 10 mid red
    (0x55, 0x00, 0x00),  # 11 dark red
    (0x00, 0xaa, 0x00),  # 12 mid green
    (0x00, 0x55, 0x00),  # 13 dark green
    (0xff, 0x80, 0x00),  # 14 orange
    (0x80, 0x40, 0x00),  # 15 brown
]

W, H = 256, 72   # 32 columns × 9 tile rows, each tile row = 8 pixel rows


def parse_title_arrays(src):
    # Data lives in a block comment  /*\n{\n  ...titlePixels data...\n};\n\nstatic u8 titleColour...\n{...};\n*/
    m = re.search(r'/\*\n\{(.*?)\};\n\nstatic u8.*?titleColour.*?\n\{(.*?)\};\n\*/', src, re.DOTALL)
    if not m:
        raise ValueError('title data block comment not found in title.c')
    pixels  = [int(x, 0) for x in re.findall(r'0x[0-9a-fA-F]+|\d+', m.group(1))]
    colours = [int(x, 0) for x in re.findall(r'0x[0-9a-fA-F]+|\d+', m.group(2))]
    return pixels, colours


def render(pixels, colours):
    img = bytearray(W * H * 4)   # RGBA
    for prow in range(H):
        trow = prow // 8
        for col in range(32):
            byte = pixels[prow * 32 + col]
            attr = colours[trow * 32 + col]
            paper = (attr >> 4) & 0xf
            ink   = attr & 0xf
            for b in range(8):
                bit = (byte >> (7 - b)) & 1
                r, g, bv = PALETTE[ink if bit else paper]
                off = (prow * W + col * 8 + b) * 4
                img[off:off + 4] = bytes([r, g, bv, 0xff])
    return img


def write_png(path, width, height, rgba):
    import zlib

    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(c[4:]) & 0xffffffff)

    raw = b''
    for y in range(height):
        raw += b'\x00' + bytes(rgba[y * width * 4:(y + 1) * width * 4])
    compressed = zlib.compress(raw, 9)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', compressed))
        f.write(chunk(b'IEND', b''))


def main():
    import base64

    src_path = os.path.join(ROOT, 'src', 'title.c')
    with open(src_path) as f:
        src = f.read()

    pixels, colours = parse_title_arrays(src)

    if len(pixels) != W * H // 8:
        raise ValueError(f'unexpected titlePixels length {len(pixels)}, expected {W*H//8}')
    if len(colours) != (H // 8) * 32:
        raise ValueError(f'unexpected titleColour length {len(colours)}, expected {H//8*32}')

    rgba = render(pixels, colours)
    out  = os.path.join(ROOT, 'sprites', 'title.png')
    write_png(out, W, H, rgba)
    print(f'wrote {out}  ({W}×{H})')

    png_bytes = open(out, 'rb').read()
    b64 = base64.b64encode(png_bytes).decode()
    js_out = os.path.join(ROOT, 'editor', 'js', 'title_data.js')
    with open(js_out, 'w') as f:
        f.write(f"export const TITLE_PNG_SRC = 'data:image/png;base64,{b64}';\n")
    print(f'wrote {js_out}')


if __name__ == '__main__':
    main()
