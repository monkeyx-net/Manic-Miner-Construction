// Headless regression harness. Links the game modules without main.c, stands in for the
// SDL front end, plays every level with seeded pseudo-random input and prints one line of
// game state per frame. Diff the output of two builds to prove a refactor did not change
// gameplay:
//
//   make sim && ./sim > before.txt      (then change the code)
//   make sim && ./sim > after.txt && diff before.txt after.txt
//
// Columns: run, frame, state, level, lives, air, score, miner x/y/frame/mirror, NPC x/y/frame
// for each active NPC, then hashes of the low-res screen: whole screen, and HUD rows only.

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/misc.h"
#include "../src/video.h"
#include "../src/game.h"
#include "../src/gamedata.h"

EVENT           action = do_nothing;
EVENT           responder = do_nothing;
EVENT           ticker = do_nothing;
EVENT           drawer = do_nothing;
int             game_input = KEY_NONE;
int             video_sync = 0;
const COLOUR   *sys_border = &video_colour[0];

static u8       screen[WIDTH * HEIGHT];
static int      keys;
static int      border;

void do_nothing(void) {}
void do_quit(void) {}

void system_set_pixel(int point, int index) { screen[point] = (u8)index; }
void system_blit_surface(void *src, int x, int y) { (void)src; (void)x; (void)y; }
void system_border(int index) { border = index; }
int  system_poll_key(int key) { return key >= 0 && key < 32 && (keys >> key) & 1; }
int  system_is_key(int key) { return system_poll_key(key); }

static unsigned rng;

static unsigned next_rand(void) {
    rng = rng * 1103515245u + 12345u;
    return rng >> 16;
}

static unsigned hash(const u8 *p, int n) {
    unsigned h = 2166136261u;
    while (n--) h = (h ^ *p++) * 16777619u;
    return h;
}

static const char *state_name(void) {
    if (game_is_gameover()) return "over";
    if (game_is_victory())  return "win";
    if (game_is_playing())  return "play";
    return "other";
}

static void print_frame(const char *run, int frame) {
    MINER_RENDER m;
    NPC_RENDER   n[8];
    int          score, hi, i;

    miner_get_render_data(&m);
    npcs_get_render_data(n);
    game_get_scores(&score, &hi);

    printf("%s %d %s L%d lives%d air%d score%d border%d M%d,%d,%d,%d", run, frame, state_name(),
           game_level, game_lives, game_air, score, border, m.x, m.y, m.sprite_idx, m.mirror);
    for (i = 0; i < 8; i++) {
        if (n[i].active) printf(" N%d,%d,%d", n[i].x, n[i].y, n[i].frame);
    }
    printf(" S%08x H%08x\n", hash(screen, sizeof screen), hash(screen + 128 * WIDTH, 64 * WIDTH));
}

static void step(void) {
    action();
    if (game_input != KEY_NONE) {
        responder();
        game_input = KEY_NONE;
    }
    ticker();
    drawer();
}

// Plays one level with random held inputs; lives are topped up so deaths keep being tested.
static void play_level(int level, int frames) {
    char run[16];
    int  f, hold = 0;

    snprintf(run, sizeof run, "L%02d", level);
    rng = 1234u + (unsigned)level * 7919u;

    game_demo = 0;
    game_reset();
    game_level = level;
    game_draw_hud();
    game_action();

    for (f = 0; f < frames; f++) {
        if (hold-- <= 0) {
            static const int combos[] = {
                0, 1 << KEY_LEFT, 1 << KEY_RIGHT, 1 << KEY_JUMP,
                (1 << KEY_LEFT) | (1 << KEY_JUMP), (1 << KEY_RIGHT) | (1 << KEY_JUMP),
                1 << KEY_RIGHT, 1 << KEY_LEFT
            };
            keys = combos[next_rand() % 8];
            hold = 2 + (int)(next_rand() % 40);
        }
        if (game_lives < 2) game_lives = 9;
        step();
        print_frame(run, f);
    }
}

// Runs inside a scratch directory that links to the game data, so the config and save
// files the game writes never touch the real ones.
static char scratch_dir[] = "/tmp/mmsimXXXXXX";

static void remove_scratch_dir(void) {
    char cmd[64];
    snprintf(cmd, sizeof cmd, "rm -rf %s", scratch_dir);
    if (system(cmd) != 0) fprintf(stderr, "sim: cannot remove %s\n", scratch_dir);
}

static void enter_scratch_dir(void) {
    static const char *links[] = {"gfx", "sfx", "levels.json"};
    char cwd[4096], *dir = scratch_dir, src[4200];
    int  i;

    if (!getcwd(cwd, sizeof cwd) || !mkdtemp(dir) || chdir(dir) != 0) {
        fprintf(stderr, "sim: cannot create scratch directory\n");
        exit(1);
    }
    for (i = 0; i < 3; i++) {
        snprintf(src, sizeof src, "%s/%s", cwd, links[i]);
        if (symlink(src, links[i]) != 0) {
            fprintf(stderr, "sim: cannot link %s\n", src);
            exit(1);
        }
    }
    atexit(remove_scratch_dir);
}

int main(int argc, char *argv[]) {
    int frames = argc > 1 ? atoi(argv[1]) : 3000;
    int level, f;

    enter_scratch_dir();

    TTF_Init();
    video_set_font(VIDEO_FONT_SMALL,       TTF_OpenFont("gfx/fonts/pixeldroidConsoleRegular.otf", 20));
    video_set_font(VIDEO_FONT_LARGE,       TTF_OpenFont("gfx/fonts/PressStart2P_400Regular.ttf", 9));
    video_set_font(VIDEO_FONT_TITLE,       TTF_OpenFont("gfx/fonts/MANICMINER-Regular.otf", 14));
    video_set_font(VIDEO_FONT_EXTRA_LARGE, TTF_OpenFont("gfx/fonts/MANICMINER-Regular.otf", 40));
    video_init();
    title_init();
    if (!game_data_load("levels.json")) {
        fprintf(stderr, "sim: levels.json: %s\n", game_data_error());
        return 1;
    }

    // Title screen and attract-mode demo with no input.
    action = loader_action;
    for (f = 0; f < frames; f++) {
        step();
        print_frame("title", f);
    }

    for (level = 0; level < num_levels; level++) {
        play_level(level, frames);
    }
    return 0;
}
