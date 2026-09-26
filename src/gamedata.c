#include <stdarg.h>
#include <stdio.h>
#include "common.h"
#include "game.h"
#include "json.h"
#include "gamedata.h"

// levels.json is checked as it is read: every field must be present and in range, and the
// npcs, miner and portal lists must have one entry per level. The first problem found stops
// the load, and game_data_error() says where it is.

static char     data_error[200];

const char *game_data_error(void) {
    return data_error;
}

static int fail(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(data_error, sizeof data_error, fmt, args);
    va_end(args);
    return 0;
}

// Reads the integer node at what, which must lie in lo..hi.
static int get_int(JSON *node, int lo, int hi, int *out, const char *what) {
    if (!json_is_int(node)) {
        return fail("%s is missing or not a number", what);
    }
    *out = json_int(node);
    if (*out < lo || *out > hi) {
        return fail("%s is %d, must be %d to %d", what, *out, lo, hi);
    }
    return 1;
}

// Reads a field of an object; see get_int.
static int get_field(JSON *obj, const char *key, int lo, int hi, int *out, const char *where) {
    char what[96];
    snprintf(what, sizeof what, "%s.%s", where, key);
    return get_int(json_get(obj, key), lo, hi, out, what);
}

// Checks that node at what is an array of exactly len entries.
static int check_array(JSON *node, int len, const char *what) {
    if (!json_is_array(node) || json_length(node) != len) {
        return fail("%s must be a list of %d entries", what, len);
    }
    return 1;
}

// Reads an array of len integers in lo..hi.
static int get_ints(JSON *arr, int len, int lo, int hi, int *out, const char *where) {
    char what[96];
    int  i;

    if (!check_array(arr, len, where)) return 0;
    for (i = 0; i < len; i++) {
        snprintf(what, sizeof what, "%s[%d]", where, i);
        if (!get_int(json_index(arr, i), lo, hi, &out[i], what)) return 0;
    }
    return 1;
}

// Reads a 16×16 sprite bitmap: 16 rows of 16 bits.
static int get_bitmap(JSON *arr, u16 bitmap[16], const char *where) {
    int rows[16], i;

    if (!get_ints(arr, 16, 0, 0xffff, rows, where)) return 0;
    for (i = 0; i < 16; i++) bitmap[i] = (u16)rows[i];
    return 1;
}

static int load_level(JSON *entry, int i) {
    char    where[48], what[96];
    int     data[512];
    u8      colours[10];
    int     types[10];
    int     j, value;
    JSON    *name, *sub;

    snprintf(where, sizeof where, "levels[%d]", i);

    name = json_get(entry, "name");
    if (json_string(name)[0] == '\0') {
        return fail("%s.name is missing", where);
    }

    snprintf(what, sizeof what, "%s.data", where);
    if (!get_ints(json_get(entry, "data"), 512, 0, 9, data, what)) return 0;

    sub = json_get(entry, "info");
    snprintf(what, sizeof what, "%s.info", where);
    if (!check_array(sub, 10, what)) return 0;
    for (j = 0; j < 10; j++) {
        snprintf(what, sizeof what, "%s.info[%d]", where, j);
        if (!get_field(json_index(sub, j), "colour", 0, 255, &value, what)) return 0;
        colours[j] = (u8)value;
        if (!get_field(json_index(sub, j), "type", T_ITEM, T_VOID, &types[j], what)) return 0;
    }

    level_set_entry(i, json_string(name), data, colours, types);

    // The air bar is 224 pixels; more air keeps it full for longer. 0 would kill at once.
    if (!get_field(entry, "air", 1, 65535, &value, where)) return 0;
    game_set_air(i, value);
    if (!get_field(entry, "border", 0, 15, &value, where)) return 0;
    game_set_border(i, value);
    return 1;
}

// A level's NPCs: 8 slots, each either null (empty) or a full NPC definition.
static int load_npcs(JSON *entry, int level, int sprites) {
    static const char *keys[] = {"x", "y", "min", "max", "move", "speed", "gfx", "ink", "nframes", "frame", "solar_powered_generator"};
    const int lo[] = {0,  0,  0,   0,   MOVE_NONE,   0,  0,           0,  0, 0, 0};
    const int hi[] = {31, 15, 255, 255, MOVE_EUGENE, 16, sprites - 1, 15, 7, 7, 1};
    char    where[48];
    int     v[11], slot, k;
    JSON    *r;

    snprintf(where, sizeof where, "npcs[%d]", level);
    if (!check_array(entry, 8, where)) return 0;

    npcs_clear_level(level);
    for (slot = 0; slot < 8; slot++) {
        r = json_index(entry, slot);
        snprintf(where, sizeof where, "npcs[%d][%d]", level, slot);
        if (json_is_null(r)) continue;
        for (k = 0; k < 11; k++) {
            if (!get_field(r, keys[k], lo[k], hi[k], &v[k], where)) return 0;
        }
        npcs_set_start(level, slot, v[0], v[1], v[2], v[3], v[4], v[5], v[6], (u8)v[7], v[8], v[9], v[10]);
    }
    return 1;
}

// Parses the entire levels.json file and populates level geometry, tile gfx/info,
// NPC sprites and start positions, miner start positions, and portal data.
// Returns 0 if the file cannot be read or any of it is missing or out of range.
int game_data_load(const char *path) {
    JSON    *root, *section, *entry;
    char    where[48];
    int     i, j, n, sprites, v[5];
    u16     frames[8][16];
    int     ok = 0;

    root = json_parse_file(path);
    if (!root) {
        if (json_error_line())
            fail("%s is not valid JSON (line %d)", path, json_error_line());
        else
            fail("%s cannot be read", path);
        return 0;
    }

    section = json_get(root, "levels");
    n = json_length(section);
    if (n < 1 || n > MAX_LEVELS) {
        fail("levels must be a list of 1 to %d levels", MAX_LEVELS);
        goto done;
    }
    for (i = 0; i < n; i++) {
        if (!load_level(json_index(section, i), i)) goto done;
    }
    game_set_num_levels(n);

    section = json_get(root, "sprites");
    sprites = json_length(section);
    if (sprites < 1 || sprites > MAX_NPC_SPRITES) {
        fail("sprites must be a list of 1 to %d sprites", MAX_NPC_SPRITES);
        goto done;
    }
    for (i = 0; i < sprites; i++) {
        entry = json_index(section, i);
        snprintf(where, sizeof where, "sprites[%d]", i);
        if (!check_array(entry, 8, where)) goto done;
        for (j = 0; j < 8; j++) {
            snprintf(where, sizeof where, "sprites[%d][%d]", i, j);
            if (!get_bitmap(json_index(entry, j), frames[j], where)) goto done;
        }
        npcs_set_sprite(i, frames);
    }

    section = json_get(root, "miner_sprite");
    if (!check_array(section, 4, "miner_sprite")) goto done;
    for (i = 0; i < 4; i++) {
        snprintf(where, sizeof where, "miner_sprite[%d]", i);
        if (!get_bitmap(json_index(section, i), frames[i], where)) goto done;
    }
    miner_set_sprite(frames);

    section = json_get(root, "npcs");
    if (!check_array(section, n, "npcs (one entry per level)")) goto done;
    for (i = 0; i < n; i++) {
        if (!load_npcs(json_index(section, i), i, sprites)) goto done;
    }

    section = json_get(root, "miner");
    if (!check_array(section, n, "miner (one entry per level)")) goto done;
    for (i = 0; i < n; i++) {
        entry = json_index(section, i);
        snprintf(where, sizeof where, "miner[%d]", i);
        if (!get_field(entry, "x", 0, 31, &v[0], where) ||
            !get_field(entry, "y", 0, 15, &v[1], where) ||
            !get_field(entry, "frame", 0, 3, &v[2], where) ||
            !get_field(entry, "dir", 0, 1, &v[3], where) ||
            !get_field(entry, "ink", 0, 15, &v[4], where)) goto done;
        miner_set_start(i, v[0], v[1], v[2], v[3], (u8)v[4]);
    }

    section = json_get(root, "portal");
    if (!check_array(section, n, "portal (one entry per level)")) goto done;
    for (i = 0; i < n; i++) {
        u16  gfx[16];
        int  colour[2];
        char what[64];

        entry = json_index(section, i);
        snprintf(where, sizeof where, "portal[%d]", i);
        snprintf(what, sizeof what, "%s.gfx", where);
        if (!get_bitmap(json_get(entry, "gfx"), gfx, what)) goto done;
        snprintf(what, sizeof what, "%s.colour", where);
        if (!get_ints(json_get(entry, "colour"), 2, 0, 15, colour, what)) goto done;
        if (!get_field(entry, "x", 0, 31, &v[0], where) ||
            !get_field(entry, "y", 0, 15, &v[1], where)) goto done;
        portal_set_entry(i, v[0], v[1], gfx, (u8)colour[0], (u8)colour[1]);
    }

    npcs_set_sprite_count(sprites);
    ok = 1;

done:
    json_free(root);
    return ok;
}
