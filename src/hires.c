#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include "misc.h"
#include "game.h"
#include "video.h"
#include "hires.h"
#include "assets.h"

#define HR_W  512
#define HR_H  256

static SDL_Texture *hr_tiles;
static SDL_Texture *title_miner_tex;
static SDL_Texture *live_miner_tex;
static int          lives_seq_idx  = 0;
static int          lives_seq_tick = 0;
static TTF_Font    *fps_font      = NULL;
static SDL_Texture *fps_tex       = NULL;
static int          fps_tex_w      = 0;
static int          fps_tex_h      = 0;
static int          fps_value     = -1;
static int          fps_frames    = 0;
static Uint32       fps_last_tick  = 0;

typedef struct { Uint32 *px; int w, h; } HrSprite;
enum { HR_MINER, HR_NPCS, HR_PORTAL, HR_BOOT, HR_TILES, HR_SPRITE_COUNT };
static HrSprite spr[HR_SPRITE_COUNT];

// How blit_sprite colours a 32×32 cell: a sheet pixel is "set" when its alpha is at least 128.
typedef enum {
    BLIT_SOLID,     // set pixels become col
    BLIT_TINT,      // set pixels become col, keeping the sheet pixel's alpha
    BLIT_SOURCE,    // set pixels keep their sheet colour, fully opaque
    BLIT_CUTOUT,    // as BLIT_SOURCE, and unset pixels become transparent
    BLIT_TWO_TONE   // set pixels become col, unset pixels become bg
} BLIT_MODE;

// Copies the 32×32 cell at sheet pixel (sx, sy) to (hx, hy) in buf, optionally mirrored.
// BLIT_CUTOUT and BLIT_TWO_TONE write every pixel of the cell; the other modes only set pixels.
static void blit_sprite(Uint32 *buf, int pitch32, const HrSprite *s, int sx, int sy,
                        int hx, int hy, int mirror, BLIT_MODE mode, Uint32 col, Uint32 bg) {
    int r, c;
    for (r = 0; r < 32; r++) {
        const Uint32 *src = s->px + (sy + r) * s->w + sx;
        Uint32       *dst = buf + (hy + r) * pitch32 + hx;
        for (c = 0; c < 32; c++) {
            Uint32 p   = src[mirror ? 31 - c : c];
            int    set = (p >> 24) >= 128;
            switch (mode) {
              case BLIT_SOLID:    if (set) dst[c] = col;                                      break;
              case BLIT_TINT:     if (set) dst[c] = (p & 0xff000000u) | (col & 0x00ffffffu);  break;
              case BLIT_SOURCE:   if (set) dst[c] = p | 0xff000000u;                          break;
              case BLIT_CUTOUT:   dst[c] = set ? (p | 0xff000000u) : 0;                       break;
              case BLIT_TWO_TONE: dst[c] = set ? col : bg;                                    break;
            }
        }
    }
}

static Uint32 ColourARGB(u8 idx) {
    const COLOUR *c = &video_colour[idx & 0xf];
    return 0xff000000u | ((Uint32)c->r << 16) | ((Uint32)c->g << 8) | c->b;
}

// Tiles overlapped by sprites drawn last frame; they are redrawn to erase the sprites
// before this frame's are drawn.
static u8 sprite_cover[512];

// Records the tiles a 32×32 sprite at (hx, hy) overlaps.
static void cover_sprite(int hx, int hy) {
    int r, c;
    for (r = hy / 16; r <= (hy + 31) / 16; r++)
        for (c = hx / 16; c <= (hx + 31) / 16; c++)
            if (r >= 0 && r < 16 && c >= 0 && c < 32)
                sprite_cover[r * 32 + c] = 1;
}

// The effect colours used for the previous frame, so a change redraws every tile.
static int fx_last;
static u8  fx_last_paper, fx_last_ink;

// Draws the level's 16×16 cell for a gfx slot from tiles.png (a row per level): set pixels in
// ink, clear in paper, and rows outside r0..r1-1 (counted in 8-row units) in bg. Slot -1 is a
// blank tile. A conveyor's rows 0-1 and 4-5 scroll 4 px per phase in opposite directions.
static void draw_tile(Uint32 *buf, int pitch32, int tx, int ty, int slot, int type,
                      int r0, int r1, Uint32 ink, Uint32 paper, Uint32 bg) {
    const HrSprite *s    = &spr[HR_TILES];
    const Uint32   *cell = s->px + game_level * 16 * s->w + (slot < 0 ? 0 : slot) * 16;
    int left = type == T_CONVEYL ? 0 : type == T_CONVEYR ? 2 : -1;
    int step = 4 * level_conveyor_phase();
    int r, c, shift;
    for (r = 0; r < 16; r++) {
        shift = left < 0 ? 0 : r / 2 == left ? step : r / 2 == 2 - left ? 16 - step : 0;
        for (c = 0; c < 16; c++)
            buf[(ty + r) * pitch32 + tx + c] = r < r0 * 2 || r >= r1 * 2 ? bg
                : slot >= 0 && (cell[r * s->w + ((c + shift) & 15)] >> 24) >= 128 ? ink : paper;
    }
}

void hires_init(SDL_Renderer *renderer) {
    hr_tiles = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, HR_W, HR_H);
    if (hr_tiles)
        SDL_SetTextureBlendMode(hr_tiles, SDL_BLENDMODE_BLEND);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    static const char *paths[HR_SPRITE_COUNT] = {
        "gfx/sprites/miner.png", "gfx/sprites/npcs.png",
        "gfx/sprites/portals.png", "gfx/sprites/boot.png", "gfx/sprites/tiles.png"
    };
    int i;
    for (i = 0; i < HR_SPRITE_COUNT; i++)
        spr[i].px = video_load_sprite_png(paths[i], &spr[i].w, &spr[i].h);

    fps_font = TTF_OpenFont("gfx/fonts/pixeldroidConsoleRegular.otf", 64);
    if (!fps_font)
        asset_problem("Cannot load gfx/fonts/pixeldroidConsoleRegular.otf: %s", TTF_GetError());

    title_miner_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING, 32, 32);
    if (title_miner_tex)
        SDL_SetTextureBlendMode(title_miner_tex, SDL_BLENDMODE_BLEND);

    live_miner_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, 32, 32);
    if (live_miner_tex)
        SDL_SetTextureBlendMode(live_miner_tex, SDL_BLENDMODE_BLEND);
}

// Reports a sprite sheet smaller than w×h pixels.
static void check_sheet(int which, const char *name, int w, int h, const char *why) {
    if (spr[which].px && (spr[which].w < w || spr[which].h < h))
        asset_problem("gfx/sprites/%s is %dx%d, must be at least %dx%d (%s)",
                      name, spr[which].w, spr[which].h, w, h, why);
}

// Checks the sprite sheets have a cell for everything levels.json uses. Called once the
// levels are loaded, so rendering never has to skip a sprite.
void hires_check_sheets(void) {
    check_sheet(HR_MINER, "miner.png", 4 * 32, 32, "4 frames");
    check_sheet(HR_NPCS, "npcs.png", 8 * 32, npcs_sprite_count() * 32, "8 frames per levels.json sprite");
    check_sheet(HR_PORTAL, "portals.png", 32, (num_levels + 1) * 32, "a portal per level, then the swordfish");
    check_sheet(HR_BOOT, "boot.png", 2 * 32, 32, "boot and plinth");
    check_sheet(HR_TILES, "tiles.png", 10 * 16, num_levels * 16, "10 tiles per level");
}

void hires_quit(void) {
    int i;
    if (hr_tiles) { SDL_DestroyTexture(hr_tiles); hr_tiles = NULL; }
    if (title_miner_tex) { SDL_DestroyTexture(title_miner_tex); title_miner_tex = NULL; }
    if (live_miner_tex)  { SDL_DestroyTexture(live_miner_tex);  live_miner_tex  = NULL; }
    if (fps_tex)        { SDL_DestroyTexture(fps_tex);        fps_tex        = NULL; }
    if (fps_font)       { TTF_CloseFont(fps_font);            fps_font       = NULL; }
    for (i = 0; i < HR_SPRITE_COUNT; i++) {
        if (spr[i].px) { SDL_free(spr[i].px); spr[i].px = NULL; }
    }
}

// Composites the 512×256 hi-res tile layer with NPC, miner, portal, and game-over sprites,
// then blits the result and the low-res HUD strip to the SDL renderer viewport. This is the
// only renderer of the playfield: while the miner dies or between levels, everything is
// drawn in the effect's paper and ink colours (lives_get_colours, trans_get_colours).
void hires_render(SDL_Renderer *renderer, SDL_Texture *gameTexture,
                  const SDL_Rect *viewport) {
    void   *pixels;
    int     pitch, pitch32;
    Uint32 *buf;
    int     i, r, c;
    int     tile_h, hud_h;
    SDL_Rect tileVP, hudSrc, hud_vp;
    int     fx;
    u8      fx_paper = 0, fx_ink = 0;

    if (!hr_tiles) return;

    tile_h  = viewport->h * 128 / 192;
    hud_h   = viewport->h - tile_h;
    tileVP = (SDL_Rect){ viewport->x, viewport->y,          viewport->w, tile_h };
    hudSrc = (SDL_Rect){ 0,           128,          256,                     64 };
    hud_vp  = (SDL_Rect){ viewport->x, viewport->y + tile_h,  viewport->w, hud_h  };

    SDL_LockTexture(hr_tiles, NULL, &pixels, &pitch);
    buf     = (Uint32 *)pixels;
    pitch32 = pitch / 4;

    fx = lives_get_colours(&fx_paper, &fx_ink) || trans_get_colours(&fx_paper, &fx_ink);
    if (fx != fx_last || (fx && (fx_paper != fx_last_paper || fx_ink != fx_last_ink)))
        level_mark_all_hires_dirty();
    fx_last       = fx;
    fx_last_paper = fx_paper;
    fx_last_ink   = fx_ink;

    if (game_is_gameover()) {
        for (r = 0; r < HR_H; r++)
            for (c = 0; c < HR_W; c++)
                buf[r * pitch32 + c] = 0x00000000u;
        for (i = 0; i < 512; i++)
            level_clear_hires_dirty(i);
        memset(sprite_cover, 0, sizeof sprite_cover);
    } else {
        Uint32 bg_col = ColourARGB(fx ? fx_paper : level_get_bg());

        for (i = 0; i < 512; i++) {
            TILE_RENDER t;
            int col = i % 32;
            int row = i / 32;
            int tx  = col * 16;
            int ty  = row * 16;
            int top = 0, bottom = 8;
            Uint32 paper_col, ink_col;

            level_get_render_tile(i, &t);

            // Items change colour every frame, so they are always redrawn.
            int animated = level_tile_is_animated(t.type, t.data) || t.type == T_ITEM;
            if (!level_is_hires_dirty(i) && !animated && !sprite_cover[i])
                continue;
            level_clear_hires_dirty(i);

            if (t.type == T_VOID) {
                Uint32 fill_col = (game_level == TWENTY) ? 0x00000000u : bg_col;
                for (r = 0; r < 16; r++)
                    for (c = 0; c < 16; c++)
                        buf[(ty + r) * pitch32 + (tx + c)] = fill_col;
                continue;
            }

            if (fx) {
                paper_col = bg_col;
                ink_col   = ColourARGB(fx_ink);
            } else {
                paper_col = (t.type == T_SPACE && (t.data & B_BEAM)) ? ColourARGB(6) : ColourARGB(t.paper);
                ink_col   = ColourARGB(t.ink);
            }

            if (t.shape == SHAPE_TOP_ROWS)
                bottom = t.data < 0 ? 0 : t.data > 8 ? 8 : t.data;
            else if (t.shape == SHAPE_BOTTOM_ROWS)
                top = t.data < 0 ? 0 : t.data > 8 ? 8 : t.data;

            draw_tile(buf, pitch32, tx, ty, t.slot, t.type, top, bottom, ink_col, paper_col, bg_col);
        }
        memset(sprite_cover, 0, sizeof sprite_cover);

        {
            NPC_RENDER bots[8];
            const HrSprite *npcs = &spr[HR_NPCS];
            npcs_get_render_data(bots);
            for (i = 0; i < 8; i++) {
                int    gfx, frame, hx, hy;
                Uint32 ink;

                if (!bots[i].active) continue;

                gfx   = bots[i].gfx;
                frame = bots[i].frame;
                hx    = bots[i].x * 2;
                hy    = bots[i].y * 2;
                ink   = ColourARGB(fx ? fx_ink : bots[i].ink);

                // Much of npcs.png is only half opaque. On most levels that shows the
                // border colour through; The Final Barrier shows the title picture layer
                // instead, so its NPCs are drawn solid.
                blit_sprite(buf, pitch32, npcs, frame * 32, gfx * 32, hx, hy, bots[i].mirror,
                            fx || game_level == TWENTY ? BLIT_SOLID : BLIT_TINT, ink, 0);
                cover_sprite(hx, hy);
            }
        }

        {
            MINER_RENDER m;
            const HrSprite *miner = &spr[HR_MINER];
            int hx, hy;

            miner_get_render_data(&m);
            if (game_is_victory()) {
                PORTAL_RENDER p;
                portal_get_render_data(&p);
                hx = p.x * 16;
                hy = p.y * 16 - 32;
            } else {
                hx = m.x * 2;
                hy = m.y * 2;
            }
            blit_sprite(buf, pitch32, miner, (m.sprite_idx & 3) * 32, 0, hx, hy,
                        m.mirror, fx ? BLIT_SOLID : BLIT_SOURCE, ColourARGB(fx_ink), 0);
            cover_sprite(hx, hy);
        }

        {
            PORTAL_RENDER p;
            const HrSprite *portal = &spr[HR_PORTAL];
            portal_get_render_data(&p);
            int    sy_base = (p.show_swordfish ? num_levels : game_level) * 32;
            if (p.gfx) {
                int    px      = p.x * 16;
                int    py      = p.y * 16;
                Uint32 set_col = ColourARGB(fx ? fx_ink   : p.colour[p.flash]);
                Uint32 clr_col = ColourARGB(fx ? fx_paper : p.colour[p.flash ^ 1]);

                if (p.show_swordfish)
                    blit_sprite(buf, pitch32, portal, 0, sy_base, px, py, 0, BLIT_SOURCE, 0, 0);
                else
                    blit_sprite(buf, pitch32, portal, 0, sy_base, px, py, 0, BLIT_TWO_TONE, set_col, clr_col);
                cover_sprite(px, py);
            }
        }
    }

     {
        GAMEOVER_RENDER g;
        const HrSprite *boot = &spr[HR_BOOT];
        gameover_get_render_data(&g);
        if (g.active) {
            Uint32 white  = ColourARGB(0x7);
            Uint32 black  = 0xff000000u;
            int    col_x   = g.boot_x * 2;
            int    boot_py = g.boot_visible ? (g.boot_y * 2) : (96 * 2);

            blit_sprite(buf, pitch32, boot, 0, 0, col_x, 112 * 2, 0, BLIT_SOLID, white, 0);

            if (g.boot_visible)
                blit_sprite(buf, pitch32, &spr[HR_MINER], 0, 0, col_x + 8, 96 * 2, 0, BLIT_SOURCE, 0, 0);

            for (r = 0; r < boot_py && r < HR_H; r++)
                for (c = 4; c < 20; c++) {
                    Uint32 pix = (((col_x + c) >> 1) + (r >> 1)) & 1 ? white : black;
                    buf[r * pitch32 + col_x + c] = pix;
                }

            blit_sprite(buf, pitch32, boot, 32, 0, col_x, boot_py, 0, BLIT_SOLID, white, 0);
        }
    }

    SDL_UnlockTexture(hr_tiles);

    if (game_level == TWENTY || game_is_gameover()) {
        SDL_Rect tileSrc = { 0, 0, 256, 128 };
        SDL_RenderCopy(renderer, gameTexture, &tileSrc, &tileVP);
    }
    SDL_RenderCopy(renderer, hr_tiles, NULL, &tileVP);

    SDL_RenderCopy(renderer, gameTexture, &hudSrc, &hud_vp);

    if (live_miner_tex) {
        void *px; int pitch, l;

        int frame_changed = 0;
        if (++lives_seq_tick >= 8) {
            lives_seq_tick = 0;
            lives_seq_idx  = (lives_seq_idx + 1) & 7;
            frame_changed = 1;
        }
        int frame_idx = lives_seq_idx & 3;
        int mirror   = (lives_seq_idx >= 4);

        if (frame_changed && SDL_LockTexture(live_miner_tex, NULL, &px, &pitch) == 0) {
            blit_sprite((Uint32 *)px, pitch / 4, &spr[HR_MINER], frame_idx * 32, 0, 0, 0,
                        mirror, BLIT_CUTOUT, 0, 0);
            SDL_UnlockTexture(live_miner_tex);
        }

        {
            SDL_Rect dst;
            dst.w = 16 * hud_vp.w / 256;
            dst.h = 16 * hud_vp.h / 64;
            dst.y = hud_vp.y + 22 * hud_vp.h / 64;
            for (l = 0; l < game_lives - 1; l++) {
                dst.x = hud_vp.x + (4 + l * 16) * hud_vp.w / 256;
                SDL_RenderCopy(renderer, live_miner_tex, NULL, &dst);
            }
        }
    }
}

// Draws the hi-res miner.png sprite at the title screen position (ZX pixel 236,80)
// with a transparent background, matching the scale used during gameplay.
void hires_draw_title_miner(SDL_Renderer *renderer, const SDL_Rect *viewport) {
    MINER_RENDER m;
    const HrSprite *miner = &spr[HR_MINER];
    void *px;
    int   pitch;

    if (!title_miner_tex) return;

    miner_get_seq_render_data(&m);

    if (SDL_LockTexture(title_miner_tex, NULL, &px, &pitch) < 0) return;
    blit_sprite((Uint32 *)px, pitch / 4, miner, (m.sprite_idx & 3) * 32, 0, 0, 0,
                m.mirror, BLIT_CUTOUT, 0, 0);
    SDL_UnlockTexture(title_miner_tex);

    {
        SDL_Rect dst = {
            viewport->x + 236 * viewport->w / 256,
            viewport->y +  80 * viewport->h / 192,
            16 * viewport->w / 256,
            16 * viewport->h / 192
        };
        SDL_RenderCopy(renderer, title_miner_tex, NULL, &dst);
    }
}

void hires_draw_fps_overlay(SDL_Renderer *renderer, const SDL_Rect *viewport) {
    Uint32 now = SDL_GetTicks();
    fps_frames++;

    if (fps_last_tick == 0) fps_last_tick = now;

    if (now - fps_last_tick >= 1000) {
        int new_fps = (int)((fps_frames * 1000u) / (now - fps_last_tick));
        fps_last_tick = now;
        fps_frames   = 0;

        if (new_fps != fps_value) {
            fps_value = new_fps;
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", fps_value);
            if (fps_font) {
                SDL_Color white = {255, 255, 255, 255};
                SDL_Surface *surf = TTF_RenderText_Blended(fps_font, buf, white);
                if (surf) {
                    if (fps_tex) SDL_DestroyTexture(fps_tex);
                    fps_tex  = SDL_CreateTextureFromSurface(renderer, surf);
                    if (fps_tex) SDL_SetTextureScaleMode(fps_tex, SDL_ScaleModeNearest);
                    fps_tex_w = surf->w;
                    fps_tex_h = surf->h;
                    SDL_FreeSurface(surf);
                }
            }
        }
    }

    if (!fps_tex) return;
    {
        int sh = 16 * viewport->h / 192;
        int sw = fps_tex_h ? (fps_tex_w * sh / fps_tex_h) : sh;
        SDL_Rect dst = {
            viewport->x + viewport->w - sw - 2,
            viewport->y,
            sw, sh
        };
        SDL_RenderCopy(renderer, fps_tex, NULL, &dst);
    }
}
