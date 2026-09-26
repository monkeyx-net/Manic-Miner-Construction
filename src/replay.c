#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "misc.h"
#include "game.h"
#include "video.h"
#include "savestate.h"
#include "replay.h"

int replay_mode = REPLAY_NONE;

static int *buffer = NULL;
static int buffer_len = 0;
static int buffer_cap = 0;
static int play_tick = 0;
static int tick_bits = 0;

#define BIT_LEFT  1
#define BIT_RIGHT 2
#define BIT_JUMP  4

static LEVEL_SAVE start_level;
static MINER_SAVE  start_miner;
static PORTAL_SAVE start_portal;
static NPC_SAVE  start_npcs[8];
static int        have_start_state = 0;
static int        saved_score = -1, saved_hiscore = -1;
static int        saved_level = -1, saved_lives = -1;
static int        saved_air = 0, saved_kong = 0;

static void ensure_capacity(int n) {
    if (buffer_cap >= n) return;
    int newcap = buffer_cap ? buffer_cap * 2 : 256;
    while (newcap < n) newcap *= 2;
    buffer = realloc(buffer, sizeof(int) * newcap);
    buffer_cap = newcap;
}

static int sample_bits() {
    return (system_poll_key(KEY_LEFT)  ? BIT_LEFT  : 0)
         | (system_poll_key(KEY_RIGHT) ? BIT_RIGHT : 0)
         | (system_poll_key(KEY_JUMP)  ? BIT_JUMP  : 0);
}

void replay_tick(void) {
    if (replay_mode == REPLAY_RECORDING) {
        ensure_capacity(buffer_len + 1);
        buffer[buffer_len++] = sample_bits();
    } else if (replay_mode == REPLAY_PLAYING) {
        if (play_tick < buffer_len) {
            tick_bits = buffer[play_tick++];
        } else {
            replay_mode = REPLAY_NONE;
        }
    }
}

int replay_is_key(int key) {
    if (key == KEY_LEFT)  return (tick_bits & BIT_LEFT)  ? 1 : 0;
    if (key == KEY_RIGHT) return (tick_bits & BIT_RIGHT) ? 1 : 0;
    if (key == KEY_JUMP)  return (tick_bits & BIT_JUMP)  ? 1 : 0;
    return 0;
}

void replay_stop_recording(void) {
    if (replay_mode == REPLAY_RECORDING) {
        replay_mode = REPLAY_NONE;
        replay_save();
    }
}

void replay_toggle_r(void) {
    if (replay_mode == REPLAY_RECORDING) {
        replay_mode = REPLAY_NONE;
        replay_save();
    } else if (replay_mode == REPLAY_PLAYING) {
        replay_mode = REPLAY_NONE;
        free(buffer); buffer = NULL; buffer_len = buffer_cap = 0;
    } else {
        replay_start_recording();
    }
}

// Snapshots the full game state (tiles, miner, NPCs, portal, scores) before a run begins
// so playback can restore exact starting conditions.
void replay_start_recording(void) {
    buffer_len = 0; play_tick = 0; tick_bits = 0; replay_mode = REPLAY_RECORDING;

    game_get_scores(&saved_score, &saved_hiscore);
    saved_level = game_level;
    saved_lives = game_lives;
    saved_air   = game_air;
    saved_kong  = kong_fallen;

    level_get_save_data(&start_level);
    miner_get_save_data(&start_miner);
    portal_get_save_data(&start_portal);
    npcs_get_save_data(start_npcs);
    have_start_state = 1;
}

// Restores the recording snapshot as a pending save-state, then enters REPLAY_PLAYING
// mode to drive miner movement from the stored input buffer.
void replay_start_playback(void) {
    game_demo = 0;
    game_reset();
    if (saved_level >= 0) game_level = saved_level;
    if (saved_lives >= 0) game_lives = saved_lives;
    if (saved_score >= 0 || saved_hiscore >= 0) {
        int cur_score = 0, cur_hiscore = 0;
        game_get_scores(&cur_score, &cur_hiscore);
        game_set_scores(saved_score   >= 0 ? saved_score   : cur_score,
                       saved_hiscore >= 0 ? saved_hiscore : cur_hiscore);
    }
    video_pixel_fill(0, WIDTH * HEIGHT, 0);
    game_draw_hud();
    if (have_start_state) {
        savestate_set_pending(&start_level, &start_miner, start_npcs, &start_portal,
                             saved_air, saved_kong);
    }
    replay_mode = REPLAY_PLAYING;
    play_tick = 0; tick_bits = 0;
    game_action();
}

void replay_draw_osd(void) {
    static int prev_mode  = REPLAY_NONE;
    static int osd_counter = 0;

    if (replay_mode == REPLAY_RECORDING) {
        u8 ink = ((osd_counter / 25) % 2 == 0) ? C_RED : C_BLACK;
        video_write_f(SCORE / WIDTH, 112, ink, "REC", VIDEO_FONT_LARGE);
        osd_counter++;
    } else {
        if (prev_mode != REPLAY_NONE)
            video_write_f(SCORE / WIDTH, 112, C_BLACK, "REC", VIDEO_FONT_LARGE);
        osd_counter = 0;
    }

    prev_mode = replay_mode;
}

// Serialises the input buffer and optional starting snapshot to replay.dat
// in a key=value / CSV text format.
void replay_save(void) {
    if (buffer_len == 0) return;
    FILE *f = fopen("replay.dat", "w");
    if (!f) return;
    int cur_hiscore; game_get_scores(NULL, &cur_hiscore);
    fprintf(f, "version=2\n");
    fprintf(f, "level=%d\n",   saved_level  >= 0 ? saved_level  : game_level);
    fprintf(f, "lives=%d\n",   saved_lives  >= 0 ? saved_lives  : game_lives);
    fprintf(f, "score=%d\n",   saved_score  >= 0 ? saved_score  : 0);
    fprintf(f, "hiscore=%d\n", cur_hiscore);
    fprintf(f, "air=%d\n",     saved_air);
    fprintf(f, "kong=%d\n",    saved_kong);
    fprintf(f, "count=%d\n", buffer_len);

    if (have_start_state) {
        int mv[MINER_SAVE_FIELDS], nv[NPC_SAVE_FIELDS];

        save_write_key_list(f, "start_level_tiles",    start_level.tile_type,     512);
        save_write_key_list(f, "start_level_gfx",      start_level.tile_gfx,      512);
        save_write_key_list(f, "start_level_collapse", start_level.collapse_data, 512);
        fprintf(f, "start_level_itemcount=%d\n", start_level.item_count);

        miner_save_to_ints(&start_miner, mv);
        save_write_key_list(f, "start_miner", mv, MINER_SAVE_FIELDS);

        fprintf(f, "start_portal_ready=%d\n", start_portal.ready);

        fprintf(f, "start_npcs=");
        for (int r = 0; r < 8; r++) {
            if (r) fputc(';', f);
            npc_save_to_ints(&start_npcs[r], nv);
            save_write_list(f, nv, NPC_SAVE_FIELDS);
        }
        fprintf(f, "\n");
    }

    save_write_list(f, buffer, buffer_len);
    fprintf(f, "\n");
    fclose(f);
}

int replay_has_buffer(void) { return buffer_len > 0; }

int replay_exists(void) {
    FILE *f = fopen("replay.dat", "r");
    if (!f) return 0; fclose(f); return 1;
}

void replay_delete(void) {
    remove("replay.dat");
    free(buffer); buffer = NULL; buffer_len = buffer_cap = 0; replay_mode = REPLAY_NONE; have_start_state = 0; saved_score = -1; saved_hiscore = -1; saved_level = -1; saved_lives = -1;
}

// Returns the text after key if line starts with it, else NULL.
static char *value_of(char *line, const char *key) {
    size_t n = strlen(key);
    return strncmp(line, key, n) == 0 ? line + n : NULL;
}

// Reads replay.dat, parsing the key=value header and NPC/miner/level snapshot fields,
// then populates the input buffer ready for playback.
int replay_load(void) {
    FILE *f = fopen("replay.dat", "r");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, sz, f); buf[sz] = '\0';
    fclose(f);

    free(buffer); buffer = NULL; buffer_len = buffer_cap = 0; have_start_state = 0;

    int file_score = -1, file_hiscore = -1;

    char *outer_ctx;
    char *line = strtok_r(buf, "\n", &outer_ctx);
    while (line) {
        char *v;
        if ((v = value_of(line, "version=")) || (v = value_of(line, "count="))) {
        } else if ((v = value_of(line, "level="))) {
            saved_level = atoi(v);
        } else if ((v = value_of(line, "lives="))) {
            saved_lives = atoi(v);
        } else if ((v = value_of(line, "score="))) {
            file_score = atoi(v);
        } else if ((v = value_of(line, "hiscore="))) {
            file_hiscore = atoi(v);
        } else if ((v = value_of(line, "air="))) {
            saved_air = atoi(v);
        } else if ((v = value_of(line, "kong="))) {
            saved_kong = atoi(v);
        } else if ((v = value_of(line, "start_level_tiles="))) {
            save_parse_list(v, start_level.tile_type, 512);
            have_start_state = 1;
        } else if ((v = value_of(line, "start_level_gfx="))) {
            save_parse_list(v, start_level.tile_gfx, 512);
            have_start_state = 1;
        } else if ((v = value_of(line, "start_level_collapse="))) {
            save_parse_list(v, start_level.collapse_data, 512);
            have_start_state = 1;
        } else if ((v = value_of(line, "start_level_itemcount="))) {
            start_level.item_count = atoi(v);
            have_start_state = 1;
        } else if ((v = value_of(line, "start_miner="))) {
            int vals[MINER_SAVE_FIELDS];
            if (save_parse_list(v, vals, MINER_SAVE_FIELDS) == MINER_SAVE_FIELDS) {
                miner_save_from_ints(&start_miner, vals);
                have_start_state = 1;
            }
        } else if ((v = value_of(line, "start_portal_ready="))) {
            start_portal.ready = atoi(v);
            have_start_state = 1;
        } else if ((v = value_of(line, "start_npcs="))) {
            char *rctx;
            char *r = strtok_r(v, ";", &rctx);
            for (int idx = 0; r && idx < 8; idx++, r = strtok_r(NULL, ";", &rctx)) {
                int vals[NPC_SAVE_FIELDS];
                if (save_parse_list(r, vals, NPC_SAVE_FIELDS) == NPC_SAVE_FIELDS)
                    npc_save_from_ints(&start_npcs[idx], vals);
            }
            have_start_state = 1;
        } else if (strchr(line, ',')) {
            // The input buffer: one comma-separated line of per-tick key bits.
            char *ctx;
            for (char *tok = strtok_r(line, ",", &ctx); tok; tok = strtok_r(NULL, ",", &ctx)) {
                ensure_capacity(buffer_len + 1);
                buffer[buffer_len++] = atoi(tok);
            }
        }

        line = strtok_r(NULL, "\n", &outer_ctx);
    }

    saved_score = file_score;
    saved_hiscore = file_hiscore;

    free(buf);
    return buffer_len > 0;
}
