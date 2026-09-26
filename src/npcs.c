#include <string.h>
#include "common.h"
#include "game.h"
#include "video.h"
#include "audio.h"
#include "collision.h"

typedef struct {
    u8      x, y;
    int     min, max;
    EVENT   DoMove;
    int     active;
    EVENT   DoSolarPoweredGenerator;
    int     speed;
    int     gfx;
    u8      ink;
    int     nframes; 
    int     frame;
    int     tile;
} NPC;

static u16      npc_sprite[MAX_NPC_SPRITES][8][16];
static int      npc_sprite_count;

static void do_npc_left(void);
static void do_npc_right(void);
static void do_npc_up(void);
static void do_npc_down(void);
static void do_npc_kong(void);
static void do_npc_skylab(void);
static void do_npc_fall(void);
static void do_npc_eugene(void);
static void do_npc_solar_powered_generator(void);

static NPC    npc_start[MAX_LEVELS][8];
static int      npc_align[8] = {4, 6, 6, 6, 6, 6, 6, 6};
static NPC    npc_this[8], *cur_npc;

// Steps an NPC one animation frame in the given direction (±1), reversing travel at min/max x bounds.
static void do_npc_horizontal(int dir) {
    if (game_ticks & cur_npc->speed) return;
    cur_npc->frame += dir;
    if ((dir > 0) == (cur_npc->frame < 4)) return;
    if (dir > 0 ? cur_npc->x < cur_npc->max : cur_npc->x > cur_npc->min) {
        cur_npc->x += 8 * dir;
        cur_npc->tile += dir;
        cur_npc->frame = (dir > 0) ? 0 : 7;
        return;
    }
    cur_npc->DoMove = (dir > 0) ? do_npc_left : do_npc_right;
    cur_npc->frame  = (dir > 0) ? 7 : 0;
}

// Moves an NPC vertically by its speed each tick, reversing at bounds and updating the
// tile coordinate when crossing an 8-pixel row boundary.
static void do_npc_vertical(int dir) {
    int pos = cur_npc->y + dir * cur_npc->speed;
    if (dir < 0 ? pos < cur_npc->min : pos >= cur_npc->max) {
        cur_npc->DoMove = (dir < 0) ? do_npc_down : do_npc_up;
    } else {
        if ((pos & 120) != (cur_npc->y & 120)) {
            cur_npc->tile += dir * 32;
        }
        cur_npc->y = pos;
    }
    cur_npc->frame++;
}

static void do_npc_solar_powered_generator(void) {
    int     count;
    int     tile = cur_npc->tile, adj = 1;

    for (count = 0; count < npc_align[cur_npc->y & 7]; count++, tile += adj, adj ^= 30) {
        level_set_solar_powered_generator_tile(tile, B_NPC);
    }
}

void npcs_barrel() {
    npc_this[2].max = 18 * 8;
}

static void do_npc_left(void)  { do_npc_horizontal(-1); }
static void do_npc_right(void) { do_npc_horizontal(1);  }
static void do_npc_up(void)    { do_npc_vertical(-1);   }
static void do_npc_down(void)  { do_npc_vertical(1);    }

static void do_npc_kong() {
    npc_this[0].frame &= 2;
    npc_this[0].frame |= ((game_ticks >> 3) & 1);
}

static void do_npc_fall() {
    if (cur_npc->y == cur_npc->max) {
        cur_npc->DoMove = do_nothing;
        cur_npc->active = 0;
        if (game_level == 7 || game_level == 11) {
            kong_fallen = 1;
            if (level_all_items_collected()) {
                portal_ready();
            }
        }
    } else {
        cur_npc->y += cur_npc->speed;
        do_npc_kong();
        game_score_add(100);
    }
}

void npcs_kong() {
    npc_this[0].frame |= 2;
    npc_this[0].nframes |= 2;
    npc_this[0].ink = 0xf;
    npc_this[0].DoMove = do_npc_fall;

    audio_sfx(SFX_KONG);
}

static void do_npc_eugene() {
    if (cur_npc->y < cur_npc->max) {
        cur_npc->y += cur_npc->speed;
    }

    cur_npc->ink = (cur_npc->ink - 1) & 0x7;
}

void npcs_eugene() {
    npc_this[2].DoMove = do_npc_eugene;
}

static void do_npc_skylab() {
    if (cur_npc->y < cur_npc->max) {
        cur_npc->y += cur_npc->speed;
    } else if (++cur_npc->frame == 8) {
        cur_npc->frame = 0;
        cur_npc->x += 64;
        cur_npc->y = cur_npc->min;
    }
}

void npcs_ticker() {
    int count;

    cur_npc = &npc_this[0];

    for (count = 0; count < 8; count++, cur_npc++) {
        cur_npc->DoMove();
    }
}

// Horizontal walkers with an 8-frame cycle are drawn like the miner: frames 0-3 face
// right, and left-facing frames 4-7 are frames 3-0 mirrored instead of separate art.
static int npc_is_mirrored(const NPC *npc, int *fi) {
    *fi = npc->frame & npc->nframes;
    if (npc->nframes != 7 || *fi < 4) return 0;
    if (npc->DoMove != do_npc_left && npc->DoMove != do_npc_right) return 0;
    *fi = 7 - *fi;
    return 1;
}

// Returns the NPC's current sprite frame, and sets where it is drawn and whether mirrored.
static const u16 *npc_frame(const NPC *npc, int *pos, int *mirror) {
    int fi;
    *mirror = npc_is_mirrored(npc, &fi);
    *pos = (npc->y << 8) | npc->x;
    return npc_sprite[npc->gfx][fi];
}

// Marks the tiles each NPC stands in for the Solar Power Generator beam.
void npcs_mark_solar_powered_generator_tiles() {
    int count;

    cur_npc = &npc_this[0];

    for (count = 0; count < 8; count++, cur_npc++) {
        cur_npc->DoSolarPoweredGenerator();
    }
}

// Records where every visible NPC is this frame for the miner's collision test.
void npcs_mark_collisions() {
    int i, pos, mirror;
    const u16 *gfx;

    collision_clear();
    for (i = 0; i < 8; i++) {
        if (!npc_this[i].active) continue;
        gfx = npc_frame(&npc_this[i], &pos, &mirror);
        collision_add_npc(pos, gfx, mirror);
    }
}

void npcs_version(int version) {
    npc_start[16][2].gfx = 25 + version;
    npc_start[16][3].gfx = 25 + version;
    npc_start[16][4].gfx = 25 + version;
    npc_start[16][5].gfx = 25 + version;
    npc_start[17][4].gfx = 27 + version;
    npc_start[17][5].gfx = 27 + version;
    npc_start[17][6].gfx = 27 + version;
    npc_start[17][7].gfx = 27 + version;
}

static int move_to_id(EVENT fn) {
    if (fn == do_npc_left) return MOVE_LEFT;
    if (fn == do_npc_right) return MOVE_RIGHT;
    if (fn == do_npc_up) return MOVE_UP;
    if (fn == do_npc_down) return MOVE_DOWN;
    if (fn == do_npc_kong) return MOVE_KONG;
    if (fn == do_npc_skylab) return MOVE_SKYLAB;
    if (fn == do_npc_fall) return MOVE_FALL;
    if (fn == do_npc_eugene) return MOVE_EUGENE;
    return MOVE_NONE;
}

static EVENT id_to_move(int id) {
    switch (id) {
        case MOVE_LEFT: return do_npc_left;
        case MOVE_RIGHT: return do_npc_right;
        case MOVE_UP: return do_npc_up;
        case MOVE_DOWN: return do_npc_down;
        case MOVE_KONG: return do_npc_kong;
        case MOVE_SKYLAB: return do_npc_skylab;
        case MOVE_FALL: return do_npc_fall;
        case MOVE_EUGENE: return do_npc_eugene;
    }
    return do_nothing;
}

void npcs_get_save_data(NPC_SAVE d[8]) {
    int i;
    for (i = 0; i < 8; i++) {
        d[i].x = npc_this[i].x;
        d[i].y = npc_this[i].y;
        d[i].frame = npc_this[i].frame;
        d[i].tile = npc_this[i].tile;
        d[i].subpix = 0; 
        d[i].nframes = npc_this[i].nframes;
        d[i].ink = npc_this[i].ink;
        d[i].move = move_to_id(npc_this[i].DoMove);
        d[i].active = npc_this[i].active;
    }
}

void npcs_get_render_data(NPC_RENDER out[8]) {
    int i;
    for (i = 0; i < 8; i++) {
        int fi;
        out[i].x      = npc_this[i].x;
        out[i].y      = npc_this[i].y;
        out[i].ink    = npc_this[i].ink;
        out[i].active = npc_this[i].active;
        out[i].gfx    = npc_this[i].gfx;
        if (out[i].active && npc_this[i].gfx >= 0 && npc_this[i].gfx < MAX_NPC_SPRITES) {
            out[i].mirror = npc_is_mirrored(&npc_this[i], &fi);
            out[i].frame = fi;
        } else {
            out[i].frame = 0;
            out[i].mirror = 0;
        }
    }
}

void npcs_set_save_data(NPC_SAVE d[8]) {
    int i;
    for (i = 0; i < 8; i++) {
        npc_this[i].x = d[i].x;
        npc_this[i].y = d[i].y;
        npc_this[i].frame = d[i].frame;
        npc_this[i].tile = d[i].tile;
        npc_this[i].nframes = d[i].nframes;
        npc_this[i].ink = d[i].ink;
        if (d[i].active) {
            npc_this[i].DoMove = id_to_move(d[i].move);
            npc_this[i].active = 1;
        } else {
            npc_this[i].DoMove = do_nothing;
            npc_this[i].active = 0;
        }
    }
}

void npcs_set_sprite(int idx, u16 frames[8][16]) {
    if (idx < 0 || idx >= MAX_NPC_SPRITES) return;
    memcpy(npc_sprite[idx], frames, 8 * 16 * sizeof(u16));
}

// How many NPC sprites levels.json defines; npcs.png must have a row for each.
int npcs_sprite_count(void) {
    return npc_sprite_count;
}

void npcs_set_sprite_count(int count) {
    npc_sprite_count = count;
}

void npcs_clear_level(int level) {
    int i;
    if (level < 0 || level >= MAX_LEVELS) return;
    for (i = 0; i < 8; i++) {
        NPC *r = &npc_start[level][i];
        memset(r, 0, sizeof *r);
        r->DoMove = do_nothing;
        r->active = 0;
        r->DoSolarPoweredGenerator = do_nothing;
    }
}

void npcs_set_start(int level, int slot, int x, int y, int min, int max,
                     int move_id, int speed, int gfx_idx, u8 ink,
                     int nframes, int frame, int solar_powered_generator) {
    NPC *r;
    if (level < 0 || level >= MAX_LEVELS || slot < 0 || slot >= 8) return;
    r = &npc_start[level][slot];
    r->x       = (u8)x;
    r->y       = (u8)y;
    r->min     = min;
    r->max     = max;
    r->DoMove  = id_to_move(move_id);
    r->active  = 1;
    r->DoSolarPoweredGenerator = solar_powered_generator ? do_npc_solar_powered_generator : do_nothing;
    r->speed   = speed;
    r->gfx     = gfx_idx;
    r->ink     = ink;
    r->nframes = nframes;
    r->frame   = frame;
    r->tile    = 0;
}

// Copies start positions into the live NPC array, applying Skylab (start at min y)
// and Solar Power Generator (tile coordinate from grid position) special-case positioning.
void npcs_init() {
    int     count;
    NPC   *npc = &npc_this[0], *start = &npc_start[game_level][0];
    for (count = 0; count < 8; count++, npc++, start++) {
        npc->DoMove = start->DoMove;
        npc->active = start->active;
        npc->DoSolarPoweredGenerator = start->DoSolarPoweredGenerator;
        npc->x = start->x * 8;
        if (game_level == SKYLAB) {
            npc->y = start->min;
        } else {
            npc->y = start->y * 8;
            if (game_level == SOLAR_POWERED_GENERATOR) {
                npc->tile = start->y * 32 + start->x;
            }
        }
        npc->min = start->min;
        npc->max = start->max;
        npc->speed = start->speed;
        npc->gfx = start->gfx;
        npc->ink = start->ink;
        npc->nframes = start->nframes;
        npc->frame = start->frame;
    }
}
