#include "misc.h"
#include "video.h"
#include "game.h"
#include "audio.h"

static int      lives_level;

// The level's ink fades from white to black on black paper. The ticker runs first and leaves
// lives_level at -1 on the last frame, which is clamped to black.
static u8 lives_ink(void) {
    return lives_level > 0 ? lives_level >> 1 : 0;
}

// The hi-res renderer draws the fade (see lives_get_colours). The low-res fills recolour the
// title picture that shows through The Final Barrier's void tiles.
static void do_lives_drawer() {
    video_level_ink_fill(lives_ink());
    game_extra_life();
    game_draw_air();
}

// Reports the colours the whole playfield is drawn in while the miner dies.
int lives_get_colours(u8 *paper, u8 *ink) {
    if (drawer != do_lives_drawer) return 0;
    *paper = 0x0;
    *ink = lives_ink();
    return 1;
}

static void do_lives_ticker() {
    if (lives_level-- > 0) {
        return;
    }
    if (game_lives == 0) {
        action = gameover_action;
        return;
    }

    action = game_action;
}

static void do_lives_init() {
    game_lives--;
    lives_level = 15;
    video_level_paper_fill(0x0);
    system_border(0x0);
    audio_sfx(SFX_DIE);
    ticker = do_lives_ticker;
}

void lives_action() {
    responder = do_nothing;
    ticker = do_lives_init;
    drawer = do_lives_drawer;
    action = do_nothing;
}
