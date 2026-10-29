#!/usr/bin/env python3
"""Decode a JSW64 (JSWED 2.3.x, variant "Z") 128K TAP and survey its rooms.

Built for "Manic Miner: Deeper and Down 128K v1.1" to see which levels need
mechanics this port lacks. Read-only: never touches levels.json.

Usage:
  python3 -I tools/jsw64_tap.py survey <tap> <outdir>
      writes <outdir>/survey.md and <outdir>/NN.png (256x128 room renders)
  python3 -I tools/jsw64_tap.py dump <tap> <room>
      hex/field dump of one room, for checking the decoder

Format notes. Room layout is from seasip.info/Jsw/jsw64room.html. Items, the
bank map and the cell-type table location were found by inspecting this TAP:
  - rooms.jz1/3/4/6 are 16K pages, 16 rooms x 1024 bytes each; room n lives
    in bank [1,3,4,6][n // 16] at (n % 16) * 1024.
  - rtime.jz2 is the runtime image for 8000-FFFF. Room buffer is 8000-83FF,
    so sprite pages 80-83 refer to the room itself (free space at +100).
  - Items: JSW-style table, 256 entries, A4xx (bits 0-5 room, bit 6
    uncollected, bit 7 y bit 3) and A5xx (bits 0-4 x, bits 5-7 y bits 0-2).
  - Cell-type table (16 bytes, bank 7 F4FF) is in hiload.jz7, which sits in
    bank 7 from DB00.
Room fields (offsets into the 1024-byte room):
  00..   guardian instances, 8 bytes each, FF terminated (address at DA-DB)
  41-B5  13 cell defs: attribute + 8 bitmap bytes
  B6-D5  name
  D6     bits 0-1 conveyor direction, bits 2-4/5-7 top/bottom image
  D7-D8  conveyor attr-buffer address (5E00 based), D9 conveyor length
  DE     bits 0-2 border, 3-5 Willy ink, 6 rigor mortis, 7 superjump
  DF-E0  air, E1-E8 item bitmap, E9-EC exits L/R/U/D, ED Willy sprite page
  EE-F8  portal: gfx addr, attr pos (5C00), bitmap pos, attr, dest room,
         dest Willy attr pos, dest Willy y
  FF     flags: 0 lose items, 1 escalators down, 2 ramps->escalators,
         3 free falls, 7 bonus room
  200-3FF 32x16 attribute map; each attr picks the cell def with that attr
"""

import os
import struct
import sys
import zlib

ROOM_BANKS = [1, 3, 4, 6]
BANK_FILES = {1: 'rooms.jz1', 3: 'rooms.jz3', 4: 'rooms.jz4', 6: 'rooms.jz6'}
HILOAD_BASE = 0xDB00    # bank 7 address of hiload.jz7 byte 0
CELLTYPE_ADDR = 0xF4FF  # bank 7 address of the 16-byte cell-type table

CELL_NAMES = ['air', 'water', 'earth', 'fire', 'ramp\\', 'ramp/', 'conv<<',
              'conv>>', 'crumble', 'trampoline']

# GB0 low nibble -> guardian kind; type 8 is split by high nibble below.
GUARD_KINDS = {0: 'blank', 1: 'horizontal', 2: 'vertical', 3: 'rope',
               4: 'arrow', 5: 'diagonal1', 6: 'diagonal2',
               7: 'vertical-cycling', 9: 'horizontal-cycling', 0xA: 'vertical',
               0xD: 'diagonal1-cycling', 0xE: 'diagonal2-cycling'}
EXT_KINDS = {0x0: 'skylab', 0x1: 'eugene', 0x2: 'eugene', 0x3: 'eugene',
             0x8: 'trigger', 0x9: 'switch', 0xA: 'opening-wall',
             0xB: 'stopper', 0xC: 'stopper'}
PORT_KINDS = {'horizontal', 'vertical', 'skylab', 'eugene'}

PORT_MAX_NPCS = 8
PORT_TILE_SLOTS = 9  # 10 info slots, one taken by the item

ZX = [(0, 0, 0), (0, 0, 205), (205, 0, 0), (205, 0, 205),
      (0, 205, 0), (0, 205, 205), (205, 205, 0), (205, 205, 205),
      (0, 0, 0), (0, 0, 255), (255, 0, 0), (255, 0, 255),
      (0, 255, 0), (0, 255, 255), (255, 255, 0), (255, 255, 255)]


# ---------------------------------------------------------------------------
# TAP / memory
# ---------------------------------------------------------------------------

def read_tap(path):
    """Return {filename: data} for each header+data pair in the TAP."""
    with open(path, 'rb') as f:
        raw = f.read()
    blocks, i = [], 0
    while i + 2 <= len(raw):
        n = raw[i] | raw[i + 1] << 8
        blocks.append(raw[i + 2:i + 2 + n])
        i += 2 + n
    files = {}
    for hdr, data in zip(blocks, blocks[1:]):
        if len(hdr) == 19 and hdr[0] == 0 and data and data[0] == 0xFF:
            files[hdr[2:12].decode('latin-1').strip()] = data[1:-1]
    return files


class Game:
    def __init__(self, path):
        files = read_tap(path)
        self.banks = {b: files[f] for b, f in BANK_FILES.items()}
        self.rtime = files['rtime.jz2']       # 8000-FFFF
        hi = files['hiload.jz7']
        off = CELLTYPE_ADDR - HILOAD_BASE
        self.cell_types = list(hi[off:off + 16])
        self.items = self._items()

    def room_bytes(self, n):
        bank = self.banks[ROOM_BANKS[n // 16]]
        return bank[(n % 16) * 1024:(n % 16 + 1) * 1024]

    def mem(self, addr, n, room):
        """Read n bytes at a runtime address, with the room buffer at 8000."""
        if 0x8000 <= addr < 0x8400:
            return room[addr - 0x8000:addr - 0x8000 + n]
        return self.rtime[addr - 0x8000:addr - 0x8000 + n]

    def _items(self):
        items = {}
        for i in range(256):
            a, b = self.rtime[0x2400 + i], self.rtime[0x2500 + i]
            y = (a >> 7) << 3 | b >> 5
            items.setdefault(a & 0x3F, []).append((b & 31, y))
        return items

    def rooms(self):
        """All non-empty rooms in room-number order."""
        out = []
        for n in range(64):
            r = self.room_bytes(n)
            if len(r) == 1024 and any(r[0xB6:0xD6]):
                out.append(Room(self, n, r))
        return out


# ---------------------------------------------------------------------------
# Room decode
# ---------------------------------------------------------------------------

class Room:
    def __init__(self, game, n, r):
        self.n, self.raw = n, r
        self.name = r[0xB6:0xD6].decode('latin-1').strip()
        self.defs = [(r[0x41 + 9 * k], r[0x42 + 9 * k:0x4A + 9 * k])
                     for k in range(13)]
        self.attr_to_def = {}
        for k, (a, _) in enumerate(self.defs):
            self.attr_to_def.setdefault(a, k)
        self.map = list(r[0x200:0x400])
        self.cell_type = [game.cell_types[k] for k in range(13)]
        self.conv_dir = r[0xD6] & 3
        self.top_img, self.bot_img = (r[0xD6] >> 2) & 7, r[0xD6] >> 5
        conv = (r[0xD7] | r[0xD8] << 8) - 0x5E00
        self.conveyor = (conv % 32, conv // 32, r[0xD9])
        self.border = r[0xDE] & 7
        self.willy_ink = (r[0xDE] >> 3) & 7
        self.rigor, self.superjump = bool(r[0xDE] & 0x40), bool(r[0xDE] & 0x80)
        self.solar = (r[0xDC], r[0xDD])
        self.air = (r[0xDF], r[0xE0])
        self.item_gfx = r[0xE1:0xE9]
        self.exits = list(r[0xE9:0xED])
        self.willy_page = r[0xED]
        pa = (r[0xF0] | r[0xF1] << 8) - 0x5C00
        self.portal = dict(gfx=r[0xEE] | r[0xEF] << 8, x=pa % 32, y=pa // 32,
                           attr=r[0xF4], dest=r[0xF5])
        wa = (r[0xF6] | r[0xF7] << 8) - 0x5C00
        self.dest_willy = (wa % 32, r[0xF8] // 2)
        self.patches = r[0xF9:0xFF]
        self.flags = r[0xFF]
        self.guard_addr = r[0xDA] | r[0xDB] << 8
        self.guardians = []
        g = game.mem(self.guard_addr, 0x80, r)
        for k in range(0, len(g) - 7, 8):
            if g[k] == 0xFF:
                break
            self.guardians.append(Guardian(g[k:k + 8]))
        self.items = game.items.get(n, [])

    def def_at(self, x, y):
        """Cell def index at (x, y); None if the attr matches no def."""
        return self.attr_to_def.get(self.map[y * 32 + x])

    def type_at(self, x, y):
        k = self.def_at(x, y)
        return 1 if k is None else self.cell_type[k]  # unknown -> water

    def type_counts(self):
        c = {}
        for y in range(16):
            for x in range(32):
                t = self.type_at(x, y)
                c[t] = c.get(t, 0) + 1
        return c

    def defs_used(self):
        return sorted({self.def_at(x, y) for y in range(16) for x in range(32)
                       if self.def_at(x, y) is not None})

    def unknown_attrs(self):
        return sorted({a for a in self.map if a not in self.attr_to_def})

    def open_edges(self):
        """Which of L/R/U/D edges have a non-earth cell."""
        edges = {
            'L': [(0, y) for y in range(16)], 'R': [(31, y) for y in range(16)],
            'U': [(x, 0) for x in range(32)], 'D': [(x, 15) for x in range(32)]}
        return {k: any(self.type_at(x, y) != 2 for x, y in v)
                for k, v in edges.items()}


class Guardian:
    def __init__(self, b):
        self.b = bytes(b)
        lo, hi = b[0] & 15, b[0] >> 4
        self.kind = (EXT_KINDS.get(hi, 'ext%x' % hi) if lo == 8
                     else GUARD_KINDS.get(lo, 'type%x' % lo))
        self.ink, self.bright = b[1] & 7, bool(b[1] & 8)
        self.x, self.frame = b[2] & 31, b[2] >> 5
        self.y = b[3] // 2
        self.page = b[5]
        if self.kind == 'opening-wall':
            self.x, self.y = b[6] & 31, b[7] // 2

    def drawn(self):
        return self.kind not in ('trigger', 'opening-wall', 'stopper', 'blank')

    def __repr__(self):
        return '%s@%d,%d' % (self.kind, self.x, self.y)


# ---------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------

def _chunk(tag, data):
    c = tag + data
    return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)


def write_png(path, width, height, rows):
    """rows: list of height bytearrays, each width*3 RGB bytes."""
    raw = b''.join(b'\x00' + bytes(r) for r in rows)
    png = b'\x89PNG\r\n\x1a\n'
    png += _chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
    png += _chunk(b'IDAT', zlib.compress(raw, 9))
    png += _chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(png)


def render(game, room, path):
    W, H = 256, 128
    rows = [bytearray(W * 3) for _ in range(H)]

    def put(x, y, rgb):
        if 0 <= x < W and 0 <= y < H:
            rows[y][x * 3:x * 3 + 3] = bytes(rgb)

    def colours(attr):
        br = 8 if attr & 0x40 else 0
        return ZX[(attr & 7) | br], ZX[((attr >> 3) & 7) | br]

    def blit8(cx, cy, bmp, attr):
        ink, paper = colours(attr)
        for r in range(8):
            for c in range(8):
                put(cx * 8 + c, cy * 8 + r, ink if bmp[r] & (0x80 >> c) else paper)

    def blit16(px, py, data, ink):
        for r in range(16):
            v = data[2 * r] << 8 | data[2 * r + 1]
            for c in range(16):
                if v & (0x8000 >> c):
                    put(px + c, py + r, ink)

    for y in range(16):
        for x in range(32):
            a = room.map[y * 32 + x]
            k = room.def_at(x, y)
            blit8(x, y, room.defs[1 if k is None else k][1], a)

    for x, y in room.items:
        blit8(x, y, room.item_gfx, (room.map[y * 32 + x] & 0x38) | 0x46)

    p = room.portal
    if p['gfx']:
        ink, paper = colours(p['attr'])
        data = game.mem(p['gfx'], 32, room.raw)
        for r in range(16):
            v = data[2 * r] << 8 | data[2 * r + 1]
            for c in range(16):
                put(p['x'] * 8 + c, p['y'] * 8 + r, ink if v & (0x8000 >> c) else paper)

    for g in room.guardians:
        if not g.drawn():
            # mark non-sprite specials with a small cross
            for d in range(-3, 4):
                put(g.x * 8 + 4 + d, g.y + 4 + d, (255, 0, 255))
                put(g.x * 8 + 4 + d, g.y + 4 - d, (255, 0, 255))
            continue
        data = game.mem(g.page << 8 | (g.frame * 32), 32, room.raw)
        blit16(g.x * 8, g.y, data, ZX[g.ink | (8 if g.bright else 0)])

    write_png(path, W, H, rows)


# ---------------------------------------------------------------------------
# Survey
# ---------------------------------------------------------------------------

def features(room):
    """Return list of (feature, detail) the port does not support."""
    out = []
    tc = room.type_counts()
    if tc.get(4) or tc.get(5):
        out.append(('ramp', '%d\\ %d/' % (tc.get(4, 0), tc.get(5, 0))))
    if tc.get(9):
        out.append(('trampoline', '%d cells' % tc[9]))
    kinds = {}
    for g in room.guardians:
        kinds[g.kind] = kinds.get(g.kind, 0) + 1
    for k, c in sorted(kinds.items()):
        if k not in PORT_KINDS:
            out.append((k, '%d' % c))
    if len(room.guardians) - kinds.get('trigger', 0) - kinds.get('stopper', 0) > PORT_MAX_NPCS:
        out.append(('>8 guardians', '%d' % len(room.guardians)))
    if len(room.defs_used()) > PORT_TILE_SLOTS:
        out.append(('>9 tile slots', '%d' % len(room.defs_used())))
    if room.unknown_attrs():
        out.append(('unknown attrs', ' '.join('%02x' % a for a in room.unknown_attrs())))
    edges = room.open_edges()
    links = ['%s->%d' % (e, d) for e, d in zip('LRUD', room.exits)
             if edges[e] and d != room.n]
    if links:
        out.append(('open edge', ' '.join(links)))
    if room.willy_page >= 0x80:
        out.append(('custom Willy sprite', 'page %02x' % room.willy_page))
    for bit, name in ((1, 'escalators down'), (2, 'ramps->escalators'),
                      (3, 'free falls'), (7, 'bonus room')):
        if room.flags & (1 << bit):
            out.append((name, ''))
    if room.top_img or room.bot_img:
        out.append(('top/bottom image', '%d/%d' % (room.top_img, room.bot_img)))
    if room.rigor or room.superjump:
        out.append(('rigor/superjump', ''))
    if room.solar[1]:
        out.append(('solar beam', ''))
    if room.conv_dir >= 2:
        out.append(('sticky/off conveyor', str(room.conv_dir)))
    if room.guard_addr != 0x8000:
        out.append(('guardian table moved', '%04x' % room.guard_addr))
    return out


def survey(tap, outdir):
    game = Game(tap)
    rooms = [r for r in game.rooms() if r.name != 'Template Room']
    os.makedirs(outdir, exist_ok=True)
    lines = ['# Deeper and Down survey', '',
             'Source: `%s`  ' % os.path.basename(tap),
             'Cell-type table: %s' % ' '.join(
                 '%d=%s' % (k, CELL_NAMES[t]) for k, t in enumerate(game.cell_types[:13])),
             '',
             'Tile slots = distinct cell defs used in the map (port budget %d). '
             'Guardians = all 8-byte records incl. triggers/switches (port max %d sprites). '
             'Feature cells count map cells of that type, including decorative ones.'
             % (PORT_TILE_SLOTS, PORT_MAX_NPCS), '',
             '"open edge" = a map edge with a non-earth cell whose exit is not the room '
             'itself (in JSW64, leaving the room there goes to that room). Whether '
             'Willy can reach it needs checking by eye.  ',
             'Rooms with flag bit 0 (lose items on death, as in MM): %d of %d.'
             % (sum(r.flags & 1 for r in rooms), len(rooms)), '',
             '| # | Room | Name | Slots | Items | Guardians | Conv | Crumble | Ramp | Tramp | Unsupported |',
             '|---|---|---|---|---|---|---|---|---|---|---|']
    by_feature = {}
    chain = []
    for i, room in enumerate(rooms, 1):
        tc = room.type_counts()
        kinds = {}
        for g in room.guardians:
            kinds[g.kind] = kinds.get(g.kind, 0) + 1
        gtxt = ', '.join('%d %s' % (c, k) for k, c in sorted(kinds.items())) or '-'
        feats = features(room)
        for f, d in feats:
            by_feature.setdefault(f, []).append((i, room.name, d))
        ftxt = '; '.join(f + (' (%s)' % d if d else '') for f, d in feats) or '-'
        lines.append('| %d | %d | %s | %d | %d | %s | %d | %d | %d | %d | %s |' % (
            i, room.n, room.name, len(room.defs_used()), len(room.items), gtxt,
            tc.get(6, 0) + tc.get(7, 0), tc.get(8, 0),
            tc.get(4, 0) + tc.get(5, 0), tc.get(9, 0), ftxt))
        if room.portal['dest'] != room.n + 1:
            chain.append('%d %s: portal -> room %d' % (i, room.name, room.portal['dest']))
        render(game, room, os.path.join(outdir, '%02d.png' % i))

    lines += ['', '## Unsupported features', '']
    for f in sorted(by_feature, key=lambda f: (-len(by_feature[f]), f)):
        lv = by_feature[f]
        lines.append('- **%s** (%d): %s' % (f, len(lv), ', '.join(
            '%d %s' % (i, n) + (' [%s]' % d if d else '') for i, n, d in lv)))
    lines += ['', '## Portal chain', '']
    lines += ['- ' + c for c in chain] or ['- every room\'s portal leads to the next room']
    with open(os.path.join(outdir, 'survey.md'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('%d levels -> %s' % (len(rooms), outdir))


def dump(tap, n):
    game = Game(tap)
    room = Room(game, n, game.room_bytes(n))
    print('room %d: %s' % (n, room.name))
    for k, (a, bmp) in enumerate(room.defs):
        print('  def %2d attr %02x %-10s %s' % (k, a, CELL_NAMES[room.cell_type[k]], bmp.hex()))
    for y in range(16):
        print('  ' + ''.join('?' if room.def_at(x, y) is None else
                             '%x' % room.def_at(x, y) for x in range(32)))
    for g in room.guardians:
        print('  guardian %s %r' % (g.b.hex(' '), g))
    print('  items', room.items)
    print('  portal', room.portal, 'dest willy', room.dest_willy)
    print('  exits', room.exits, 'air', room.air, 'border', room.border,
          'conveyor', room.conveyor, 'dir', room.conv_dir, 'flags %02x' % room.flags)


# ---------------------------------------------------------------------------
# Import into levels.json and the sprite sheets
# ---------------------------------------------------------------------------

BASE_LEVELS, BASE_SPRITES = 20, 29   # the original Manic Miner data kept in front
FIRST_START = (28, 3, 1)             # level 1 Willy x, y, dir: read off the map sheet

# Port tile types (src/game.h)
T_ITEM, T_SWITCHOFF, T_SPACE, T_SOLID, T_FLOOR, T_COLLAPSE = 0, 1, 3, 4, 5, 6
T_CONVEYL, T_CONVEYR, T_HARM, T_TRAMP, T_RAMPL, T_RAMPR = 7, 8, 9, 11, 12, 13
JSW_TO_PORT = {0: T_SPACE, 1: T_FLOOR, 2: T_SOLID, 3: T_HARM, 4: T_RAMPL,
               5: T_RAMPR, 6: T_CONVEYL, 7: T_CONVEYR, 8: T_COLLAPSE, 9: T_TRAMP}
MOVE_LEFT, MOVE_RIGHT, MOVE_UP, MOVE_DOWN = 1, 2, 3, 4

# ZX colour (0-7) -> port palette index (src/misc.h), normal and bright
ZX_NORMAL = [0, 9, 10, 3, 12, 5, 14, 7]
ZX_BRIGHT = [0, 1, 2, 3, 4, 5, 6, 7]


def port_ink(attr):
    return (ZX_BRIGHT if attr & 0x40 else ZX_NORMAL)[attr & 7]


def port_paper(attr):
    return (ZX_BRIGHT if attr & 0x40 else ZX_NORMAL)[(attr >> 3) & 7]


def port_colour(attr):
    return port_paper(attr) << 4 | port_ink(attr)


def scale2x(bits):
    """bits: list of rows of 0/1. Returns the Scale2x-upscaled rows."""
    h, w = len(bits), len(bits[0])

    def px(x, y):
        return bits[min(max(y, 0), h - 1)][min(max(x, 0), w - 1)]
    out = [[0] * (w * 2) for _ in range(h * 2)]
    for y in range(h):
        for x in range(w):
            p, a, b, c, d = px(x, y), px(x, y - 1), px(x + 1, y), px(x - 1, y), px(x, y + 1)
            out[2 * y][2 * x] = a if c == a and c != d and a != b else p
            out[2 * y][2 * x + 1] = b if a == b and a != c and b != d else p
            out[2 * y + 1][2 * x] = c if d == c and d != b and c != a else p
            out[2 * y + 1][2 * x + 1] = d if b == d and b != a and d != c else p
    return out


def bytes_to_bits(rows, width):
    return [[(r >> (width - 1 - c)) & 1 for c in range(width)] for r in rows]


def paste_bits(img, bits, ox, oy):
    for y, row in enumerate(bits):
        for x, v in enumerate(row):
            img.putpixel((ox + x, oy + y), (255, 255, 255, 255) if v else (0, 0, 0, 0))


class Importer:
    def __init__(self, game):
        self.game = game
        self.sprites = []        # 8 frames x 16 rows, new sprites only
        self.sprite_ids = {}     # page bytes -> index into self.sprites
        self.sprite_names = []
        self.notes = []

    def sprite(self, data, name):
        key = bytes(data)
        if key not in self.sprite_ids:
            self.sprite_ids[key] = len(self.sprites)
            self.sprites.append([[key[f * 32 + 2 * r] << 8 | key[f * 32 + 2 * r + 1]
                                  for r in range(16)] for f in range(8)])
            self.sprite_names.append(name)
        return BASE_SPRITES + self.sprite_ids[key]

    def note(self, level, text):
        self.notes.append('%d %s: %s' % (level, self.room.name, text))

    def level(self, room, idx, start):
        """Convert one room. Returns (level, npcs, miner, portal, tile_bits)."""
        self.room = room
        g = self.game
        slots = []          # [type, colour, 16x16 bits]
        slot_of = {}

        def add(key, typ, colour, bits16):
            if key not in slot_of:
                slot_of[key] = len(slots)
                slots.append([typ, colour, bits16])
            return slot_of[key]

        def def_bits(k):
            return scale2x(bytes_to_bits(room.defs[k][1], 8))

        # Slot 0 is the background: its paper is the level's background colour.
        air_attr = room.defs[0][0]
        add(('cell', air_attr), T_SPACE, port_colour(air_attr), def_bits(0))
        data = [0] * 512
        for y in range(16):
            for x in range(32):
                a = room.map[y * 32 + x]
                k = room.attr_to_def.get(a)
                typ = JSW_TO_PORT[room.cell_type[k]] if k is not None else T_FLOOR
                data[y * 32 + x] = add(('cell', a), typ, port_colour(a),
                                       def_bits(1 if k is None else k))

        # Items: the cell becomes an item tile on the background's paper.
        item_bits = scale2x(bytes_to_bits(room.item_gfx, 8))
        for x, y in room.items:
            data[y * 32 + x] = add(('item',), T_ITEM, port_colour(air_attr) & 0xF0 | 6, item_bits)

        # Guardians, switches, walls and triggers.
        recs = room.guardians
        npcs, npc_slot, switches = [], {}, {}
        for i, gd in enumerate(recs):
            if gd.kind in ('horizontal', 'vertical'):
                npc_slot[i] = len(npcs)
                npcs.append(self.npc(gd, room))
            elif gd.kind == 'switch':
                switches[i] = self.switch(gd, room, add, data)
            elif gd.kind not in ('trigger', 'opening-wall'):
                self.note(idx, 'guardian %s not imported' % gd.kind)
        if len(npcs) > 8:
            raise SystemExit('%s: %d guardians' % (room.name, len(npcs)))

        base = room.guard_addr
        for i, gd in enumerate(recs):
            if gd.kind not in ('trigger', 'opening-wall'):
                continue
            flag = gd.b[1] | gd.b[2] << 8
            # The flag is the state byte (+4) of a switch record, counted from the
            # room's guardian table; Clock Tower's points into a stale copy at 8000.
            table = base if base <= flag < base + 8 * len(recs) else 0x8000
            rec = (flag - table) // 8
            if (flag - table) % 8 != 4 or rec not in switches:
                raise SystemExit('%s: record %d flag %04x is not a switch' % (room.name, i, flag))
            sw = switches[rec]
            if gd.kind == 'opening-wall':
                x, y0, lines = gd.b[6] & 31, gd.b[7] // 2, gd.b[3]
                for y in range(y0 // 8, (y0 + lines + 7) // 8):
                    sw['open'].append(y * 32 + x)
            else:
                target = i + 1
                if target not in npc_slot:
                    raise SystemExit('%s: trigger %d has no guardian after it' % (room.name, i))
                sw['npc'] = self.trigger(gd, recs[target], npc_slot[target], room)

        if len(slots) > 16:
            raise SystemExit('%s: %d tile slots' % (room.name, len(slots)))
        tile_bits = [s[2] for s in slots]

        level = {'name': room.name, 'data': data,
                 'info': [{'colour': s[1], 'type': s[0]} for s in slots],
                 'air': (room.air[0] - 36) * 8 + -(-room.air[1] // 32),
                 'border': ZX_NORMAL[room.border]}
        if switches:
            level['switches'] = list(switches.values())
        if room.flags & 8:
            level['safe_fall'] = 1

        p = room.portal
        portal = {'x': p['x'], 'y': p['y'],
                  'gfx': list(self.portal_rows(room)),
                  'colour': [port_ink(p['attr']), port_paper(p['attr'])]}
        miner = {'x': start[0], 'y': start[1], 'frame': 0, 'dir': start[2], 'ink': 7}
        return level, npcs + [None] * (8 - len(npcs)), miner, portal, tile_bits

    def portal_rows(self, room):
        d = self.game.mem(room.portal['gfx'], 32, room.raw)
        return [d[2 * r] << 8 | d[2 * r + 1] for r in range(16)]

    def page(self, gd, room):
        return self.game.mem(gd.page << 8, 256, room.raw)

    def npc(self, gd, room):
        b = gd.b
        ink = port_ink(b[1] | (0x40 if b[1] & 8 else 0))
        gfx = self.sprite(self.page(gd, room), '%s %d' % (room.name, gd.page))
        nframes = 7 if b[1] >> 5 == 7 else 3
        n = {'x': gd.x, 'y': gd.y // 8, 'gfx': gfx, 'ink': ink, 'nframes': nframes,
             'frame': gd.frame & nframes, 'solar_powered_generator': 0}
        if gd.y % 8:
            self.note(self.idx, 'guardian at y=%d rounded to row %d' % (gd.y, gd.y // 8))
        if gd.kind == 'horizontal':
            lo, hi = b[6] & 31, b[7] & 31
            move = MOVE_RIGHT if b[0] & 0x80 else MOVE_LEFT
            if gd.x <= lo:
                move = MOVE_RIGHT
            elif gd.x >= hi:
                move = MOVE_LEFT
            if b[7] & 0x80:
                self.note(self.idx, 'double-speed guardian runs at normal speed')
            n.update(min=lo * 8, max=hi * 8, move=move, speed=1 if b[7] & 0x20 else 0)
        else:
            dy = b[4] - 256 if b[4] & 0x80 else b[4]
            n.update(min=b[6] // 2, max=b[7] // 2, move=MOVE_UP if dy < 0 else MOVE_DOWN,
                     speed=abs(dy) // 2)
        return n

    def switch(self, gd, room, add, data):
        """A JSW64 switch is an 8x16 lever in one half of a 16x16 frame. The port's
        switch is one tile: the lever goes on its top cell, doubled in width only."""
        page = self.page(gd, room)

        def lever(frame):
            rows = [page[frame * 32 + 2 * r] << 8 | page[frame * 32 + 2 * r + 1] for r in range(16)]
            right = not any(r & 0xFF00 for r in rows)
            half = [(r & 0xFF) if right else (r >> 8) for r in rows]
            return right, [[(h >> (7 - c // 2)) & 1 for c in range(16)] for h in half]

        right, off = lever(gd.frame)
        _, on = lever(gd.frame ^ 1)
        x, y = gd.x + right, gd.y // 8
        cell = y * 32 + x
        under = room.map[cell]
        colour = port_paper(under) << 4 | port_ink(gd.b[1] | (0x40 if gd.b[1] & 8 else 0))
        key = (gd.page, gd.frame, colour, room.n if gd.page < 0x84 else -1)
        k = add(('switch',) + key, T_SWITCHOFF, colour, off)
        add(('switch-on',) + key, T_SPACE, colour, on)
        data[cell] = k
        return {'cell': cell, 'open': []}

    def trigger(self, gd, target, slot, room):
        """A trigger rewrites the next guardian's byte 0 and bytes 4-7."""
        b = gd.b
        kind = b[3] & 15
        if kind == 0:
            return {'slot': slot, 'active': 0}
        nb = bytes([b[3]]) + target.b[1:4] + b[4:8]
        page = self.game.mem(b[5] << 8, 256, room.raw)
        gfx = self.sprite(page, '%s %d' % (room.name, b[5]))
        if kind == 1:
            lo, hi = nb[6] & 31, nb[7] & 31
            return {'slot': slot, 'move': MOVE_RIGHT if nb[0] & 0x80 else MOVE_LEFT,
                    'min': lo * 8, 'max': hi * 8, 'speed': 1 if nb[7] & 0x20 else 0, 'gfx': gfx}
        dy = nb[4] - 256 if nb[4] & 0x80 else nb[4]
        return {'slot': slot, 'move': MOVE_UP if dy < 0 else MOVE_DOWN,
                'min': nb[6] // 2, 'max': nb[7] // 2, 'speed': abs(dy) // 2, 'gfx': gfx}


def do_import(tap, repo):
    from PIL import Image
    import json
    import subprocess

    game = Game(tap)
    rooms = [r for r in game.rooms() if r.name != 'Template Room']
    by_n = {r.n: r for r in rooms}
    imp = Importer(game)
    levels, npcs, miners, portals, tiles = [], [], [], [], []
    for i, room in enumerate(rooms):
        imp.idx = BASE_LEVELS + i + 1
        if i == 0:
            start = FIRST_START
        else:
            prev = rooms[i - 1]
            if prev.portal['dest'] != room.n:
                raise SystemExit('%s portal does not lead to %s' % (prev.name, room.name))
            sx, sy = prev.dest_willy
            start = (sx, sy // 8, 0)
        out = imp.level(room, imp.idx, start)
        for lst, v in zip((levels, npcs, miners, portals, tiles), out):
            lst.append(v)

    path = os.path.join(repo, 'levels.json')
    with open(path) as f:
        js = json.load(f)
    js['levels'] = js['levels'][:BASE_LEVELS] + levels
    js['npcs'] = js['npcs'][:BASE_LEVELS] + npcs
    js['miner'] = js['miner'][:BASE_LEVELS] + miners
    js['portal'] = js['portal'][:BASE_LEVELS] + portals
    js['sprites'] = js['sprites'][:BASE_SPRITES] + imp.sprites
    js['sprite_names'] = js['sprite_names'][:BASE_SPRITES] + imp.sprite_names
    with open(path, 'w') as f:
        json.dump(js, f)
    subprocess.run([sys.executable, '-I', os.path.join(repo, 'tools', 'fmt_levels.py')],
                   cwd=repo, check=True)

    sheets = os.path.join(repo, 'gfx', 'sprites')
    n = BASE_LEVELS + len(levels)

    old = Image.open(os.path.join(sheets, 'tiles.png')).convert('RGBA')
    img = Image.new('RGBA', (256, n * 16), (0, 0, 0, 0))
    img.paste(old.crop((0, 0, 256, BASE_LEVELS * 16)), (0, 0))
    for i, slots in enumerate(tiles):
        for s, bits in enumerate(slots):
            paste_bits(img, bits, s * 16, (BASE_LEVELS + i) * 16)
    img.save(os.path.join(sheets, 'tiles.png'), optimize=True)

    old = Image.open(os.path.join(sheets, 'npcs.png')).convert('RGBA')
    img = Image.new('RGBA', (256, (BASE_SPRITES + len(imp.sprites)) * 32), (0, 0, 0, 0))
    img.paste(old.crop((0, 0, 256, BASE_SPRITES * 32)), (0, 0))
    for i, frames in enumerate(imp.sprites):
        for f, rows in enumerate(frames):
            paste_bits(img, scale2x(bytes_to_bits(rows, 16)), f * 32, (BASE_SPRITES + i) * 32)
    img.save(os.path.join(sheets, 'npcs.png'), optimize=True)

    # portals.png: one row per level, then the swordfish (kept from the old last row).
    old = Image.open(os.path.join(sheets, 'portals.png')).convert('RGBA')
    swordfish = old.crop((0, old.height - 32, 32, old.height))
    img = Image.new('RGBA', (32, (n + 1) * 32), (0, 0, 0, 0))
    img.paste(old.crop((0, 0, 32, BASE_LEVELS * 32)), (0, 0))
    for i, p in enumerate(portals):
        paste_bits(img, scale2x(bytes_to_bits(p['gfx'], 16)), 0, (BASE_LEVELS + i) * 32)
    img.paste(swordfish, (0, n * 32))
    img.save(os.path.join(sheets, 'portals.png'), optimize=True)

    print('%d levels, %d sprites imported' % (len(levels), len(imp.sprites)))
    for line in imp.notes:
        print('  ' + line)


def main(argv):
    if len(argv) == 4 and argv[1] == 'survey':
        survey(argv[2], argv[3])
    elif len(argv) == 4 and argv[1] == 'dump':
        dump(argv[2], int(argv[3]))
    elif len(argv) == 4 and argv[1] == 'import':
        do_import(argv[2], argv[3])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv)
