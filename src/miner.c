#include <string.h>
#include "misc.h"
#include "video.h"
#include "collision.h"
#include "game.h"
#include "audio.h"

#define D_RIGHT     0
#define D_LEFT      1
#define D_JUMP      2

typedef struct {
    int     x, y;
    int     frame, dir;
    u8      ink;
} START;

static START    miner_start[MAX_LEVELS];

void miner_set_start(int idx, int x, int y, int frame, int dir, u8 ink) {
    if (idx < 0 || idx >= MAX_LEVELS) return;
    miner_start[idx].x     = x;
    miner_start[idx].y     = y;
    miner_start[idx].frame = frame;
    miner_start[idx].dir   = dir;
    miner_start[idx].ink   = ink;
}

typedef struct {
    int     y;
    int     tile;
    int     align;
} JUMP;

static JUMP     jump_info[18] = {
    {.y = -4, .tile = -32, .align = 6},
    {.y = -4, .tile =   0, .align = 4},
    {.y = -3, .tile = -32, .align = 6},
    {.y = -3, .tile =   0, .align = 6},
    {.y = -2, .tile =   0, .align = 4},
    {.y = -2, .tile = -32, .align = 6},
    {.y = -1, .tile =   0, .align = 6},
    {.y = -1, .tile =   0, .align = 6},
    {.y =  0, .tile =   0, .align = 6},
    {.y =  0, .tile =   0, .align = 6},
    {.y =  1, .tile =   0, .align = 6},
    {.y =  1, .tile =   0, .align = 6},
    {.y =  2, .tile =  32, .align = 4},
    {.y =  2, .tile =   0, .align = 6},
    {.y =  3, .tile =   0, .align = 6},
    {.y =  3, .tile =  32, .align = 4},
    {.y =  4, .tile =   0, .align = 6},
    {.y =  4, .tile =  32, .align = 4}
};

static u16      miner_sprite[4][16];

void miner_set_sprite(u16 frames[4][16]) {
    memcpy(miner_sprite, frames, 4 * 16 * sizeof(u16));
}

static int      miner_frame, miner_dir, miner_move;
static int      miner_air, jump_stage;
static u8       miner_ink;

u8              miner_x, miner_y;
int             miner_tile, miner_align;

static u8           miner_seq_index;
static TIMER        miner_seq_timer;

static const int    miner_sequence[8] = {0, 1, 2, 3, 0, 1, 2, 3};

void miner_set_seq(int index, int speed) {
    timer_set(&miner_seq_timer, 1, speed);
    miner_seq_index = miner_sequence[index];
}

void miner_inc_seq() {
    miner_seq_index += timer_update(&miner_seq_timer);
    miner_seq_index &= 7;
}

void miner_get_seq_render_data(MINER_RENDER *out) {
    int idx = miner_seq_index & 7;
    out->sprite_idx = miner_sequence[idx];
    out->mirror    = (idx >= 4);
    out->x         = 0;
    out->y         = 0;
    out->ink       = 0;
}

void miner_get_save_data(MINER_SAVE *d) {
    d->x = miner_x; d->y = miner_y; d->tile = miner_tile;
    d->align = miner_align; d->frame = miner_frame; d->dir = miner_dir;
    d->air = miner_air; d->jump_stage = jump_stage; d->move = miner_move; d->ink = miner_ink;
}

void miner_set_save_data(MINER_SAVE *d) {
    miner_x = d->x; miner_y = d->y; miner_tile = d->tile;
    miner_align = d->align; miner_frame = d->frame; miner_dir = d->dir;
    miner_air = d->air; jump_stage = d->jump_stage; miner_move = d->move; miner_ink = d->ink;
}

void miner_get_render_data(MINER_RENDER *out) {
    out->x         = miner_x;
    out->y         = miner_y;
    out->sprite_idx = miner_frame & 3;
    out->mirror    = (miner_dir == D_LEFT);
    out->ink       = miner_ink;
}

static int IsSolid(int tile) {
    if (level_get_tile_type(tile) == T_SOLID) {
        return 1;
    }
    if (level_get_tile_type(tile + 32) == T_SOLID) {
        return 1;
    }
    if (level_get_tile_type(tile + 64) == T_SOLID) {
        if (miner_align == 6) {
            return 1;
        }
        if (miner_air == 1 && jump_stage > 9) {
            miner_air = 0;
        }
    }
    return 0;
}

static void MoveLeftRight() {
    if (miner_move == 0) return;
    if (miner_frame < 3) {
        miner_frame++;
        return;
    }
    if (miner_dir == D_LEFT) {
        if (IsSolid(miner_tile - 1)) return;
        miner_tile--;
        miner_x -= 8;
    } else {
        if (IsSolid(miner_tile + 2)) return;
        miner_tile++;
        miner_x += 8;
    }
    miner_frame = 0;
}

// Implements the full miner physics: jump arc from the jump_info table, floor/conveyor
// interactions, collapse tile triggers, left/right movement, and free-fall gravity.
void do_miner_ticker() {
    int     i, tile, convey_dir = C_NONE, type[2];
    JUMP    *jump;

    if (miner_air == 1) {
        jump = &jump_info[jump_stage];
        tile = miner_tile + jump->tile;
        if (level_get_tile_type(tile) == T_SOLID || level_get_tile_type(tile + 1) == T_SOLID) {
            miner_air = 2;
            miner_move = 0;
            return;
        }
        if (jump_stage == 0)
            audio_sfx(SFX_JUMP);
        miner_y += jump->y;
        miner_tile = tile;
        miner_align = jump->align;
        jump_stage++;
        if (jump_stage == 18) {
            miner_air = 6;
            if (level_get_tile_type(miner_tile + 64) <= T_SPACE &&
                level_get_tile_type(miner_tile + 65) <= T_SPACE)
                audio_sfx(SFX_FALL);
            return;
        }
        if (jump_stage != 13 && jump_stage != 16) {
            MoveLeftRight();
            return;
        }
    }

    if (miner_align == 4) {
        tile = miner_tile + 64;
        type[0] = level_get_tile_type(tile);
        type[1] = level_get_tile_type(tile + 1);
        if (type[0] == T_COLLAPSE) {
            level_collapse_tile(tile);
        }
        if (type[1] == T_COLLAPSE) {
            level_collapse_tile(tile + 1);
        }
        if (type[0] == T_HARM || type[1] == T_HARM) {
            if (miner_air == 1 && (type[0] <= T_SPACE || type[1] <= T_SPACE)) {
                MoveLeftRight();
            } else {
                action = lives_action;
            }
            return;
        }
        if (type[0] > T_SPACE || type[1] > T_SPACE) {
            if (miner_air >= 12) {
                action = lives_action;
                return;
            }
            miner_air = 0;
            audio_fall_halt();
            if (type[0] == T_CONVEYL || type[1] == T_CONVEYL) {
                convey_dir = C_LEFT;
            } else if (type[0] == T_CONVEYR || type[1] == T_CONVEYR) {
                convey_dir = C_RIGHT;
            }
            i = 0;
            if (system_is_key(KEY_LEFT) || convey_dir == C_LEFT) {
                i += 1;
            }
            if (system_is_key(KEY_RIGHT) || convey_dir == C_RIGHT) {
                i += 2;
            }
            if (i == 0) {
                miner_move = 0;
            } else if (i == 1) {
                // Frame f mirrored sits where frame 3-f does, so flip the frame on a turn to keep the miner in place.
                if (miner_dir == D_RIGHT) {
                    miner_dir = D_LEFT;
                    miner_frame = 3 - miner_frame;
                    miner_move = 0;
                } else {
                    miner_move = 1;
                }
            } else if (i == 2) {
                if (miner_dir == D_LEFT) {
                    miner_dir = D_RIGHT;
                    miner_frame = 3 - miner_frame;
                    miner_move = 0;
                } else {
                    miner_move = 1;
                }
            }
            if (system_is_key(KEY_JUMP)) {
                miner_air = 1;
                jump_stage = 0;
            }
            MoveLeftRight();
            return;
        }
    }
    if (miner_air == 1) {
        MoveLeftRight();
        return;
    }

    miner_move = 0;
    if (miner_air == 0) {
        miner_air = 2;
        return;
    }

    miner_air++;
    if (miner_air == 3)
        audio_sfx(SFX_FALL);
    miner_y += 4;
    miner_align = 4;
    if (miner_y & 7) {
        miner_align = 6;
    } else {
        miner_tile += 32;
    }
}

// miner_sprite rows are stored least-significant bit leftmost, the reverse of NPC rows,
// so the right-facing miner is the mirrored bitmap.
static int miner_bitmap_mirror() {
    return miner_dir != D_LEFT;
}

// Tests the miner against the NPCs marked by npcs_mark_collisions, then iterates occupied
// tiles for hazards, items, switches and the Solar Power Generator beam.
void do_miner_drawer() {
    int     tile, adj;
    int     i;

    if (collision_hits_npc((miner_y << 8) | miner_x, miner_sprite[miner_frame], miner_bitmap_mirror())) {
        action = lives_action;
        return;
    }

    tile = miner_tile;
    for (i = 0, adj = 1; i < miner_align; i++, tile += adj, adj ^= 30) {
        if (level_get_tile_type(tile) == T_HARM) {
            action = lives_action;
            return;
        }
    }

    tile = miner_tile;
    for (i = 0, adj = 1; i < miner_align; i++, tile += adj, adj ^= 30) {
        switch (level_get_tile_type(tile)) {
          case T_ITEM:
            game_got_item(tile);
            break;

          case T_SWITCHOFF:
            level_switch(tile);
            break;

          case T_SPACE:
            level_set_solar_powered_generator_tile(tile, B_MINER);
            break;
        }
    }
}

void miner_init() {
    START   *start = &miner_start[game_level];

    miner_x = start->x * 8;
    miner_y = start->y * 8;
    miner_tile = start->y * 32 + start->x;
    miner_align = 4;
    miner_frame = start->frame;
    miner_dir = start->dir;
    miner_move = 0;
    miner_air = 0;

    miner_ink = start->ink;
}
