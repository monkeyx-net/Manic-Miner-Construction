#include "misc.h"
#include "video.h"
#include "version.h"

static const struct { const char *ch; u8 colour; int dy; } manic[5] = {
    {"M", C_RED,          0},
    {"A", C_YELLOW,       8},
    {"N", C_GREEN,        0},
    {"I", C_LIGHT_BLUE,  16},
    {"C", C_MAGENTA,      8},
};

static const struct { const char *ch; u8 colour; int dy; } miner[5] = {
    {"M", C_LIGHT_BLUE,  16},
    {"I", C_MAGENTA,      0},
    {"N", C_RED,          8},
    {"E", C_YELLOW,       0},
    {"R", C_GREEN,       16},
};

#define ROW_MANIC  62
#define ROW_MINER  65
#define FONT_H     48

static int   loader_ticks = 0;
static int   loader_flash = 0;
static TIMER loader_timer;

static void draw_word(const void *word, int row) {
    const struct { const char *ch; u8 colour; int dy; } *w = word;
    int total = 0;
    for (int i = 0; i < 5; i++) total += video_text_width_f(w[i].ch, VIDEO_FONT_EXTRA_LARGE);
    int x = (WIDTH - total) / 2;
    for (int i = 0; i < 5; i++) {
        video_write_f(row + w[i].dy, x, w[i].colour, w[i].ch, VIDEO_FONT_EXTRA_LARGE);
        x += video_text_width_f(w[i].ch, VIDEO_FONT_EXTRA_LARGE);
    }
}

static void do_loader_drawer() {
    video_pixel_fill(ROW_MANIC * WIDTH, (FONT_H + 16 + ROW_MINER - ROW_MANIC) * WIDTH, C_BLACK);
    if (loader_flash == 0)
        draw_word(manic, ROW_MANIC);
    else
        draw_word(miner, ROW_MINER);
}

static void do_loader_ticker() {
    if (timer_update(&loader_timer))
        loader_flash ^= 1;

    if (loader_ticks++ == 256)
        action = title_action;
}

static void do_loader_init() {
    video_write(172 * WIDTH, C_WHITE, "monkeyx-net", VIDEO_FONT_SMALL);
    video_write(172 * WIDTH + WIDTH - video_text_width_f(BUILD, VIDEO_FONT_SMALL), C_BLUE, BUILD, VIDEO_FONT_SMALL);

    loader_ticks = 0;
    loader_flash = 0;
    timer_set(&loader_timer, 1, 45);
    ticker = do_loader_ticker;
}

static void do_loader_responder() {
    action = title_action;
}

void loader_action() {
    responder = do_loader_responder;
    ticker    = do_loader_init;
    drawer    = do_loader_drawer;
    action    = do_nothing;
}
