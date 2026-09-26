#ifndef SAVESTATE_H
#define SAVESTATE_H

#include <stdio.h>
#include "common.h"
#include "game.h"

#define NUM_SLOTS 5

typedef struct {
    int level;
    int lives;
    int score;
} SAVE_INFO;

extern int game_config_lives;
extern int game_config_level;
extern int game_config_show_fps;

void game_config_save(void);
void game_config_load(void);

int savestate_exists(int slot);
void savestate_delete(int slot);
int savestate_get_info(int slot, SAVE_INFO *info);

void savestate_save(int slot);
int  savestate_load(int slot);

// Shared by the save-state and replay file formats.
#define MINER_SAVE_FIELDS 10
#define NPC_SAVE_FIELDS   9

void save_write_list(FILE *f, const int *v, int n);
void save_write_key_list(FILE *f, const char *key, const int *v, int n);
int  save_parse_list(char *s, int *out, int max);
void miner_save_to_ints(const MINER_SAVE *m, int out[MINER_SAVE_FIELDS]);
void miner_save_from_ints(MINER_SAVE *m, const int in[MINER_SAVE_FIELDS]);
void npc_save_to_ints(const NPC_SAVE *n, int out[NPC_SAVE_FIELDS]);
void npc_save_from_ints(NPC_SAVE *n, const int in[NPC_SAVE_FIELDS]);

int  savestate_has_pending_restore(void);
void savestate_apply_pending_restore(void);
void savestate_set_pending(LEVEL_SAVE *ld, MINER_SAVE *md, NPC_SAVE rd[8], PORTAL_SAVE *pd, int air, int kong);

#endif
