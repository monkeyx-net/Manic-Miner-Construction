#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "savestate.h"
#include "game.h"

int game_config_lives   = 3;
int game_config_level   = 0;
int game_config_show_fps = 0;

static MINER_SAVE  pending_md;
static LEVEL_SAVE  pending_ld;
static PORTAL_SAVE pending_pd;
static NPC_SAVE  pending_rd[8];
static int         pending_air        = 0;
static int         pending_kong_fallen = 0;
static int         has_pending_restore = 0;

int savestate_has_pending_restore(void) { return has_pending_restore; }

void savestate_set_pending(LEVEL_SAVE *ld, MINER_SAVE *md, NPC_SAVE rd[8], PORTAL_SAVE *pd, int air, int kong) {
    pending_ld        = *ld;
    pending_md        = *md;
    memcpy(pending_rd, rd, sizeof(pending_rd));
    pending_pd        = *pd;
    pending_air       = air;
    pending_kong_fallen = kong;
    has_pending_restore = 1;
}

void savestate_apply_pending_restore(void) {
    if (!has_pending_restore) return;
    has_pending_restore = 0;

    level_set_save_data(&pending_ld);
    miner_set_save_data(&pending_md);
    npcs_set_save_data(pending_rd);
    portal_set_save_data(&pending_pd);

    game_air        = pending_air;
    game_air_old     = 224;
    game_draw_air   = do_nothing;
    kong_fallen     = pending_kong_fallen;

    if (pending_pd.ready) portal_ready();

    game_draw_score();
    game_draw_hi_score();
}

static const char *CONFIG_FILE = "gameconfig.dat";

void game_config_save(void) {
    FILE *f = fopen(CONFIG_FILE, "w");
    if (!f) return;
    fprintf(f, "lives=%d\nlevel=%d\nshowfps=%d\n", game_config_lives, game_config_level, game_config_show_fps);
    fprintf(f, "levelhs=");
    for (int i = 0; i < 20; i++) {
        if (i) fputc(',', f);
        fprintf(f, "%d", game_get_level_hi_score(i));
    }
    fputc('\n', f);
    fclose(f);
}

// Reads the persistent config file and restores lives count, starting level, and per-level high scores.
void game_config_load(void) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *k = strtok(line, "=\n");
        char *v = strtok(NULL, "\n");
        if (!k || !v) continue;
        if (strcmp(k, "lives") == 0) game_config_lives = atoi(v);
        else if (strcmp(k, "level") == 0) game_config_level = atoi(v);
        else if (strcmp(k, "showfps") == 0) game_config_show_fps = atoi(v);
        else if (strcmp(k, "levelhs") == 0) {
            char *tmp = strdup(v);
            char *tok = strtok(tmp, ",");
            int i = 0;
            while (tok && i < 20) {
                game_set_level_hi_score(i++, atoi(tok));
                tok = strtok(NULL, ",");
            }
            free(tmp);
        }
    }
    fclose(f);
}

// Writes "a,b,c" with no key or newline.
void save_write_list(FILE *f, const int *v, int n) {
    for (int i = 0; i < n; i++) {
        if (i) fputc(',', f);
        fprintf(f, "%d", v[i]);
    }
}

// Writes a "key=a,b,c" line.
void save_write_key_list(FILE *f, const char *key, const int *v, int n) {
    fprintf(f, "%s=", key);
    save_write_list(f, v, n);
    fputc('\n', f);
}

// Parses up to max comma-separated integers from s (modifying it); returns how many were read.
int save_parse_list(char *s, int *out, int max) {
    int n = 0;
    char *ctx;
    char *tok = strtok_r(s, ",\n", &ctx);
    while (tok && n < max) {
        out[n++] = atoi(tok);
        tok = strtok_r(NULL, ",\n", &ctx);
    }
    return n;
}

// Field order used on disk for MINER_SAVE and NPC_SAVE; save files and replays both rely on it.
void miner_save_to_ints(const MINER_SAVE *m, int out[MINER_SAVE_FIELDS]) {
    out[0] = m->x;     out[1] = m->y;     out[2] = m->tile;       out[3] = m->align;
    out[4] = m->frame; out[5] = m->dir;   out[6] = m->air;        out[7] = m->jump_stage;
    out[8] = m->move;  out[9] = m->ink;
}

void miner_save_from_ints(MINER_SAVE *m, const int in[MINER_SAVE_FIELDS]) {
    m->x     = in[0]; m->y   = in[1]; m->tile = in[2]; m->align      = in[3];
    m->frame = in[4]; m->dir = in[5]; m->air  = in[6]; m->jump_stage = in[7];
    m->move  = in[8]; m->ink = in[9];
}

void npc_save_to_ints(const NPC_SAVE *n, int out[NPC_SAVE_FIELDS]) {
    out[0] = n->x;      out[1] = n->y;       out[2] = n->frame;
    out[3] = n->tile;   out[4] = n->subpix;  out[5] = n->nframes;
    out[6] = n->ink;    out[7] = n->move;    out[8] = n->active;
}

void npc_save_from_ints(NPC_SAVE *n, const int in[NPC_SAVE_FIELDS]) {
    n->x    = in[0]; n->y      = in[1]; n->frame   = in[2];
    n->tile = in[3]; n->subpix = in[4]; n->nframes = in[5];
    n->ink  = in[6]; n->move   = in[7]; n->active  = in[8];
}

static const char *const miner_keys[MINER_SAVE_FIELDS] = {
    "miner_x", "miner_y", "miner_tile", "miner_align", "miner_frame",
    "miner_dir", "miner_air", "jump_stage", "miner_move", "miner_ink"
};

static const char *const npc_keys[NPC_SAVE_FIELDS] = {
    "npc_x", "npc_y", "npc_frame", "npc_tile", "npc_subpix",
    "npc_nframes", "npc_ink", "npc_move", "npc_active"
};

static int key_index(const char *k, const char *const *keys, int n) {
    for (int i = 0; i < n; i++)
        if (strcmp(k, keys[i]) == 0) return i;
    return -1;
}

static char *slot_file(int slot) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "savestate_%d.dat", slot);
    return buf;
}

int savestate_exists(int slot) {
    FILE *f = fopen(slot_file(slot), "r");
    if (!f) return 0; fclose(f); return 1;
}

void savestate_delete(int slot) {
    remove(slot_file(slot));
}

int savestate_get_info(int slot, SAVE_INFO *info) {
    FILE *f = fopen(slot_file(slot), "r");
    if (!f) return 0;
    char line[8192];
    int version = 0;
    int level=0, lives=0, score=0;
    while (fgets(line, sizeof(line), f)) {
        char *k = strtok(line, "=\n");
        char *v = strtok(NULL, "\n");
        if (!k || !v) continue;
        if (strcmp(k, "version") == 0) version = atoi(v);
        else if (strcmp(k, "level") == 0) level = atoi(v);
        else if (strcmp(k, "lives") == 0) lives = atoi(v);
        else if (strcmp(k, "score") == 0) score = atoi(v);
    }
    fclose(f);
    if (version != 1) return 0;
    if (info) {
        info->level = level;
        info->lives = lives;
        info->score = score;
    }
    return 1;
}

// Serialises complete game state (miner, level tiles, NPCs, portal, scores) to a numbered
// slot file in key=value / CSV format.
void savestate_save(int slot) {
    FILE *f = fopen(slot_file(slot), "w");
    if (!f) return;
    fprintf(f, "version=1\n");
    fprintf(f, "level=%d\n", game_level);
    fprintf(f, "lives=%d\n", game_lives);
    fprintf(f, "air=%d\n", game_air);
    fprintf(f, "kongfallen=%d\n", kong_fallen ? 1 : 0);
    int score, hiscore; game_get_scores(&score, &hiscore);
    fprintf(f, "score=%d\n", score);
    fprintf(f, "hiscore=%d\n", hiscore);

    MINER_SAVE md; miner_get_save_data(&md);
    int mv[MINER_SAVE_FIELDS]; miner_save_to_ints(&md, mv);
    for (int j = 0; j < MINER_SAVE_FIELDS; j++) fprintf(f, "%s=%d\n", miner_keys[j], mv[j]);

    LEVEL_SAVE ld; level_get_save_data(&ld);
    fprintf(f, "itemcount=%d\n", ld.item_count);
    save_write_key_list(f, "tiletypes",    ld.tile_type,     512);
    save_write_key_list(f, "tilegfx",      ld.tile_gfx,      512);
    save_write_key_list(f, "collapsedata", ld.collapse_data, 512);

    PORTAL_SAVE pd; portal_get_save_data(&pd);
    fprintf(f, "portalready=%d\n", pd.ready);

    NPC_SAVE rd[8]; npcs_get_save_data(rd);
    int nv[8][NPC_SAVE_FIELDS];
    for (int i = 0; i < 8; i++) npc_save_to_ints(&rd[i], nv[i]);
    for (int j = 0; j < NPC_SAVE_FIELDS; j++) {
        int col[8];
        for (int i = 0; i < 8; i++) col[i] = nv[i][j];
        save_write_key_list(f, npc_keys[j], col, 8);
    }

    fclose(f);
}

// Reads a slot file and queues all deserialised state as a pending restore so the game
// applies it after the next level_init.
int savestate_load(int slot) {
    FILE *f = fopen(slot_file(slot), "r");
    if (!f) return 0;
    char line[8192];
    int mv[MINER_SAVE_FIELDS] = {0};
    int nv[8][NPC_SAVE_FIELDS] = {{0}};
    MINER_SAVE md;
    LEVEL_SAVE ld; memset(&ld, 0, sizeof(ld));
    PORTAL_SAVE pd; memset(&pd, 0, sizeof(pd));
    NPC_SAVE rd[8];

    while (fgets(line, sizeof(line), f)) {
        char *k = strtok(line, "=\n");
        char *v = strtok(NULL, "\n");
        int j;
        if (!k || !v) continue;
        if (strcmp(k, "level") == 0) game_level = atoi(v);
        else if (strcmp(k, "lives") == 0) game_lives = atoi(v);
        else if (strcmp(k, "air") == 0) game_air = atoi(v);
        else if (strcmp(k, "kongfallen") == 0) kong_fallen = atoi(v) ? 1 : 0;
        else if (strcmp(k, "score") == 0) {
            int hiscore = 0;
            game_get_scores(NULL, &hiscore);
            game_set_scores(atoi(v), hiscore);
        }
        else if ((j = key_index(k, miner_keys, MINER_SAVE_FIELDS)) >= 0) mv[j] = atoi(v);
        else if (strcmp(k, "itemcount") == 0) ld.item_count = atoi(v);
        else if (strcmp(k, "portalready") == 0) pd.ready = atoi(v);
        else if (strcmp(k, "tiletypes") == 0)    save_parse_list(v, ld.tile_type, 512);
        else if (strcmp(k, "tilegfx") == 0)      save_parse_list(v, ld.tile_gfx, 512);
        else if (strcmp(k, "collapsedata") == 0) save_parse_list(v, ld.collapse_data, 512);
        else if ((j = key_index(k, npc_keys, NPC_SAVE_FIELDS)) >= 0) {
            int col[8], n = save_parse_list(v, col, 8);
            for (int i = 0; i < n; i++) nv[i][j] = col[i];
        }
    }
    fclose(f);

    miner_save_from_ints(&md, mv);
    for (int i = 0; i < 8; i++) npc_save_from_ints(&rd[i], nv[i]);

    pending_md          = md;
    pending_ld          = ld;
    pending_pd          = pd;
    memcpy(pending_rd, rd, sizeof(rd));
    pending_air         = game_air;   
    pending_kong_fallen  = kong_fallen;
    has_pending_restore  = 1;
    return 1;
}
