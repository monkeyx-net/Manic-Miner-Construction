#define MUS_TITLE   0
#define MUS_GAME    1
#define MUS_STOP    0
#define MUS_PLAY    1

enum {
    SFX_DIE,
    SFX_KONG,
    SFX_GAMEOVER,
    SFX_AIR,
    SFX_VICTORY,
    SFX_JUMP,
    SFX_FALL,
    SFX_NONE
};

extern int  audio_music_playing;

void audio_init(void);
void audio_quit(void);
void audio_music(int, int);
void audio_play(int);
void audio_sfx(int);
void audio_fall_halt(void);
void audio_drawer(void);
