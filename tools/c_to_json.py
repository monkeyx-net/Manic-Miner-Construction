#!/usr/bin/env python3
"""Extract game data from C sources and write levels.json."""

import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")


def read_src(name):
    with open(os.path.join(SRC, name)) as f:
        return f.read()


def strip_comments(text):
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return text


def find_balanced(text, start):
    """Return index of closing brace matching text[start] which must be '{'."""
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    return len(text) - 1


def extract_ints(text):
    return [int(x, 0) for x in re.findall(r"0x[0-9a-fA-F]+|-?\d+", text)]


# ---------------------------------------------------------------------------
# levels.c
# ---------------------------------------------------------------------------

TYPE_MAP = {
    "T_ITEM": 0,
    "T_SWITCHOFF": 1,
    "T_SWITCHON": 2,
    "T_SPACE": 3,
    "T_SOLID": 4,
    "T_FLOOR": 5,
    "T_COLLAPSE": 6,
    "T_CONVEYL": 7,
    "T_CONVEYR": 8,
    "T_HARM": 9,
    "T_VOID": 10,
}


def parse_gfx_body(body):
    """Parse the content of a .gfx = { ... } block, handling SPACE macro."""
    rows = []
    pos = 0
    while pos < len(body):
        while pos < len(body) and body[pos] in " \t\n\r,":
            pos += 1
        if pos >= len(body):
            break
        if body[pos : pos + 5] == "SPACE":
            rows.append([0] * 8)
            pos += 5
        elif body[pos] == "{":
            end = find_balanced(body, pos)
            vals = extract_ints(body[pos + 1 : end])
            rows.append((vals + [0] * 8)[:8])
            pos = end + 1
        else:
            pos += 1
    while len(rows) < 10:
        rows.append([0] * 8)
    return rows[:10]


def parse_info_body(body):
    """Parse the content of a .info = { ... } block."""
    entries = []
    pos = 0
    while pos < len(body) and len(entries) < 10:
        while pos < len(body) and body[pos] in " \t\n\r,":
            pos += 1
        if pos >= len(body):
            break
        if body[pos] == "{":
            end = find_balanced(body, pos)
            cell = body[pos + 1 : end]
            nums = re.findall(r"0x[0-9a-fA-F]+|\d+", cell)
            colour = int(nums[0], 0) if nums else 0
            tm = re.search(r"T_\w+", cell)
            ttype = TYPE_MAP.get(tm.group(0), 0) if tm else 0
            entries.append({"colour": colour, "type": ttype})
            pos = end + 1
        else:
            pos += 1
    while len(entries) < 10:
        entries.append({"colour": 0, "type": 3})
    return entries[:10]


def parse_levels(text):
    text = strip_comments(text)
    m = re.search(r"levelData\s*\[[^\]]*\]\s*=\s*\{", text)
    if not m:
        raise ValueError("levelData[20] not found in levels.c")
    arr_start = text.index("{", m.start())
    arr_end = find_balanced(text, arr_start)
    body = text[arr_start + 1 : arr_end]

    levels = []
    pos = 0
    while pos < len(body):
        idx = body.find("{", pos)
        if idx < 0:
            break
        end = find_balanced(body, idx)
        entry = body[idx + 1 : end]

        nm = re.search(r'\.name\s*=\s*"([^"]*)"', entry)
        name = nm.group(1) if nm else ""

        dm = re.search(r"\.data\s*=\s*\{", entry)
        if dm:
            ds = entry.index("{", dm.start())
            de = find_balanced(entry, ds)
            data = extract_ints(entry[ds + 1 : de])
        else:
            data = [0] * 512
        data = (data + [0] * 512)[:512]

        gm = re.search(r"\.gfx\s*=\s*\{", entry)
        if gm:
            gs = entry.index("{", gm.start())
            ge = find_balanced(entry, gs)
            gfx = parse_gfx_body(entry[gs + 1 : ge])
        else:
            gfx = [[0] * 8] * 10

        im = re.search(r"\.info\s*=\s*\{", entry)
        if im:
            is_ = entry.index("{", im.start())
            ie = find_balanced(entry, is_)
            info = parse_info_body(entry[is_ + 1 : ie])
        else:
            info = [{"colour": 0, "type": 3}] * 10

        levels.append({"name": name, "data": data, "gfx": gfx, "info": info})
        pos = end + 1

    return levels


# ---------------------------------------------------------------------------
# npcs.c — sprites
# ---------------------------------------------------------------------------


def parse_sprites(text):
    text = strip_comments(text)
    m = re.search(r"npcSprite\s*\[29\]\s*\[8\]\s*\[16\]\s*=\s*\{", text)
    if not m:
        raise ValueError("npcSprite not found in npcs.c")
    arr_start = text.index("{", m.start())
    arr_end = find_balanced(text, arr_start)
    body = text[arr_start + 1 : arr_end]

    sprites = []
    pos = 0
    while pos < len(body) and len(sprites) < 29:
        idx = body.find("{", pos)
        if idx < 0:
            break
        end = find_balanced(body, idx)
        sprite_body = body[idx + 1 : end]

        frames = []
        sp = 0
        while sp < len(sprite_body) and len(frames) < 8:
            while sp < len(sprite_body) and sprite_body[sp] in " \t\n\r,":
                sp += 1
            if sp >= len(sprite_body):
                break
            if sprite_body[sp] == "{":
                fe = find_balanced(sprite_body, sp)
                vals = extract_ints(sprite_body[sp + 1 : fe])
                frames.append((vals + [0] * 16)[:16])
                sp = fe + 1
            else:
                sp += 1

        while len(frames) < 8:
            frames.append([0] * 16)
        sprites.append(frames)
        pos = end + 1

    return sprites


# ---------------------------------------------------------------------------
# npcs.c — npc starts
# ---------------------------------------------------------------------------

MOVE_MAP = {
    "DoNpcLeft": 1,
    "DoNpcRight": 2,
    "DoNpcUp": 3,
    "DoNpcDown": 4,
    "DoNpcKong": 5,
    "DoNpcSkylab": 6,
    "DoNpcFall": 7,
    "DoNpcEugene": 8,
}


def parse_npcs(text):
    text = strip_comments(text)
    m = re.search(r"npcStart\s*\[[^\]]*\]\s*\[[^\]]*\]\s*=\s*\{", text)
    if not m:
        raise ValueError("npcStart not found in npcs.c")
    arr_start = text.index("{", m.start())
    arr_end = find_balanced(text, arr_start)
    body = text[arr_start + 1 : arr_end]

    all_npcs = []
    pos = 0
    while pos < len(body) and len(all_npcs) < 1000:
        idx = body.find("{", pos)
        if idx < 0:
            break
        end = find_balanced(body, idx)
        level_body = body[idx + 1 : end]

        slots = []
        lp = 0
        while lp < len(level_body) and len(slots) < 8:
            while lp < len(level_body) and level_body[lp] in " \t\n\r,":
                lp += 1
            if lp >= len(level_body):
                break
            if level_body[lp : lp + 7] == "NONPC":
                slots.append(None)
                lp += 7
                continue
            if level_body[lp] == "{":
                re_end = find_balanced(level_body, lp)
                cell = level_body[lp + 1 : re_end]

                nums = re.findall(r"0x[0-9a-fA-F]+|\d+", cell)
                fns = re.findall(r"DoNpc\w+|DoNothing", cell)

                x = int(nums[0], 0) if len(nums) > 0 else 0
                y = int(nums[1], 0) if len(nums) > 1 else 0
                min_ = int(nums[2], 0) if len(nums) > 2 else 0
                max_ = int(nums[3], 0) if len(nums) > 3 else 0
                speed = int(nums[4], 0) if len(nums) > 4 else 0
                gfx = int(nums[5], 0) if len(nums) > 5 else 0
                ink = int(nums[6], 0) if len(nums) > 6 else 0
                nframes = int(nums[7], 0) if len(nums) > 7 else 0
                frame = int(nums[8], 0) if len(nums) > 8 else 0

                move_fn = fns[0] if fns else "DoNothing"
                solar_powered_generator_fn = fns[2] if len(fns) > 2 else "DoNothing"

                slots.append(
                    {
                        "x": x,
                        "y": y,
                        "min": min_,
                        "max": max_,
                        "move": MOVE_MAP.get(move_fn, 0),
                        "speed": speed,
                        "gfx": gfx,
                        "ink": ink,
                        "nframes": nframes,
                        "frame": frame,
                        "solar_powered_generator": 1 if solar_powered_generator_fn == "DoNpcSpg" else 0,
                    }
                )
                lp = re_end + 1
            else:
                lp += 1

        while len(slots) < 8:
            slots.append(None)
        all_npcs.append(slots)
        pos = end + 1

    return all_npcs


# ---------------------------------------------------------------------------
# miner.c
# ---------------------------------------------------------------------------

DIR_MAP = {"D_RIGHT": 0, "D_LEFT": 1, "D_JUMP": 2}


def parse_miner(text):
    text = strip_comments(text)
    m = re.search(r"minerStart\s*\[[^\]]*\]\s*=\s*\{", text)
    if not m:
        raise ValueError("minerStart not found in miner.c")
    arr_start = text.index("{", m.start())
    arr_end = find_balanced(text, arr_start)
    body = text[arr_start + 1 : arr_end]

    miners = []
    pos = 0
    while pos < len(body) and len(miners) < 20:
        idx = body.find("{", pos)
        if idx < 0:
            break
        end = find_balanced(body, idx)
        entry = body[idx + 1 : end]

        xm = re.search(r"\.x\s*=\s*(\d+)", entry)
        ym = re.search(r"\.y\s*=\s*(\d+)", entry)
        fm = re.search(r"\.frame\s*=\s*(\d+)", entry)
        dm = re.search(r"\.dir\s*=\s*(D_\w+)", entry)
        im = re.search(r"\.ink\s*=\s*(0x[0-9a-fA-F]+|\d+)", entry)

        miners.append(
            {
                "x": int(xm.group(1)) if xm else 0,
                "y": int(ym.group(1)) if ym else 0,
                "frame": int(fm.group(1)) if fm else 0,
                "dir": DIR_MAP.get(dm.group(1), 0) if dm else 0,
                "ink": int(im.group(1), 0) if im else 0,
            }
        )
        pos = end + 1

    return miners


# ---------------------------------------------------------------------------
# portal.c
# ---------------------------------------------------------------------------


def parse_portal(text):
    text = strip_comments(text)
    m = re.search(r"portalData\s*\[[^\]]*\]\s*=\s*\{", text)
    if not m:
        raise ValueError("portalData not found in portal.c")
    arr_start = text.index("{", m.start())
    arr_end = find_balanced(text, arr_start)
    body = text[arr_start + 1 : arr_end]

    portals = []
    pos = 0
    while pos < len(body) and len(portals) < 20:
        idx = body.find("{", pos)
        if idx < 0:
            break
        end = find_balanced(body, idx)
        entry = body[idx + 1 : end]

        xm = re.search(r"\.x\s*=\s*(\d+)", entry)
        ym = re.search(r"\.y\s*=\s*(\d+)", entry)

        gm = re.search(r"\.gfx\s*=\s*\{", entry)
        if gm:
            gs = entry.index("{", gm.start())
            ge = find_balanced(entry, gs)
            gfx = extract_ints(entry[gs + 1 : ge])
        else:
            gfx = []
        gfx = (gfx + [0] * 16)[:16]

        cm = re.search(r"\.colour\s*=\s*\{", entry)
        if cm:
            cs = entry.index("{", cm.start())
            ce = find_balanced(entry, cs)
            colour = [
                int(x, 0) for x in re.findall(r"0x[0-9a-fA-F]+|\d+", entry[cs + 1 : ce])
            ]
        else:
            colour = []
        colour = (colour + [0, 0])[:2]

        portals.append(
            {
                "x": int(xm.group(1)) if xm else 0,
                "y": int(ym.group(1)) if ym else 0,
                "gfx": gfx,
                "colour": colour,
            }
        )
        pos = end + 1

    return portals


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------


def parse_game(text):
    """Extract airData and levelBorder arrays from game.c."""
    text = strip_comments(text)
    air = [220] * 20
    border = [0] * 20
    m = re.search(r"airData\s*\[.*?\]\s*=\s*\{([^}]*)\}", text)
    if m:
        air = extract_ints(m.group(1))[:20]
    m = re.search(r"levelBorder\s*\[.*?\]\s*=\s*\{([^}]*)\}", text)
    if m:
        border = extract_ints(m.group(1))[:20]
    return air, border


def main():
    print("Parsing levels.c ...")
    levels = parse_levels(read_src("levels.c"))

    print("Parsing npcs.c (sprites) ...")
    sprites = parse_sprites(read_src("npcs.c"))

    print("Parsing npcs.c (starts) ...")
    npcs_text = read_src("npcs.c")
    npcs = parse_npcs(npcs_text)

    print("Parsing miner.c ...")
    miners = parse_miner(read_src("miner.c"))

    print("Parsing portal.c ...")
    portals = parse_portal(read_src("portal.c"))

    print("Parsing game.c (air/border) ...")
    air_list, border_list = parse_game(read_src("game.c"))
    for i, L in enumerate(levels):
        L["air"] = air_list[i] if i < len(air_list) else 220
        L["border"] = border_list[i] if i < len(border_list) else 0

    # Preserve minerSprite from existing levels.json — it no longer lives in
    # any C source file (moved out of miner.c in the minerSprite refactor).
    out = os.path.join(ROOT, "levels.json")
    miner_sprite = None
    if os.path.exists(out):
        with open(out) as f:
            existing = json.load(f)
        miner_sprite = existing.get("minerSprite")

    data = {
        "levels": levels,
        "sprites": sprites,
        "npcs": npcs,
        "miner": miners,
        "portal": portals,
        **({"minerSprite": miner_sprite} if miner_sprite is not None else {}),
    }

    # Pretty-print with indent=2
    json_text = json.dumps(data, indent=2)

    # Post-process: collapse any array that only contains numbers/nulls to a single line.
    # This regex matches [ followed by whitespace, then a sequence of numbers/nulls and commas,
    # then whitespace and ]. It avoids arrays containing objects or other arrays.
    def collapse(m):
        # Remove all newlines and extra spaces inside the array
        return "[" + re.sub(r"\s+", " ", m.group(1)).strip() + "]"

    # We apply it to any array that doesn't contain { or [
    json_text = re.sub(r"\[\s+([^\[\]\{]*?)\s+\]", collapse, json_text)

    with open(out, "w") as f:
        f.write(json_text)
        f.write("\n")

    print(f"Written {out}")
    print(
        f"  levels={len(levels)} sprites={len(sprites)} npcs={len(npcs)} "
        f"miner={len(miners)} portal={len(portals)}"
    )


if __name__ == "__main__":
    main()
