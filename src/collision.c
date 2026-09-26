#include "misc.h"
#include "collision.h"

// Miner-versus-NPC collisions are pixel-perfect on the 256×192 low-res grid. Each frame the
// game records which pixels the NPC sprites cover, then tests the miner's sprite against
// them. The mask is kept apart from the video buffer, so collisions do not depend on what
// has been drawn or in what order.
static u8       npc_mask[WIDTH * HEIGHT];
static int      npc_pixels[8 * 16 * 16];
static int      npc_pixel_count;

// Forgets the NPC pixels recorded for the previous frame.
void collision_clear(void) {
    while (npc_pixel_count > 0) {
        npc_mask[npc_pixels[--npc_pixel_count]] = 0;
    }
}

// Records the set pixels of an NPC sprite drawn at pos.
void collision_add_npc(int pos, const u16 *line, int mirror) {
    int     pixel[16 * 16];
    int     count = sprite_pixels(pos, line, mirror, pixel);
    int     i;

    for (i = 0; i < count; i++) {
        if (npc_mask[pixel[i]] == 0) {
            npc_mask[pixel[i]] = 1;
            npc_pixels[npc_pixel_count++] = pixel[i];
        }
    }
}

// Returns 1 if any set pixel of a sprite at pos overlaps a recorded NPC pixel.
int collision_hits_npc(int pos, const u16 *line, int mirror) {
    int     pixel[16 * 16];
    int     count = sprite_pixels(pos, line, mirror, pixel);
    int     i;

    for (i = 0; i < count; i++) {
        if (npc_mask[pixel[i]]) {
            return 1;
        }
    }
    return 0;
}
