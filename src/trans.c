#include "common.h"
#include "video.h"
#include "game.h"
#include "audio.h"

static int      trans_level;

// The playfield cycles through paper and ink colours as trans_level counts down from 63, then
// holds the colours of level 1 while the remaining air is added to the score.
static int trans_colour_level(void) {
    return trans_level > 0 ? trans_level : 1;
}

static void do_trans_drawer(void);

// Reports the colours the whole playfield is drawn in between levels.
int trans_get_colours(u8 *paper, u8 *ink) {
    if (drawer != do_trans_drawer) return 0;
    *paper = trans_colour_level() >> 3;
    *ink = trans_colour_level() & 0x7;
    return 1;
}

// The hi-res renderer draws the colour cycle (see trans_get_colours). The low-res fills
// recolour the title picture that shows through The Final Barrier's void tiles.
static void do_trans_drawer() {
    if (game_demo == 0) {
        game_draw_air();
        game_extra_life();
    }

    if (trans_level > 0) {
        video_level_paper_fill(trans_level >> 3);
        video_level_ink_fill(trans_level & 0x7);
    }
}

// Converts remaining air to score after completing a level, then advances to the next level
// (or wraps to 0); in demo mode skips air bonus.
static void do_trans_ticker() {
    if (trans_level > 1) {
        trans_level--;
        return;
    }
    if (game_demo == 0) {
        if (trans_level == 1) {
            trans_level--;
            audio_sfx(SFX_AIR);
        }
        if (game_air > 8) {
            game_score_add(8);
            game_reduce_air(8);
        } else if (game_air > 0) {
            game_score_add(game_air);
            game_reduce_air(game_air);
        }
        if (game_air_old > 0) {
            return;
        }
    }
    if (game_demo == 0) {
        int score, hiscore;
        game_get_scores(&score, &hiscore);
        game_update_level_hi_score(game_level, score);
    }
    if (game_level++ >= num_levels - 1) {
        game_level = 0;
    }
    action = game_action;
}

static void do_trans_init() {
    trans_level = 63;

    if (game_demo == 0) {
        audio_play(MUS_STOP);
    }

    ticker = do_trans_ticker;
}

static void do_trans_responder() {
    action = title_action;
}

void trans_action() {
    responder = game_demo ? do_trans_responder : do_nothing;
    ticker = do_trans_init;
    drawer = do_trans_drawer;

    action = do_nothing;
}
