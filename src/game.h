#ifndef GAME_H
#define GAME_H

#define LIVES   150 * WIDTH + 4
#define SCORE   172 * WIDTH

// Which of a tile's 8 rows are drawn; the rest show the level background. For the two
// partial shapes, data is the row count: SHAPE_TOP_ROWS draws rows 0..data-1 (a collapsing
// floor, or the top of the Kong wall as it slides open), SHAPE_BOTTOM_ROWS draws rows
// data..7 (the bottom of the Kong wall).
enum { SHAPE_FULL, SHAPE_TOP_ROWS, SHAPE_BOTTOM_ROWS };

typedef struct {
    u8        paper, ink;
    int       type;
    int       data;
    int       shape;
    int       slot;     // index into the level's 10 gfx slots, or -1
} TILE_RENDER;

typedef struct {
    u8  x, y;
    u8  ink;
    int active;
    int gfx;       
    int frame;     
    int mirror;    
} NPC_RENDER;

typedef struct {
    int        x, y;
    const u16 *gfx;
    u8         colour[2];
    int        flash;
    int        ready;
    int        show_swordfish;
} PORTAL_RENDER;

typedef struct {
    u8  x, y;
    int sprite_idx;
    int mirror;
    u8  ink;
} MINER_RENDER;

typedef struct {
    int active;
    int boot_x, boot_y;   
    int boot_visible;    
} GAMEOVER_RENDER;

extern int  game_level;
extern int  num_levels;
extern int  game_demo;
extern int  game_lives;
extern int  game_air, game_air_old;
extern int  game_ticks;
extern int  kong_fallen;
void game_set_num_levels(int n);
void game_set_air(int level, int air);
void game_set_border(int level, int border);

extern EVENT    game_draw_air;
extern EVENT    game_extra_life;

int  game_is_playing(void);
int  game_is_on_screen(void);
void game_leave_screen(void);
int  lives_get_colours(u8 *paper, u8 *ink);
int  trans_get_colours(u8 *paper, u8 *ink);
int  game_is_victory(void);
int  game_is_gameover(void);
void game_score_add(int);
void game_reduce_air(int);
void game_reset(void);
void game_check_high_score(void);
void game_draw_score(void);
void game_draw_hud(void);
void game_draw_hi_score(void);
void game_get_scores(int *score, int *hiscore);
void game_set_scores(int score, int hiscore);
void game_got_item(int);
void game_change_level(void);
void game_pause(int);
void game_start_save_flash(int slot);
void game_draw_save_osd(void);

void gameover_get_render_data(GAMEOVER_RENDER *out);

#define EUGENE  4
#define SKYLAB  13
#define SOLAR_POWERED_GENERATOR 18
#define TWENTY  19

#define MAX_NPC_SPRITES 29

enum {
    T_ITEM,
    T_SWITCHOFF,
    T_SWITCHON,
    T_SPACE,
    T_SOLID,
    T_FLOOR,
    T_COLLAPSE,
    T_CONVEYL,
    T_CONVEYR,
    T_HARM,
    T_VOID
};

enum {
    C_NONE,
    C_LEFT,
    C_RIGHT
};

int  game_get_level_hi_score(int level);
void game_update_level_hi_score(int level, int score);
void game_set_level_hi_score(int level, int score);

const char *level_get_name(int level);
void level_set_entry(int idx, const char *name, int data[512], u8 colours[10], int types[10]);
void level_init(void);
void level_ticker(void);
int  level_conveyor_phase(void);
void level_mark_tile_dirty(int);
int  level_tile_is_animated(int type, int data);
int  level_is_hires_dirty(int);
void level_clear_hires_dirty(int);
void level_mark_all_hires_dirty(void);
u8 level_get_bg(void);
void level_get_render_tile(int pos, TILE_RENDER *out);
int level_get_tile_type(int);
void level_collapse_tile(int);
void level_tile_delete(int);
void level_switch(int);
void level_set_solar_powered_generator_tile(int, int);
void level_clear_solar_powered_generator_flags(void);
int level_get_solar_powered_generator_tile(int);
int level_reduce_item_count(void);
int level_all_items_collected(void);
void level_cycle_item_colours(void);

typedef struct {
    int tile_type[512];
    int tile_gfx[512];
    int collapse_data[512];
    int tile_paper[512];
    int tile_ink[512];
    int item_count;
} LEVEL_SAVE;

void level_get_save_data(LEVEL_SAVE *d);
void level_set_save_data(LEVEL_SAVE *d);

extern u8   miner_x, miner_y;
extern int  miner_tile, miner_align;

typedef struct {
    int x, y, tile;
    int align, frame, dir;
    int air, jump_stage, move, ink;
} MINER_SAVE;

void miner_get_save_data(MINER_SAVE *d);
void miner_set_save_data(MINER_SAVE *d);
void miner_get_render_data(MINER_RENDER *out);

void miner_set_sprite(u16 frames[4][16]);
void miner_set_start(int idx, int x, int y, int frame, int dir, u8 ink);
void miner_init(void);
void do_miner_ticker(void);
void do_miner_drawer(void);
void miner_set_seq(int, int);
void miner_inc_seq(void);
void miner_get_seq_render_data(MINER_RENDER *out);

extern EVENT    portal_ticker;
void portal_set_entry(int idx, int x, int y, u16 gfx[16], u8 c0, u8 c1);
void portal_init(void);
void portal_drawer(void);
void portal_ready(void);
void portal_sword_fish(void);

typedef struct {
    int ready;
} PORTAL_SAVE;

void portal_get_save_data(PORTAL_SAVE *d);
void portal_set_save_data(PORTAL_SAVE *d);
void portal_get_render_data(PORTAL_RENDER *out);

void npcs_init(void);
void npcs_mark_collisions(void);
void npcs_version(int);
void npcs_set_sprite(int idx, u16 frames[8][16]);
void npcs_clear_level(int level);
void npcs_set_start(int level, int slot, int x, int y, int min, int max,
                     int move_id, int speed, int gfx_idx, u8 ink,
                     int nframes, int frame, int solar_powered_generator);
void npcs_mark_solar_powered_generator_tiles(void);
int  npcs_sprite_count(void);
void npcs_set_sprite_count(int);
void npcs_ticker(void);
void npcs_eugene(void);
void npcs_barrel(void);
void npcs_kong(void);

enum {
    MOVE_NONE,
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_UP,
    MOVE_DOWN,
    MOVE_KONG,
    MOVE_SKYLAB,
    MOVE_FALL,
    MOVE_EUGENE
};

typedef struct {
    int x, y, frame, tile;
    int subpix, nframes, ink;
    int move;
    int active;
} NPC_SAVE;

void npcs_get_save_data(NPC_SAVE d[8]);
void npcs_set_save_data(NPC_SAVE d[8]);
void npcs_get_render_data(NPC_RENDER out[8]);

void do_solar_powered_generator_drawer(void);

void title_screen_copy(void);

#endif 
