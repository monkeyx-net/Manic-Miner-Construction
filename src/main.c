#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "misc.h"
#include "video.h"
#include "audio.h"
#include "replay.h"
#include "savestate.h"
#include "game.h"
#include "gamedata.h"
#include "hires.h"
#include "assets.h"

static TTF_Font *open_font(const char *path, int size) {
    TTF_Font *font = TTF_OpenFont(path, size);
    if (!font) asset_problem("Cannot load %s: %s", path, TTF_GetError());
    return font;
}

static SDL_Window           *sdlWindow;
static SDL_Renderer         *sdl_renderer;
static SDL_Texture          *sdl_texture, *sdl_target;
static SDL_Surface          *sdl_surface;
static SDL_Rect             sdl_viewport;

static const Uint8          *key_state;
static SDL_GameController   *controller = NULL;

static void ctrl_open(int idx) {
    if (!SDL_IsGameController(idx) || controller) return;
    controller = SDL_GameControllerOpen(idx);
}

static void ctrl_close(SDL_JoystickID id) {
    if (!controller) return;
    if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) != id) return;
    SDL_GameControllerClose(controller);
    controller = NULL;
    for (int i = 0; i < SDL_NumJoysticks(); i++) ctrl_open(i);
}

const COLOUR                *sys_border = &video_colour[0];

static int                  game_running   = 1;
static int                  is_fullscreen  = 1;
static int                  window_scale   = 3;
static const char          *screenshot_path = NULL;
static int                  screenshot_frames = 0;
static int                  no_sticks      = 0;

// Fits the integer-scaled viewport into a w×h window and recreates the render target to match.
static void set_viewport(int w, int h) {
    int mul = video_viewport(w, h, &sdl_viewport.x, &sdl_viewport.y,
                                   &sdl_viewport.w, &sdl_viewport.h);
    if (sdl_target) SDL_DestroyTexture(sdl_target);
    sdl_target = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_TARGET, WIDTH * mul, HEIGHT * mul);
}

// Switches between fullscreen-desktop and windowed mode, recreating the scaled render target texture.
static void apply_display_mode(int fullscreen) {
    SDL_DisplayMode dm;
    int w, h;

    is_fullscreen = fullscreen;

    if (fullscreen) {
        SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_GetDesktopDisplayMode(0, &dm);
        w = dm.w;  h = dm.h;
    } else {
        SDL_SetWindowFullscreen(sdlWindow, 0);
        w = WIDTH * window_scale;  h = HEIGHT * window_scale;
        SDL_SetWindowSize(sdlWindow, w, h);
        SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    set_viewport(w, h);
}

int                         game_input;

int                         video_sync = 0;

static TIMER                timer_frame;

EVENT                       action = loader_action;
EVENT                       responder = do_nothing;
EVENT                       ticker = do_nothing;
EVENT                       drawer = do_nothing;

static const SDL_Keycode    sdl_key[] = {
     SDLK_LEFT, SDLK_RIGHT, SDLK_SPACE, SDLK_RETURN, SDLK_LSHIFT, SDLK_RSHIFT,
     SDLK_0, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9,
     SDLK_ESCAPE, SDLK_PAUSE, SDLK_LALT, SDLK_UNKNOWN, SDLK_UNKNOWN,
     SDLK_s, SDLK_u, SDLK_o, SDLK_UP, SDLK_DOWN, SDLK_r, SDLK_l, SDLK_DELETE
};

void do_nothing() {
}

void do_quit() {
    game_running = 0;
    drawer = do_nothing;
    ticker = do_nothing;
}

int system_poll_key(int key) {
    int max_key = (int)(sizeof(sdl_key) / sizeof(sdl_key[0]));

    if (key >= 0 && key < max_key && sdl_key[key] != SDLK_UNKNOWN &&
        key_state[SDL_GetScancodeFromKey(sdl_key[key])]) {
        return 1;
    }

    if (!no_sticks && controller) {
        Sint16 ax = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
        Sint16 ay = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
        switch (key) {
          case KEY_LEFT:  return SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT)  || ax < -8000;
          case KEY_RIGHT: return SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || ax >  8000;
          case KEY_JUMP:  return SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) ||
                                 SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);
          case KEY_UP:    return SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP)    || ay < -8000;
          case KEY_DOWN:  return SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN)  || ay >  8000;
          default: break;
        }
    }

    return 0;
}

int system_is_key(int key) {
    if (replay_mode == REPLAY_PLAYING) {
        return replay_is_key(key);
    }
    return system_poll_key(key);
}

// Polls one SDL event and translates keyboard and controller input to internal KEY_* constants;
// fullscreen toggle and window resize are handled as side-effects.
static int system_get_event() {
    SDL_Event   event;

    game_input = KEY_NONE;

    if (SDL_PollEvent(&event) == 0) {
        return 0;
    }

    if (event.type == SDL_QUIT) {
        do_quit();
    }

    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        set_viewport(event.window.data1, event.window.data2);
        return 1;
    }

    if (!no_sticks && event.type == SDL_CONTROLLERDEVICEADDED) {
        ctrl_open(event.cdevice.which);
        return 1;
    }

    if (!no_sticks && event.type == SDL_CONTROLLERDEVICEREMOVED) {
        ctrl_close(event.cdevice.which);
        return 1;
    }

    if (!no_sticks && event.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (event.cbutton.button) {
          case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     game_input = KEY_LEFT;   break;
          case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    game_input = KEY_RIGHT;  break;
          case SDL_CONTROLLER_BUTTON_DPAD_UP:       game_input = KEY_UP;     break;
          case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     game_input = KEY_DOWN;   break;
          case SDL_CONTROLLER_BUTTON_A:
          case SDL_CONTROLLER_BUTTON_START:         game_input = KEY_ENTER;  break;
          case SDL_CONTROLLER_BUTTON_B:
          case SDL_CONTROLLER_BUTTON_BACK:          game_input = KEY_ESCAPE; break;
          case SDL_CONTROLLER_BUTTON_X:
          case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  game_input = KEY_PAUSE;  break;
          case SDL_CONTROLLER_BUTTON_Y:
          case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: game_input = KEY_MUTE;   break;
        }
        return 1;
    }

    if (event.type != SDL_KEYDOWN) {
        return 1;
    }

    if (event.key.repeat) {
        return 1;
    }

    if (event.key.keysym.sym == SDLK_F11 ||
        (event.key.keysym.sym == SDLK_RETURN &&
         (event.key.keysym.mod & (KMOD_LALT | KMOD_RALT)))) {
        apply_display_mode(!is_fullscreen);
        return 1;
    }

    switch (event.key.keysym.sym) {
      case SDLK_RETURN:
        game_input = KEY_ENTER;
        break;

      case SDLK_ESCAPE:
        game_input = KEY_ESCAPE;
        break;

      case SDLK_PAUSE:
      case SDLK_TAB:
        game_input = KEY_PAUSE;
        break;

      case SDLK_LALT:
      case SDLK_RALT:
        game_input = KEY_MUTE;
        break;

      case SDLK_0:
      case SDLK_1:
      case SDLK_2:
      case SDLK_3:
      case SDLK_4:
      case SDLK_5:
      case SDLK_6:
      case SDLK_7:
      case SDLK_8:
      case SDLK_9:
        game_input = KEY_0 + (event.key.keysym.sym - SDLK_0);
        break;

      case SDLK_LEFT:
        game_input = KEY_LEFT;
        break;

      case SDLK_RIGHT:
        game_input = KEY_RIGHT;
        break;

      case SDLK_UP:
        game_input = KEY_UP;
        break;

      case SDLK_DOWN:
        game_input = KEY_DOWN;
        break;

      case SDLK_l:
        game_input = KEY_L;
        break;

      case SDLK_s:
        game_input = KEY_S;
        break;

      case SDLK_o:
        game_input = KEY_O;
        break;

      case SDLK_r:
        game_input = KEY_R;
        break;

      case SDLK_u:
        game_input = KEY_U;
        break;

      case SDLK_DELETE:
        game_input = KEY_DELETE;
        break;

      default:
        game_input = KEY_ELSE;
    }

    return 1;
}

// The streaming texture is ARGB8888, so each pixel is one Uint32 taken from a palette
// built once at start-up; sys_pixels/sys_pitch32 are refreshed on every texture lock.
static Uint32               sys_palette[16];
static Uint32              *sys_pixels;
static int                  sys_pitch32;

static void system_init_palette(void) {
    for (int i = 0; i < 16; i++)
        sys_palette[i] = 0xff000000u | ((Uint32)video_colour[i].r << 16) |
                         ((Uint32)video_colour[i].g << 8) | video_colour[i].b;
}

void system_set_pixel(int point, int index) {
    sys_pixels[(unsigned)point / WIDTH * sys_pitch32 + (unsigned)point % WIDTH] = sys_palette[index];
}

void system_blit_surface(void *src, int dst_x, int dst_y) {
    SDL_Rect dst = {dst_x, dst_y, 0, 0};
    SDL_BlitSurface((SDL_Surface *)src, NULL, sdl_surface, &dst);
}

void system_border(int index) {
    sys_border = &video_colour[index];
}

static void main_loop_iteration(void);

// Loads the levels and checks every asset the game needs. If anything is missing or wrong,
// shows what and returns 0: the game does not run with parts of it missing.
static int game_ready(const char *levels_path) {
    char message[2400];

    if (!game_data_load(levels_path)) {
        snprintf(message, sizeof message, "levels.json: %s", game_data_error());
    } else {
        hires_check_sheets();
        if (asset_problems()[0] == '\0') return 1;
        snprintf(message, sizeof message, "%s", asset_problems());
    }
    SDL_Log("Cannot start: %s", message);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Manic Miner cannot start", message, sdlWindow);
    return 0;
}

#ifdef __EMSCRIPTEN__
static EM_BOOL EmscriptenOnResize(int eventType, const EmscriptenUiEvent *e, void *user_data) {
    SDL_SetWindowSize(sdlWindow, e->windowInnerWidth, e->windowInnerHeight);
    return EM_FALSE;
}

// The game cannot run without its levels, so it only starts once levels.json has loaded.
static void EmLevelsOk(void *arg, void *buf, int size) {
    (void)arg;
    FILE *f = fopen("/levels_live.json", "wb");
    if (f) { fwrite(buf, 1, (size_t)size, f); fclose(f); }
    if (!game_ready("/levels_live.json")) return;
    emscripten_set_main_loop(main_loop_iteration, 0, 1);
}

static void EmLevelsErr(void *arg) {
    (void)arg;
    SDL_Log("Cannot download levels.json");
}
#endif

// Runs all pending game ticks (action/input/ticker/drawer) in a timer-gated loop,
// then presents hi-res or standard render to the SDL renderer.
static void main_loop_iteration(void) {
    int frame;

    if (!game_running) {
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        return;
    }

    frame = timer_update(&timer_frame);

    if (frame > 0) {
        SDL_LockTextureToSurface(sdl_texture, NULL, &sdl_surface);
        sys_pixels  = (Uint32 *)sdl_surface->pixels;
        sys_pitch32 = sdl_surface->pitch / 4;

        while (frame--) {
            action();

            while (system_get_event()) {
                if (game_input != KEY_NONE) {
                    responder();
                }
            }

            ticker();
            replay_tick();
            drawer();
            replay_draw_osd();
            game_draw_save_osd();

            video_sync = 0;

            if (screenshot_path && screenshot_frames > 0 && --screenshot_frames == 0) {
                SDL_Surface *rgb = SDL_ConvertSurfaceFormat(sdl_surface, SDL_PIXELFORMAT_RGB24, 0);
                if (rgb) { IMG_SavePNG(rgb, screenshot_path); SDL_FreeSurface(rgb); }
                game_running = 0;
            }
        }

        SDL_UnlockTexture(sdl_texture);
    } else {
        SDL_Delay(1);
    }

    SDL_SetRenderDrawColor(sdl_renderer, sys_border->r, sys_border->g, sys_border->b, 0xff);
    SDL_RenderClear(sdl_renderer);

    if (game_is_on_screen()) {
        hires_render(sdl_renderer, sdl_texture, &sdl_viewport);
    } else {
        SDL_SetRenderTarget(sdl_renderer, sdl_target);
        SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
        SDL_SetRenderTarget(sdl_renderer, NULL);
        SDL_RenderCopy(sdl_renderer, sdl_target, NULL, &sdl_viewport);
        if (title_miner_active())
            hires_draw_title_miner(sdl_renderer, &sdl_viewport);
    }

    if (game_config_show_fps)
        hires_draw_fps_overlay(sdl_renderer, &sdl_viewport);

    SDL_RenderPresent(sdl_renderer);
}

int main(int argc, char *argv[]) {
    int             win_w, win_h;
    int             refresh_rate;
#ifndef __EMSCRIPTEN__
    SDL_DisplayMode mode;
#endif

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [OPTIONS]\n\n"
                   "Options:\n"
                   "  -h, --help              Show this help message and exit\n"
                   "  -w, --window            Start in windowed mode (default: fullscreen)\n"
                   "  -s, --scale N           Set window scale factor 1-8 (default: 3, implies --window)\n"
                   "      --screenshot PATH   Save PNG screenshot after 400 frames and exit\n"
                   "  -ns, --nosticks         Disable game controller / joystick support\n",
                   argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--window") == 0 || strcmp(argv[i], "-w") == 0) {
            is_fullscreen = 0;
        } else if ((strcmp(argv[i], "--scale") == 0 || strcmp(argv[i], "-s") == 0) && i + 1 < argc) {
            int s = atoi(argv[++i]);
            if (s >= 1 && s <= 8) { window_scale = s; is_fullscreen = 0; }
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot_path   = argv[++i];
            screenshot_frames = 400;
            is_fullscreen = 0;
        } else if (strcmp(argv[i], "--nosticks") == 0 || strcmp(argv[i], "-ns") == 0) {
            no_sticks = 1;
        }
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | (no_sticks ? 0 : SDL_INIT_GAMECONTROLLER));
    TTF_Init();

    TTF_Font *font_small      = open_font("gfx/fonts/pixeldroidConsoleRegular.otf", 20);
    TTF_Font *font_large      = open_font("gfx/fonts/PressStart2P_400Regular.ttf", 9);
    TTF_Font *font_title      = open_font("gfx/fonts/MANICMINER-Regular.otf", 14);
    TTF_Font *font_extra_large = open_font("gfx/fonts/MANICMINER-Regular.otf", 40);
    video_set_font(VIDEO_FONT_SMALL,       font_small);
    video_set_font(VIDEO_FONT_LARGE,       font_large);
    video_set_font(VIDEO_FONT_TITLE,       font_title);
    video_set_font(VIDEO_FONT_EXTRA_LARGE, font_extra_large);

    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
#ifdef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
#endif

#ifdef __EMSCRIPTEN__
    {
        win_w = EM_ASM_INT({ return window.innerWidth; });
        win_h = EM_ASM_INT({ return window.innerHeight; });
        sdlWindow = SDL_CreateWindow("Manic Miner", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, 0);
        emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_FALSE,
            EmscriptenOnResize);
    }
#else
    SDL_GetDesktopDisplayMode(0, &mode);

    if (is_fullscreen) {
        win_w = mode.w;
        win_h = mode.h;
        sdlWindow = SDL_CreateWindow("Manic Miner", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        win_w = WIDTH * window_scale;
        win_h = HEIGHT * window_scale;
        sdlWindow = SDL_CreateWindow("Manic Miner", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, 0);
    }
#endif

    sdl_renderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_TARGETTEXTURE | SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    set_viewport(win_w, win_h);
    sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

    IMG_Init(IMG_INIT_PNG);
    system_init_palette();
    video_init();
    title_init();
    hires_init(sdl_renderer);
    audio_init();

    SDL_ShowCursor(SDL_DISABLE);

    key_state = SDL_GetKeyboardState(NULL);
    if (!no_sticks) {
        for (int i = 0; i < SDL_NumJoysticks(); i++) ctrl_open(i);
    }

#ifdef __EMSCRIPTEN__
    refresh_rate = 60;
#else
    refresh_rate = mode.refresh_rate > 0 ? mode.refresh_rate : 60;
#endif
    timer_set(&timer_frame, TICKRATE, refresh_rate);

    game_config_load();
    replay_load();

#ifdef __EMSCRIPTEN__

    emscripten_async_wget_data("levels.json", NULL, EmLevelsOk, EmLevelsErr);
#else
    int levels_loaded = game_ready("levels.json");
    if (!levels_loaded) game_running = 0;
    while (game_running) {
        main_loop_iteration();
    }

    audio_quit();
    hires_quit();
    video_quit();
    IMG_Quit();
    TTF_CloseFont(font_small);
    TTF_CloseFont(font_large);
    TTF_CloseFont(font_title);
    TTF_CloseFont(font_extra_large);
    TTF_Quit();
    SDL_DestroyTexture(sdl_texture);
    SDL_DestroyTexture(sdl_target);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(sdlWindow);

    SDL_Quit();

    if (!levels_loaded) return 1;
#endif
    return 0;
}
