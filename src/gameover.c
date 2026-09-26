#include "common.h"
#include "misc.h"
#include "video.h"
#include "audio.h"
#include "game.h"
#include "replay.h"

static int      boot_ticks;
static int      gameover_active;

static u8       game_colors[4];
static u8       over_colors[4];

// Renders the colour-cycling "Game Over" text after the boot animation completes.
static void do_gameover_drawer() {
    if (boot_ticks < 96) {
        return;
    }

    {
        static const char * const gl[4] = {"G ", "a ", "m ", "e"};
        int x = 7 * 8;
        for (int i = 0; i < 4; i++)
            x = video_write_f(48, x, game_colors[i], gl[i], VIDEO_FONT_LARGE);
    }
    {
        static const char * const ol[4] = {"O ", "v ", "e ", "r"};
        int x = 18 * 8;
        for (int i = 0; i < 4; i++)
            x = video_write_f(48, x, over_colors[i], ol[i], VIDEO_FONT_LARGE);
    }
}

static void do_gameover_ticker() {
    int c = boot_ticks >> 2;
    game_colors[0] = c++ & 0x7;
    game_colors[1] = c++ & 0x7;
    game_colors[2] = c++ & 0x7;
    game_colors[3] = c++ & 0x7;
    over_colors[0] = c++ & 0x7;
    over_colors[1] = c++ & 0x7;
    over_colors[2] = c++ & 0x7;
    over_colors[3] = c++ & 0x7;
    boot_ticks++;
    if (boot_ticks == 256) {
        gameover_active = 0;
        action = title_action;
    }
}

static void do_gameover_init() {
    game_check_high_score();
    video_pixel_fill(0, 128 * WIDTH, 0x0);
    level_mark_all_hires_dirty();
    boot_ticks = 0;
    gameover_active = 1;
    audio_play(MUS_STOP);
    audio_sfx(SFX_GAMEOVER);
    ticker = do_gameover_ticker;
}

int game_is_gameover(void) {
    return gameover_active;
}

void gameover_get_render_data(GAMEOVER_RENDER *out) {
    out->active      = gameover_active;
    out->boot_x       = 15 * 8;
    out->boot_y       = boot_ticks & 126;
    out->boot_visible = gameover_active && (boot_ticks <= 96);
}

void gameover_action() {
    replay_stop_recording();
    responder = do_nothing;
    ticker = do_gameover_init;
    drawer = do_gameover_drawer;
    action = do_nothing;
}
