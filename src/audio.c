#include <SDL.h>
#include <SDL_mixer.h>
#include <stdlib.h>
#include <string.h>
#include "misc.h"
#include "game.h"
#include "video.h"
#include "assets.h"
#include "audio.h"

#define SAMPLERATE  22050
#define CHANNELS    2

static Mix_Music    *musTitle;
static Mix_Music    *musGame;
static int           cur_music_idx = MUS_TITLE;
static Mix_Chunk    *sfx_chunks[SFX_NONE];

#define CH_MINER    0
#define CH_EFFECT   1
#define CH_KONG     2
#define CH_FALL     3

int audio_music_playing = MUS_STOP;

void audio_drawer(void) {
    static const struct { int pos; int shape; int ink_on; int ink_off; } keys[] = {
        {  4, 1, 0x5, 0x7 },
        { 44, 2, 0x5, 0x7 },
        { 88, 0, 0x2, 0x0 },
        {128, 0, 0x2, 0x0 },
        {164, 3, 0x5, 0x7 },
        {204, 2, 0x5, 0x7 },
    };
    static const int N = sizeof(keys) / sizeof(keys[0]);
    static Uint32 last = 0;
    static int    cur  = -1;
    if (!audio_music_playing) return;
    Uint32 now = SDL_GetTicks();
    if (now - last < 150) return;
    last = now;
    if (cur >= 0)
        video_draw_piano_key(KEYBOARD + keys[cur].pos, keys[cur].shape, keys[cur].ink_off);
    cur = (cur + 1) % N;
    video_draw_piano_key(KEYBOARD + keys[cur].pos, keys[cur].shape, keys[cur].ink_on);
}

// The sound files must exist. They can only be loaded if the machine has an audio device;
// without one the game runs silently.
static int audio_open;

static int check_file(const char *path) {
    if (asset_file_exists(path)) return 1;
    asset_problem("Cannot find %s", path);
    return 0;
}

static Mix_Chunk *load_wav(const char *path) {
    Mix_Chunk *c;
    if (!check_file(path) || !audio_open) return NULL;
    c = Mix_LoadWAV(path);
    if (!c) asset_problem("Cannot load %s: %s", path, Mix_GetError());
    return c;
}

static Mix_Music *load_music(const char *path) {
    Mix_Music *m;
    if (!check_file(path) || !audio_open) return NULL;
    m = Mix_LoadMUS(path);
    if (!m) asset_problem("Cannot load %s: %s", path, Mix_GetError());
    return m;
}

void audio_init(void) {
#ifdef __EMSCRIPTEN__
    audio_open = Mix_OpenAudio(SAMPLERATE, AUDIO_S16SYS, CHANNELS, 4096) == 0;
#else
    audio_open = Mix_OpenAudio(SAMPLERATE, AUDIO_S16SYS, CHANNELS, 512) == 0;
#endif
    if (!audio_open) SDL_Log("No audio device (%s); sound is off", Mix_GetError());
    Mix_AllocateChannels(8);
    Mix_ReserveChannels(4);
    musTitle = load_music("sfx/music/title.ogg");
    musGame  = load_music("sfx/music/game.ogg");
    sfx_chunks[SFX_DIE]      = load_wav("sfx/sounds/die.ogg");
    sfx_chunks[SFX_KONG]     = load_wav("sfx/sounds/kong.ogg");
    sfx_chunks[SFX_GAMEOVER] = load_wav("sfx/sounds/gameover.ogg");
    sfx_chunks[SFX_AIR]      = load_wav("sfx/sounds/air.ogg");
    sfx_chunks[SFX_VICTORY]  = load_wav("sfx/sounds/victory.ogg");
    sfx_chunks[SFX_JUMP]     = load_wav("sfx/sounds/jump.ogg");
    sfx_chunks[SFX_FALL]     = load_wav("sfx/sounds/fall.ogg");
}

void audio_quit(void) {
    int i;
    Mix_HaltMusic();
    Mix_HaltChannel(-1);

    if (musTitle)  { Mix_FreeMusic(musTitle);  musTitle = NULL; }
    if (musGame)   { Mix_FreeMusic(musGame);   musGame  = NULL; }
    for (i = 0; i < SFX_NONE; i++)
        if (sfx_chunks[i]) { Mix_FreeChunk(sfx_chunks[i]); sfx_chunks[i] = NULL; }

    Mix_CloseAudio();
}

static void start_music(int idx) {
    Mix_Music *m    = (idx == MUS_GAME) ? musGame : musTitle;
    int        loops = (idx == MUS_GAME) ? -1 : 0;
    if (m)
        Mix_PlayMusic(m, loops);
}

void audio_music(int music, int playing) {
    cur_music_idx       = music;
    audio_music_playing = playing;
    Mix_HaltMusic();
    if (playing)
        start_music(music);
}

void audio_play(int playing) {
    audio_music_playing = playing;
    if (!playing) {
        Mix_PauseMusic();
    } else {
        if (Mix_PausedMusic())
            Mix_ResumeMusic();
        else if (!Mix_PlayingMusic())
            start_music(cur_music_idx);
    }
}

void audio_fall_halt(void) {
    Mix_HaltChannel(CH_FALL);
}

void audio_sfx(int sfx) {
    Mix_Chunk *c = (sfx < SFX_NONE) ? sfx_chunks[sfx] : NULL;
    if (!c) return;
    if (sfx == SFX_KONG) {
        Mix_PlayChannel(CH_KONG, c, 0);
    } else if (sfx == SFX_JUMP) {
        Mix_HaltChannel(CH_MINER);
        Mix_PlayChannel(CH_MINER, c, 0);
    } else if (sfx == SFX_FALL) {
        Mix_HaltChannel(CH_MINER);
        Mix_PlayChannel(CH_FALL, c, 0);
    } else {
        Mix_HaltChannel(CH_MINER);
        Mix_HaltChannel(CH_EFFECT);
        Mix_HaltChannel(CH_FALL);
        Mix_PlayChannel(CH_EFFECT, c, 0);
    }
}
