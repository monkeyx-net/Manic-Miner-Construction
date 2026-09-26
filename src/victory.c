#include "misc.h"
#include "video.h"
#include "game.h"
#include "audio.h"
#include "replay.h"

static int      victory_timer;
static int      victory_active;

int game_is_victory(void) {
    return victory_active;
}

static void do_victory_ticker() {
    if (victory_timer-- == 0) {
        victory_active = 0;
        action = trans_action;
    }
}

static void do_victory_init() {
    system_border(0);

    victory_active = 1;

    portal_sword_fish();
    level_mark_all_hires_dirty();

    victory_timer = 50 * 9;

    audio_play(MUS_STOP);
    audio_sfx(SFX_VICTORY);

    ticker = do_victory_ticker;
}

void victory_action() {
    replay_stop_recording();
    responder = do_nothing;
    ticker = do_victory_init;
    drawer = do_nothing;

    action = do_nothing;
}
