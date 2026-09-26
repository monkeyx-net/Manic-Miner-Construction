#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL_image.h>

#include "misc.h"
#include "video.h"
#include "audio.h"
#include "game.h"
#include "savestate.h"
#include "replay.h"
#include "assets.h"

static u8      *title_pixels = NULL;
static u8      *title_colour = NULL;

// Converts the RGB title.png into ZX Spectrum–style tiles (1-bit pixel data + packed paper/ink byte
// per 8×8 tile) using majority-vote colour quantisation against the 16-colour palette.
void title_init(void) {
    SDL_Surface *raw = IMG_Load("gfx/sprites/title.png");
    if (!raw) { asset_problem("Cannot load gfx/sprites/title.png: %s", IMG_GetError()); return; }

    SDL_Surface *surf = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGB24, 0);
    SDL_FreeSurface(raw);
    if (!surf) { asset_problem("Cannot convert gfx/sprites/title.png"); return; }
    if (surf->w != WIDTH || surf->h != 72) {
        asset_problem("gfx/sprites/title.png is %dx%d, must be %dx72", surf->w, surf->h, WIDTH);
        SDL_FreeSurface(surf);
        return;
    }

    int pw = surf->w, ph = surf->h;   
    title_pixels = calloc(pw * ph / 8, 1);
    title_colour = calloc((ph / 8) * (pw / 8), 1);
    if (!title_pixels || !title_colour) { SDL_FreeSurface(surf); return; }

    SDL_LockSurface(surf);
    u8 *px = surf->pixels;
    int pitch = surf->pitch;

    for (int trow = 0; trow < ph / 8; trow++) {
        for (int tcol = 0; tcol < pw / 8; tcol++) {
            int count[16] = {0};
            for (int pr = 0; pr < 8; pr++) {
                for (int pc = 0; pc < 8; pc++) {
                    u8 *p = px + (trow * 8 + pr) * pitch + (tcol * 8 + pc) * 3;
                    for (int i = 0; i < 16; i++) {
                        if (p[0] == video_colour[i].r && p[1] == video_colour[i].g && p[2] == video_colour[i].b) {
                            count[i]++;
                            break;
                        }
                    }
                }
            }
            int paper = 0, ink = 0, pmax = -1, imax = -1;
            for (int i = 0; i < 16; i++) {
                if (count[i] > pmax) { pmax = count[i]; paper = i; }
            }
            for (int i = 0; i < 16; i++) {
                if (i != paper && count[i] > imax) { imax = count[i]; ink = i; }
            }
            if (imax < 0) ink = paper;
            title_colour[trow * (pw / 8) + tcol] = (u8)((paper << 4) | ink);

            for (int pr = 0; pr < 8; pr++) {
                u8 byte = 0;
                for (int pc = 0; pc < 8; pc++) {
                    u8 *p = px + (trow * 8 + pr) * pitch + (tcol * 8 + pc) * 3;
                    for (int i = 0; i < 16; i++) {
                        if (p[0] == video_colour[i].r && p[1] == video_colour[i].g && p[2] == video_colour[i].b) {
                            if (i == ink && i != paper) byte |= 1 << (7 - pc);
                            break;
                        }
                    }
                }
                title_pixels[(trow * 8 + pr) * (pw / 8) + tcol] = byte;
            }
        }
    }

    SDL_UnlockSurface(surf);
    SDL_FreeSurface(surf);
}

static const TICKER_SEG ticker_bugbyte[] = {
    {C_RED,"M"},{C_YELLOW,"A"},{C_GREEN,"N"},{C_LIGHT_BLUE,"I"},{C_MAGENTA,"C "},{C_LIGHT_BLUE,"M"},{C_MAGENTA,"I"},{C_RED,"N"},{C_YELLOW,"E"},{C_GREEN,"R   "},
    {C_WHITE,"(C) Bug-Byte Ltd. 1983   By Matthew Smith                                "},
    {C_WHITE,"Cursor Keys = Left & Right   Space = Jump   Pause/Tab = Pause   Alt = Tune On/Off                                "},
    {C_WHITE,"Guide Miner Willy through 20 lethal caverns ..."},
    {0,NULL}
};
static int      text_pos, text_end;

void title_screen_copy() {
    video_copy_bytes(title_pixels);
    video_copy_colour(title_colour, 0, 256 + 32);
}

static void do_start_game() {
    game_draw_hud();
    game_reset();

    game_action();
}

static void do_title_ticker() {
    text_pos -= 2;

    if (text_pos < text_end) {
        game_demo = 1;
        action = do_start_game;
    }

    miner_inc_seq();
}

static void do_title_drawer() {
    audio_drawer();
    video_pixel_fill(160 * WIDTH, 16 * WIDTH, 0x0);
    video_ticker(ticker_bugbyte, text_pos, 160);
}

// Draws the full title screen: backdrop, coloured title font, pianist keyboard animation,
// and scrolling ticker text; starts title music.
static void do_title_init() {
    game_config_save();  
    title_screen_copy();
    video_pixel_fill(72 * WIDTH, 72 * WIDTH, 0xa);
    video_pixel_fill(144 * WIDTH, 48 * WIDTH, 0x0);

    {
        for (int r = 0; r < 16; r++)
            video_pixel_fill((82 + r) * WIDTH, 160, C_MID_RED);
        int x = (160 - video_text_width_f("manic miner", VIDEO_FONT_TITLE)) / 2;
        x = video_write_f(82, x, C_WHITE, "m", VIDEO_FONT_TITLE);
        x = video_write_f(82, x, C_YELLOW, "a", VIDEO_FONT_TITLE);
        x = video_write_f(82, x, C_GREEN, "n", VIDEO_FONT_TITLE);
        x = video_write_f(82, x, C_LIGHT_BLUE, "i", VIDEO_FONT_TITLE);
        x = video_write_f(82, x, C_MAGENTA, "c", VIDEO_FONT_TITLE);
        x = video_write_f(82, x, C_MAGENTA, " ", VIDEO_FONT_TITLE);
        x = video_write_f(82, x, C_WHITE, "m", VIDEO_FONT_TITLE);
        x = video_write_f(82, x, C_MAGENTA, "i", VIDEO_FONT_TITLE);
        x = video_write_f(82, x, C_RED, "n", VIDEO_FONT_TITLE);
        x = video_write_f(82, x, C_YELLOW, "e", VIDEO_FONT_TITLE);
        x = video_write_f(82, x, C_GREEN, "r", VIDEO_FONT_TITLE);
    }

    video_write(80 * WIDTH + 19 * 8, C_BLACK, "Starring...", VIDEO_FONT_SMALL);
    video_write(88 * WIDTH + 19 * 8, C_YELLOW, "Miner Willy", VIDEO_FONT_SMALL);

    {
        int x = video_write_f(118, 6 * 8, C_BLACK, "PRESS ", VIDEO_FONT_LARGE);
        x = video_write_f(118, x, C_YELLOW, "ENTER", VIDEO_FONT_LARGE);
        video_write_f(118, x, C_BLACK, " TO START", VIDEO_FONT_LARGE);
    }

    {
        static const u8 kbd[] = {1,2,3, 1,2,2,3, 1,2,3, 1,2,2,3, 1,2,3, 1,2,2,3, 1,2,3, 1,2,2,3, 1,2,3};
        video_pixel_fill(KEYBOARD, 16 * WIDTH, C_BLACK);
        for (int k = 0; k < (int)(sizeof kbd); k++)
            video_draw_piano_key(KEYBOARD + 4 + k * 8, kbd[k], C_WHITE);
    }

    text_pos = WIDTH;
    text_end = -video_ticker_width(ticker_bugbyte);

    miner_set_seq(7, 8);

    audio_music(MUS_TITLE, MUS_PLAY);

    ticker = do_nothing;
}

extern int game_config_lives;
extern int game_config_level;

static int save_load_sel = 1;

static void do_hi_scores_responder(void) { action = title_action; }

static void draw_save_load_slots() {
    video_pixel_fill(32 * WIDTH, 80 * WIDTH, 0);

    for (int i = 1; i <= NUM_SLOTS; i++) {
        int row = (32 + (i - 1) * 16) * WIDTH;
        SAVE_INFO info;
        char text[128];
        int exists = savestate_get_info(i, &info);
        int sel = (save_load_sel == i);

        if (exists) {
            snprintf(text, sizeof(text), "%sSLOT %d  LEVEL:%02d L:%d",
                sel ? ">" : " ", i, info.level, info.lives);
            video_write_f(row / WIDTH, 8, sel ? C_YELLOW : C_WHITE, text, VIDEO_FONT_LARGE);
        } else {
            snprintf(text, sizeof(text), "%sSLOT %d  ---  EMPTY  ---",
                sel ? ">" : " ", i);
            video_write_f(row / WIDTH, 8, sel ? C_LIGHT_BLUE : C_MAGENTA, text, VIDEO_FONT_LARGE);
        }
    }
}

static void do_save_load_init() {
    save_load_sel = 1;
    video_pixel_fill(0, WIDTH * HEIGHT, 0);
    video_write_f(8, 92, C_YELLOW, "LOAD GAME", VIDEO_FONT_LARGE);
    draw_save_load_slots();
    video_write(112 * WIDTH + 4, C_MAGENTA, "UP/DOWN = SELECT SLOT", VIDEO_FONT_SMALL);
    {
        int x = video_write(120 * WIDTH + 4, C_WHITE, "ENTER=LOAD  ", VIDEO_FONT_SMALL);
        x = video_write(120 * WIDTH + x, C_RED, "DEL=ERASE  ", VIDEO_FONT_SMALL);
        video_write(120 * WIDTH + x, C_WHITE, "ESC=BACK", VIDEO_FONT_SMALL);
    }
    ticker = do_nothing;
    drawer = do_nothing;  
}

static void do_save_load_responder() {
    if (game_input == KEY_UP) {
        save_load_sel = (save_load_sel - 2 + NUM_SLOTS) % NUM_SLOTS + 1;
        draw_save_load_slots();
    } else if (game_input == KEY_DOWN) {
        save_load_sel = save_load_sel % NUM_SLOTS + 1;
        draw_save_load_slots();
    } else if (game_input == KEY_ENTER) {
        game_demo = 0;
        game_draw_hud();
        game_reset();
        if (savestate_load(save_load_sel))   {
            npcs_version(0);
            game_action();
        }
    } else if (game_input == KEY_DELETE) {
        if (savestate_exists(save_load_sel)) {
            savestate_delete(save_load_sel);
            draw_save_load_slots();
        }
    } else if (game_input == KEY_ESCAPE) {
        action = title_action;
    }
}

static void save_load_menu_action() {
    responder = do_save_load_responder;
    ticker = do_save_load_init;
    drawer = audio_drawer;
    action = do_nothing;
}

static int option_sel = 0;
static int option_lives = 3;
static int option_level = 0;
typedef enum { OPT_LIVES, OPT_LEVEL, OPT_FPS, OPT_PLAY_REPLAY, OPT_DELETE_REPLAY } OPTION;

static OPTION option_items[5];
static int option_item_count = 0;

static void draw_options_items() {
    video_pixel_fill(48 * WIDTH, 96 * WIDTH, 0);

    char buf[128];

    snprintf(buf, sizeof(buf), "LIVES:  %d", option_lives);
    video_write_f(48, 16, option_sel == 0 ? C_YELLOW : C_WHITE, buf, VIDEO_FONT_LARGE);

    snprintf(buf, sizeof(buf), "LEVEL: %02d", option_level);
    video_write_f(64, 16, option_sel == 1 ? C_YELLOW : C_WHITE, buf, VIDEO_FONT_LARGE);

    snprintf(buf, sizeof(buf), "%s", level_get_name(option_level));
    video_write_f(80, 24, C_LIGHT_BLUE, buf, VIDEO_FONT_LARGE);

    for (int i = 2; i < option_item_count; i++) {
        int row = 96 + (i - 2) * 16;
        int s = (option_sel == i);
        switch (option_items[i]) {
          case OPT_FPS:
            video_write_f(row, 16, s ? C_YELLOW : C_WHITE,
                          game_config_show_fps ? "FPS: ON " : "FPS: OFF", VIDEO_FONT_LARGE);
            break;
          case OPT_PLAY_REPLAY:
            video_write_f(row, 16, s ? C_YELLOW : C_WHITE, "PLAY REPLAY", VIDEO_FONT_LARGE);
            break;
          case OPT_DELETE_REPLAY:
            video_write_f(row, 16, s ? C_RED : C_MAGENTA, "DELETE REPLAY", VIDEO_FONT_LARGE);
            break;
          default:
            break;
        }
    }
}

static void do_options_init() {
    option_lives = game_config_lives;
    option_level = game_config_level;
    option_sel = 0;

    option_items[0] = OPT_LIVES;
    option_items[1] = OPT_LEVEL;
    option_item_count = 2;
    option_items[option_item_count++] = OPT_FPS;
    if (replay_has_buffer()) option_items[option_item_count++] = OPT_PLAY_REPLAY;
    if (replay_exists())    option_items[option_item_count++] = OPT_DELETE_REPLAY;

    video_pixel_fill(0, WIDTH * HEIGHT, 0);
    video_write_f(16, (WIDTH - 7 * 8) / 2, C_YELLOW, "OPTIONS", VIDEO_FONT_LARGE);

    draw_options_items();

    video_write(152 * WIDTH + 4, C_MAGENTA, "LEFT/RIGHT = CHANGE VALUE", VIDEO_FONT_SMALL);
    video_write(161 * WIDTH + 4, C_MAGENTA, "UP/DOWN = SWITCH SETTING", VIDEO_FONT_SMALL);
    video_write(170 * WIDTH + 4, C_WHITE, "ENTER = OK   ESC = CANCEL", VIDEO_FONT_SMALL);

    ticker = do_nothing;
    drawer = do_nothing;  
}

static void do_options_responder() {
    int max_sel = option_item_count - 1;

    if (game_input == KEY_UP || game_input == KEY_DOWN) {
        if (game_input == KEY_UP) option_sel = (option_sel - 1 + max_sel + 1) % (max_sel + 1);
        else option_sel = (option_sel + 1) % (max_sel + 1);
        draw_options_items();
    } else if (game_input == KEY_LEFT || game_input == KEY_RIGHT) {
        if (option_sel == 0) {
            option_lives = (game_input == KEY_LEFT) ? (option_lives > 1 ? option_lives - 1 : 1) : (option_lives < 9 ? option_lives + 1 : 9);
            draw_options_items();
        } else if (option_sel == 1) {
            option_level = (game_input == KEY_LEFT) ? (option_level > 0 ? option_level - 1 : 0) : (option_level < num_levels - 1 ? option_level + 1 : num_levels - 1);
            draw_options_items();
        } else if (option_items[option_sel] == OPT_FPS) {
            game_config_show_fps ^= 1;
            game_config_save();
            draw_options_items();
        }
    } else if (game_input == KEY_ENTER) {
        OPTION item = option_items[option_sel];
        if (item == OPT_FPS) {
            game_config_show_fps ^= 1;
            game_config_save();
            draw_options_items();
        } else if (item == OPT_PLAY_REPLAY) {
            replay_start_playback();
        } else if (item == OPT_DELETE_REPLAY) {
            replay_delete();
            action = title_action;
        } else {
            game_config_lives = option_lives;
            game_config_level = option_level;
            game_config_save();
            action = title_action;
        }
    } else if (game_input == KEY_ESCAPE) {
        action = title_action;
    }
}

static void DoOptionsAction() {
    responder = do_options_responder;
    ticker = do_options_init;
    drawer = do_nothing;  
    action = do_nothing;
}

static void do_title_responder() {
    if (game_input == KEY_ENTER) {
        npcs_version(0);

        game_demo = 0;
        action = do_start_game;
    } else if (game_input == KEY_L) {
        save_load_menu_action();
    } else if (game_input == KEY_S) {
        video_pixel_fill(0, WIDTH * HEIGHT, 0);
        video_write_f(0, 6 * 8, C_YELLOW, "HIGH SCORES", VIDEO_FONT_LARGE);
        for (int i = 0; i < num_levels; i++) {
            int row   = (10 + i * 8) * WIDTH;
            int score = game_get_level_hi_score(i);
            u8  ink   = (i % 2 == 0) ? C_WHITE : C_LIGHT_BLUE;
            char buf[68];
            snprintf(buf, sizeof(buf), "%2d %s", i + 1, level_get_name(i));
            video_write(row, ink, buf, VIDEO_FONT_SMALL);
            char sbuf[16];
            snprintf(sbuf, sizeof(sbuf), "%6d", score);
            video_write(row + 200, ink, sbuf, VIDEO_FONT_SMALL);
        }
        video_write(172 * WIDTH, C_RED, "PRESS ANY KEY TO RETURN", VIDEO_FONT_SMALL);
        ticker = do_nothing;
        drawer = do_nothing;  
        responder = do_hi_scores_responder;
    } else if (game_input == KEY_O) {
        DoOptionsAction();
    } else if (game_input == KEY_ESCAPE) {
        do_quit();
    }
}

static void do_title_action() {
    if (audio_music_playing == MUS_STOP) {
        return;
    }

    ticker = do_title_ticker;
    drawer = do_title_drawer;

    action = do_nothing;
}

int title_miner_active(void) {
    return drawer == do_title_drawer;
}

void title_action() {
    game_leave_screen();
    responder = do_title_responder;
    ticker = do_title_init;
    drawer = audio_drawer;

    action = do_title_action;
}
