#include "common.h"
#include "game.h"
#include "video.h"

// Traces the Solar Power Generator beam tile-by-tile, reversing direction when it hits an NPC; deals
// air damage when the beam passes through the miner's tile. Beam tiles are flagged B_BEAM
// and marked dirty so the hi-res renderer draws them.
void do_solar_powered_generator_drawer() {
    u16     tile = 23, dir = 32;
    int air = 0;
    int this;

    do {
        level_mark_tile_dirty(tile);
        level_set_solar_powered_generator_tile(tile, B_BEAM);

        this = level_get_solar_powered_generator_tile(tile);

        if (this & B_NPC) {
            dir ^= ((255 << 8) | 223);
        }

        if (this & B_MINER) {
            air = 8;
        }

        tile += dir;
    }
    while (level_get_tile_type(tile) == T_SPACE);

    level_mark_tile_dirty(tile);

    game_reduce_air(air);
}
