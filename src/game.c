#include <stdio.h>
#include <string.h>
#include "misc.h"
#include "audio.h"
#include "video.h"
#include "replay.h"
#include "savestate.h"

#include "game.h"

static void do_game_drawer(void);

#define AIR     139 * WIDTH + 28

static int      game_music = MUS_PLAY;

int             num_levels = 20;

static int      air_data[MAX_LEVELS] = {224, 224, 220, 220, 220, 220, 220, 220, 220, 224, 220, 220, 224, 224, 224, 224, 220, 220, 224, 224};

static int      level_border[MAX_LEVELS] = {C_DARK_RED, C_BLUE, C_MID_RED, C_DARK_GREEN, C_DARK_RED, C_BLACK, C_GREEN, C_LIGHT_BLUE, C_BLUE, C_MID_BLUE, C_BLUE, C_LIGHT_BLUE, C_MID_RED, C_BLUE, C_DARK_RED, C_DARK_GREEN, C_GREY, C_BLUE, C_BLACK, C_LIGHT_BLUE};

static int      game_score, game_hi_score = 0;
static int      game_extra_life_count;

static int      game_frame;
static TIMER    game_timer;

int             game_paused = 0;
int             game_level;
int             game_ticks;
int             game_demo = 1;
int             game_lives;
int             game_air, game_air_old;
int             kong_fallen = 0;

EVENT           game_extra_life = do_nothing;
EVENT           game_draw_air = do_nothing;

EVENT           solar_powered_generator_drawer;
EVENT           miner_ticker, miner_drawer;
EVENT           portal_ticker;

static int level_hi_scores[MAX_LEVELS]; 

int game_get_level_hi_score(int level) {
    if (level < 0 || level >= MAX_LEVELS) return 0;
    return level_hi_scores[level];
}

void game_update_level_hi_score(int level, int score) {
    if (level < 0 || level >= MAX_LEVELS) return;
    if (score > level_hi_scores[level])
        level_hi_scores[level] = score;
}

void game_set_level_hi_score(int level, int score) {
    if (level < 0 || level >= MAX_LEVELS) return;
    level_hi_scores[level] = score;
}

void game_set_num_levels(int n) { if (n > 0 && n <= MAX_LEVELS) num_levels = n; }
void game_set_air(int level, int air) { if (level >= 0 && level < MAX_LEVELS) air_data[level] = air; }
void game_set_border(int level, int border) { if (level >= 0 && level < MAX_LEVELS) level_border[level] = border; }

static int save_flash_timer = -1;  
static int save_flash_slot  =  0;

void game_start_save_flash(int slot) {
    save_flash_slot  = slot;
    save_flash_timer = 15;
}

void game_draw_save_osd(void) {
    if (save_flash_timer >= 0) {
        char buf[8];
        snprintf(buf, sizeof(buf), "SAVE%d", save_flash_slot);
        u8 ink = (save_flash_timer > 0) ? C_RED : C_BLACK;
        video_write_f(SCORE / WIDTH, 112, ink, buf, VIDEO_FONT_LARGE);
        if (save_flash_timer == 0) save_flash_timer = -1;
        else save_flash_timer--;
    }
}

void game_check_high_score() {
    if (game_score > game_hi_score) {
        game_hi_score = game_score;
    }
}

void game_get_scores(int *score, int *hiscore) {
    if (score) *score = game_score;
    if (hiscore) *hiscore = game_hi_score;
}

void game_set_scores(int score, int hiscore) {
    game_score = score;
    game_hi_score = hiscore;
    game_draw_score();
    game_draw_hi_score();
}

// Draws the static HUD below the level: the level-name strip background, the air bar
// label and the High/Score labels.
void game_draw_hud(void) {
    int x;
    video_pixel_fill(127 * WIDTH, 18 * WIDTH, 0x7);
    video_pixel_fill(145 * WIDTH, 47 * WIDTH, C_BLACK);
    video_write(130 * WIDTH + 3, C_BLUE, "AIR", VIDEO_FONT_SMALL);
    x = video_write_f(SCORE / WIDTH, 4, C_YELLOW, "High ....0      ", VIDEO_FONT_LARGE);
    video_write_f(SCORE / WIDTH, x, C_ORANGE, "Score ....0", VIDEO_FONT_LARGE);
}

static void do_draw_air() {
    game_air_old--;
    video_air_bar(AIR + game_air_old, 0x7);
    if (game_air_old == ((game_air + 7) / 8)) {
        game_draw_air = do_nothing;
    }
}

void game_reduce_air(int amount) {
    game_air -= amount;
    if (game_air < 0) {
        game_air = 0;
    }
    if (game_air_old > ((game_air + 7) / 8)) {
        game_draw_air = do_draw_air;
    }
}

static void draw_score(int pos, int x, int score, u8 ink) {
    int  digit = 5, i = 4;
    char text[6] = "....0";
    int  row = pos / WIDTH;
    int  w, h, r;

    while (digit-- && score) {
        text[i--] = '0' + (score % 10);
        score /= 10;
    }

    w = video_text_width_f(text, VIDEO_FONT_LARGE);
    h = video_text_height_f(VIDEO_FONT_LARGE);
    for (r = 0; r < h; r++)
        video_pixel_fill(row * WIDTH + x + r * WIDTH, w, C_BLACK);

    video_write_f(row, x, ink, text, VIDEO_FONT_LARGE);
}

void game_draw_score() {
    int x = 4 + video_text_width_f("High ....0      Score ", VIDEO_FONT_LARGE);
    draw_score(SCORE, x, game_score, C_ORANGE);
}

void game_draw_hi_score() {
    int x = 4 + video_text_width_f("High ", VIDEO_FONT_LARGE);
    draw_score(SCORE, x, game_hi_score, C_YELLOW);
}

static void do_extra_life() {
    game_extra_life_count--;
    system_border(game_extra_life_count >> 2);
    if (game_extra_life_count > 0) {
        return;
    }
    system_border(level_border[game_level]);
    game_extra_life = do_nothing;
}

// Adds to the score and triggers a border-flash extra life for every 10,000-point threshold crossed.
void game_score_add(int score) {
    int previous = game_score / 10000;
    game_score += score;
    if (game_score > 99999) game_score = 99999;
    game_draw_score();
    if (game_score / 10000 == previous) {
        return;
    }
    game_lives++;
    game_extra_life_count = 16 << 2;
    game_extra_life = do_extra_life;
}

// Collects an item tile and opens the portal once all items are taken, with level-specific
// delays for Eugene (wait for descent) and Kong levels (wait for the beast to fall first).
void game_got_item(int tile) {
    level_tile_delete(tile);
    game_score_add(100);
    if (level_reduce_item_count() > 0) {
        return;
    }
    if (game_level == EUGENE) {
        npcs_eugene();
    }
    if ((game_level == 7 || game_level == 11) && !kong_fallen) {
        return;
    }
    portal_ready();
}

int game_is_playing(void) {
    return drawer == do_game_drawer;
}

// Set from the start of a game until it returns to the title. While set, the playfield is
// drawn by the hi-res renderer, including when paused, dying or between levels.
static int game_on_screen = 0;

int game_is_on_screen(void) {
    return game_on_screen;
}

void game_leave_screen(void) {
    game_on_screen = 0;
}

// Draws the HUD, then runs the per-frame interactions: the Solar Power Generator beam
// flags, NPC collisions, the miner's hazards and pickups, item colours and the portal.
// The playfield itself is drawn afterwards by the hi-res renderer from the game state.
static void do_game_drawer() {
    game_draw_air();
    game_extra_life();
    if (game_frame == 0) {
        return;
    }
    level_clear_solar_powered_generator_flags();
    npcs_mark_solar_powered_generator_tiles();
    npcs_mark_collisions();
    miner_drawer();
    level_cycle_item_colours();
    solar_powered_generator_drawer();
    portal_drawer();
}

static void do_game_draw_once() {
    do_game_drawer();

    drawer = do_nothing;
}

// Drives all subsystems one simulation tick; in demo mode exits to the transition screen after 64 ticks.
static void do_game_ticker() {
    if (game_music == MUS_PLAY) {
        miner_inc_seq();
    }
    game_frame = timer_update(&game_timer);
    if (game_frame == 0) {
        return;
    }
    game_ticks++;
    level_ticker();
    npcs_ticker();
    miner_ticker();
    portal_ticker();
    game_reduce_air(1);
    if (game_air == 0) {
        action = lives_action;
    }
    if (game_demo == 0) {
        return;
    }
    if (game_ticks < 64) {
        return;
    }
    action = trans_action;
}

void game_pause(int paused) {
    if (game_paused == paused) {
        return;
    }
    game_paused = paused;
    if (paused) {
        ticker = do_nothing;
        drawer = do_nothing;
        audio_play(MUS_STOP);
    } else {
        ticker = do_game_ticker;
        drawer = do_game_drawer;
        audio_play(game_music);
    }
}

// Initialises all level subsystems, fills the air bar, sets the frame timer,
// and applies any pending save-state restore after level_init has run.
static void do_game_init() {
    int x;
    level_init();
    npcs_init();
    portal_init();
    portal_ticker = do_nothing;
    miner_init();
    if (game_level == SOLAR_POWERED_GENERATOR) {
        solar_powered_generator_drawer = do_solar_powered_generator_drawer;
    } else {
        solar_powered_generator_drawer = do_nothing;
    }
    for (x = 0; x < 224; x++) {
        video_air_bar(AIR + x, x < 48 ? 0x2 : 0x4);
    }
    game_air_old = 224;
    game_air = air_data[game_level] * 8;
    game_draw_air   = do_nothing;
    game_extra_life = do_nothing;
    game_ticks = 0;
    system_border(level_border[game_level]);
    timer_set(&game_timer, 12, TICKRATE);
    game_frame = 1;
    if (game_paused == 0) {
        audio_play(game_music);
        ticker = do_game_ticker;
    } else {
        ticker = do_nothing;
    }
    if (savestate_has_pending_restore())
        savestate_apply_pending_restore();
}

static void do_game_demo_responder() {
    action = title_action;
}

static void do_game_responder() {
    if (game_input == KEY_PAUSE) {
        game_pause(1 - game_paused);
    } else if (game_input == KEY_MUTE) {
        game_music = game_music == MUS_PLAY ? MUS_STOP : MUS_PLAY;
        audio_play(game_music);
        game_pause(0);
    } else if (game_input == KEY_ESCAPE) {
        replay_stop_recording();
        action = title_action;
    } else if (game_input == KEY_U) {
        static int save_slot = 1;
        savestate_save(save_slot);
        game_start_save_flash(save_slot);
        save_slot = (save_slot % NUM_SLOTS) + 1;  
    } else if (game_input == KEY_R) {
        replay_toggle_r();
    }
}

void game_reset() {
    game_lives = game_config_lives;
    game_level = game_config_level;
    game_score = 0;
    game_paused = 0;
    game_extra_life = do_nothing;
    game_draw_hi_score();
    miner_set_seq(7, 20);
    if (game_demo == 0) {
        miner_ticker = do_miner_ticker;
        miner_drawer = do_miner_drawer;
    } else {
        miner_ticker = do_nothing;
        miner_drawer = do_nothing;
    }

    audio_music(MUS_GAME, MUS_STOP);
}

void game_change_level() {
    ticker = do_game_init;
    drawer = game_paused ? do_game_draw_once : do_game_drawer;

    action = do_nothing;
}

void game_action() {
    game_on_screen = 1;
    responder = game_demo ? do_game_demo_responder : do_game_responder;
    ticker = do_game_init;
    drawer = do_game_drawer;

    action = do_nothing;
}
