#include <string.h>
#include "misc.h"
#include "video.h"
#include "game.h"

#define TILE1   369
#define TILE2   TILE1 + 32

typedef struct {
    u8      colour;
    int     type;
} INFO;

typedef struct {
    char    *name;
    int     data[512];
    INFO    info[10];
} LEVEL;

static LEVEL    level_data[MAX_LEVELS];

typedef struct {
    int     type;
    int     slot;       // gfx slot (column in tiles.png), or -1 for a blank tile
    u8      paper, ink;
    int     data;
    int     shape;
} TILE;

static TILE     level_tile[512];
static u8       level_hires_dirty[512];

static int       convey_phase;
static EVENT    do_wall;

static int      level_item_count, level_item_total;
static int      item_tile[512];

static u8       level_bg;

u8 level_get_bg() {
    return level_bg;
}

void level_get_render_tile(int pos, TILE_RENDER *out) {
    if (pos < 0 || pos >= 512) { out->slot = -1; return; }
    out->slot  = level_tile[pos].slot;
    out->paper = level_tile[pos].paper;
    out->ink   = level_tile[pos].ink;
    out->type  = level_tile[pos].type;
    out->data  = level_tile[pos].data;
    out->shape = level_tile[pos].shape;
}

void level_mark_tile_dirty(int tile) {
    if (tile >= 0 && tile < 512) {
        level_hires_dirty[tile] = 1;
    }
}

// Conveyors and part-collapsed floors change every tick, so they are redrawn every frame
// whether or not they are marked dirty.
int level_tile_is_animated(int type, int data) {
    return type == T_CONVEYL || type == T_CONVEYR || (type == T_COLLAPSE && data < 8);
}

int level_is_hires_dirty(int tile) {
    return (tile >= 0 && tile < 512) ? level_hires_dirty[tile] : 0;
}

void level_clear_hires_dirty(int tile) {
    if (tile >= 0 && tile < 512)
        level_hires_dirty[tile] = 0;
}

void level_mark_all_hires_dirty(void) {
    int i;
    for (i = 0; i < 512; i++)
        level_hires_dirty[i] = 1;
}

int level_get_solar_powered_generator_tile(int tile) {
    return level_tile[tile].data;
}

void level_set_solar_powered_generator_tile(int tile, int data) {
    level_tile[tile].data |= data;
}

// Clears the Solar Power Generator flags (beam, NPC, miner) from every space and item tile.
// Called each frame before the NPCs, miner and beam set them again, so the beam only ever
// sees where things are this frame. Tiles losing the beam are redrawn, so it leaves no trail.
void level_clear_solar_powered_generator_flags() {
    int cell;

    for (cell = 0; cell < 512; cell++) {
        if (level_tile[cell].type == T_SPACE || level_tile[cell].type == T_ITEM) {
            if (level_tile[cell].data & B_BEAM) {
                level_mark_tile_dirty(cell);
            }
            level_tile[cell].data = 0;
        }
    }
}

void level_tile_delete(int tile) {
    level_tile[tile].type = T_SPACE;
    level_tile[tile].slot = -1;
    level_tile[tile].paper = level_bg;
    level_tile[tile].ink = 0x0;
    level_tile[tile].shape = SHAPE_FULL;
    level_mark_tile_dirty(tile);
}

static void do_wall_tick() {
    level_tile[TILE2].data++;
    level_mark_tile_dirty(TILE1);
    level_mark_tile_dirty(TILE2);
    if (level_tile[TILE1].data-- > 0) {
        return;
    }

    level_tile_delete(TILE1);
    level_tile_delete(TILE2);
    npcs_barrel();

    do_wall = do_nothing;
}

void level_switch(int tile) {
    level_tile[tile].type = T_SWITCHON;
    level_tile[tile].ink = 0x4;
    level_tile[tile].slot++;
    level_mark_tile_dirty(tile);

    if (tile == 6) {
        level_tile[TILE1].data = 7;
        level_tile[TILE2].data = 1;
        level_tile[TILE1].shape = SHAPE_TOP_ROWS;
        level_tile[TILE2].shape = SHAPE_BOTTOM_ROWS;
        do_wall = do_wall_tick;
    } else if (tile == 18) { 
        level_tile_delete(79);
        level_tile_delete(80);
        npcs_kong();
    }
}

void level_collapse_tile(int tile) {
    level_tile[tile].data--;
    if (level_tile[tile].data > 0) {
        return;
    }

    level_tile_delete(tile);
}

int level_get_tile_type(int tile) {
    int type = level_tile[tile].type;

    if (type == T_VOID) {
        return T_SOLID;
    }

    return type;
}

int level_reduce_item_count() {
    return --level_item_count;
}

int level_all_items_collected() {
    return level_item_count == 0;
}

// Snapshots all 512 tile types, gfx offsets (slot * 8, or -1 for blank), collapse counters,
// and item count so a save-state or replay can restore exact mid-level tile state.
void level_get_save_data(LEVEL_SAVE *d) {
    int i;
    for (i = 0; i < 512; i++) {
        d->tile_type[i] = level_tile[i].type;
        d->tile_gfx[i] = level_tile[i].slot < 0 ? -1 : level_tile[i].slot * 8;
        d->collapse_data[i] = level_tile[i].data;
    }
    d->item_count = level_item_count;
}

// Restores tile types, gfx slots (from the offsets above), and collapse counters
// from a saved snapshot, marking every tile dirty for a full redraw.
void level_set_save_data(LEVEL_SAVE *d) {
    int i;
    for (i = 0; i < 512; i++) {
        level_tile[i].type = d->tile_type[i];
        level_tile[i].slot = d->tile_gfx[i] >= 0 && d->tile_gfx[i] < 10 * 8 ? d->tile_gfx[i] / 8 : -1;
        level_tile[i].data = d->collapse_data[i];
        level_mark_tile_dirty(i);
    }
    level_item_count = d->item_count;
}

int level_conveyor_phase(void) {
    return convey_phase;
}

void level_ticker() {
    convey_phase = (convey_phase + 1) & 3;

    do_wall();
}

// Steps each remaining item to its next flash colour.
void level_cycle_item_colours() {
    int     i;
    TILE    *tile;

    for (i = 0; i < level_item_total; i++) {
        tile = &level_tile[item_tile[i]];
        if (tile->type == T_ITEM) {
            tile->ink = (tile->ink & 3) + 3;
        }
    }
}

const char *level_get_name(int level) {
    if (level < 0 || level >= MAX_LEVELS) return "";
    return level_data[level].name;
}

static char level_names[MAX_LEVELS][64];

void level_set_entry(int idx, const char *name, int data[512], u8 colours[10], int types[10]) {
    int i;
    if (idx < 0 || idx >= MAX_LEVELS) return;
    strncpy(level_names[idx], name, 63);
    level_names[idx][63] = '\0';
    level_data[idx].name = level_names[idx];
    memcpy(level_data[idx].data, data, 512 * sizeof(int));
    for (i = 0; i < 10; i++) {
        level_data[idx].info[i].colour = colours[i];
        level_data[idx].info[i].type   = types[i];
    }
}

// Iterates all 512 tiles to build the runtime tile table, setting tile shapes, collapse state
// and item tracking, then fills the level name strip and initialises conveyors.
void level_init() {
    int     cell;
    LEVEL   *level = &level_data[game_level];
    int     *data = level->data;
    INFO    *info;
    TILE    *tile = &level_tile[0];
    level_item_count = 0;

    for (cell = 0; cell < 512; cell++, tile++, data++) {
        level_mark_tile_dirty(cell);
        info = &level->info[*data];
        tile->type = info->type;
        tile->slot = *data;
        tile->shape = SHAPE_FULL;
        if (info->type != T_VOID) {
            tile->data = 0;
            tile->ink = info->colour & 0x0f;
            tile->paper = info->colour >> 4;
        }

        if (info->type == T_ITEM) {
            tile->ink = (level_item_count & 3) + 3;
            item_tile[level_item_count] = cell;
            level_item_count++;
        } else if (info->type == T_COLLAPSE) {
            tile->data = 8;
            tile->shape = SHAPE_TOP_ROWS;
        }
    }
    level_item_total = level_item_count;

    // The playfield is drawn by the hi-res renderer. The low-res layer beneath only shows
    // through The Final Barrier's void tiles, where it holds the title picture.
    video_pixel_fill(0, 128 * WIDTH, C_BLACK);
    if (game_level == TWENTY) {
        title_screen_copy();
    }

    do_wall = do_nothing;

    level_bg = level->info[0].colour >> 4;

    convey_phase = 0;

    kong_fallen = 0;

    video_pixel_fill(122 * WIDTH, video_text_height_f(VIDEO_FONT_SMALL) * WIDTH, 0x7);
    video_write(122 * WIDTH + (WIDTH - video_text_width_f(level->name, VIDEO_FONT_SMALL)) / 2, C_BLACK, level->name, VIDEO_FONT_SMALL);
}
