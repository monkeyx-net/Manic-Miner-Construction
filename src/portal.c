#include <string.h>
#include "misc.h"
#include "game.h"

typedef struct {
    int     x, y;
    u16     gfx[16];
    u8      colour[2];
} PORTAL;

static PORTAL   portal_data[MAX_LEVELS];

void portal_set_entry(int idx, int x, int y, u16 gfx[16], u8 c0, u8 c1) {
    if (idx < 0 || idx >= MAX_LEVELS) return;
    portal_data[idx].x = x;
    portal_data[idx].y = y;
    memcpy(portal_data[idx].gfx, gfx, 16 * sizeof(u16));
    portal_data[idx].colour[0] = c0;
    portal_data[idx].colour[1] = c1;
}

static PORTAL       *portal_this;
static int          portal_tile;
static int          portal_flash;
static int          portal_is_ready;
static int          portal_show_swordfish;

static void do_portal_ticker() {
    portal_flash ^= 1;
}

void portal_ready() {
    portal_ticker = do_portal_ticker;

    portal_is_ready = 1;
}

void portal_sword_fish() {
    portal_show_swordfish = 1;
}

void portal_get_save_data(PORTAL_SAVE *d) {
    d->ready = portal_is_ready;
}

void portal_get_render_data(PORTAL_RENDER *out) {
    if (!portal_this) { out->gfx = NULL; return; }
    out->x        = portal_this->x;
    out->y        = portal_this->y;
    out->gfx      = portal_this->gfx;
    out->colour[0]= portal_this->colour[0];
    out->colour[1]= portal_this->colour[1];
    out->flash    = portal_flash;
    out->ready    = portal_is_ready;
    out->show_swordfish = portal_show_swordfish;
}

void portal_set_save_data(PORTAL_SAVE *d) {
    if (d->ready) {
        portal_ready();
    }
}

// When the portal is open and the miner is in its tile, moves on to victory or the next level.
void portal_drawer() {
    if (portal_is_ready == 0 || portal_tile != miner_tile) {
        return;
    }

    if (game_level >= num_levels - 1) {
        action = victory_action;
    } else {
        action = trans_action;
    }
}

void portal_init() {
    portal_this = &portal_data[game_level];

    portal_tile = portal_this->y * 32 + portal_this->x;

    portal_flash = 0;
    portal_is_ready = 0;
    portal_show_swordfish = 0;
}
