#ifndef COMMON_H
#define COMMON_H

#define WIDTH       256
#define HEIGHT      192
#define MAX_LEVELS  100

enum {
    KEY_LEFT,
    KEY_RIGHT,
    KEY_JUMP,
    KEY_ENTER,
    KEY_LSHIFT,
    KEY_RSHIFT,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_ESCAPE,
    KEY_PAUSE,
    KEY_MUTE,
    KEY_ELSE,
    KEY_NONE,

    KEY_S,
    KEY_U,
    KEY_O,
    KEY_UP,
    KEY_DOWN,
    KEY_R,
    KEY_L,
    KEY_DELETE
};

typedef unsigned char   u8;
typedef unsigned short  u16;
typedef unsigned int    u32;

typedef void (*EVENT)(void);

extern EVENT    action, responder, ticker, drawer;
extern int      game_input;

void do_nothing(void);
void do_quit(void);

void loader_action(void);
void title_action(void);
void title_init(void);
int  title_miner_active(void);
void game_action(void);
void gameover_action(void);
void trans_action(void);
void lives_action(void);
void victory_action(void);

#endif
