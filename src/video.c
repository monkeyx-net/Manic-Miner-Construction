#include <SDL_ttf.h>
#include <SDL_image.h>
#include <string.h>

#include "misc.h"
#include "game.h"
#include "video.h"
#include "assets.h"

static TTF_Font *fonts[VIDEO_FONT_COUNT];

void video_set_font(VIDEO_FONT which, void *f) {
    if ((unsigned)which < VIDEO_FONT_COUNT)
        fonts[which] = (TTF_Font *)f;
}

void *video_get_font(VIDEO_FONT which) {
    return ((unsigned)which < VIDEO_FONT_COUNT) ? fonts[which] : NULL;
}

static u8       video_pixel[WIDTH * HEIGHT];
static int      tile_pixel_offset[64];

static Uint32  *piano_keys_px;
static int      piano_keys_w;

static void     text_cache_clear(void);

// Loads a PNG and copies its ARGB32 pixels into a plain heap array for use by the hi-res renderer.
Uint32 *video_load_sprite_png(const char *path, int *out_w, int *out_h) {
    SDL_Surface *surf = IMG_Load(path);
    SDL_Surface *conv;
    Uint32      *px;
    int          w, h, x, y;

    if (!surf) {
        asset_problem("Cannot load %s: %s", path, IMG_GetError());
        return NULL;
    }
    conv = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(surf);
    if (!conv) {
        asset_problem("Cannot convert %s", path);
        return NULL;
    }

    w  = conv->w;
    h  = conv->h;
    px = (Uint32 *)SDL_malloc((size_t)(w * h) * 4);
    if (px) {
        SDL_LockSurface(conv);
        for (y = 0; y < h; y++) {
            Uint32 *src = (Uint32 *)((Uint8 *)conv->pixels + y * conv->pitch);
            Uint32 *dst = px + y * w;
            for (x = 0; x < w; x++) dst[x] = src[x];
        }
        SDL_UnlockSurface(conv);
        *out_w = w;
        *out_h = h;
    }
    SDL_FreeSurface(conv);
    return px;
}

// Precomputes each tile pixel's offset from the tile's top-left pixel and loads the piano keys
// sprite sheet.
void video_init() {
    int point;
    for (point = 0; point < 64; point++) {
        tile_pixel_offset[point] = (point / 8) * WIDTH + point % 8;
    }

    {
        int h;
        piano_keys_px = video_load_sprite_png("gfx/sprites/piano_keys.png", &piano_keys_w, &h);
        if (piano_keys_px && (piano_keys_w < 4 * 7 || h < 16))
            asset_problem("gfx/sprites/piano_keys.png is %dx%d, must be at least 28x16 (4 keys)",
                          piano_keys_w, h);
    }
}

void video_quit(void) {
    if (piano_keys_px) { SDL_free(piano_keys_px); piano_keys_px = NULL; }
    text_cache_clear();
}

void video_draw_piano_key(int pos, int note, int ink) {
    int sx_base = note * 7;
    int col, row;
    for (col = 0; col < 7; col++) {
        for (row = 0; row < 16; row++) {
            Uint32 pix = piano_keys_px[row * piano_keys_w + sx_base + col];
            if ((pix >> 24) >= 128)
                system_set_pixel(pos + col + row * WIDTH, ink);
        }
    }
}

// Rendered text is cached as a 1-bit mask per (font, string) so repeated strings such as the
// title ticker or HUD labels are rasterised by SDL_ttf once rather than every frame. Ink is
// applied at blit time, so one entry serves every colour. Each entry also keeps the string's
// advance width so callers such as the ticker need not re-measure it. Least-recently-used entries
// are evicted.
#define TEXT_CACHE_SIZE 64

typedef struct {
    TTF_Font   *font;
    char       *text;
    u8         *mask;
    int         w, h;
    int         advance;
    Uint32      used;
} TEXT_CACHE;

static TEXT_CACHE   text_cache[TEXT_CACHE_SIZE];
static Uint32       text_cache_clock;

static const TEXT_CACHE *text_cache_get(TTF_Font *font, const char *text)
{
    TEXT_CACHE  *e, *victim = &text_cache[0];
    SDL_Surface *s;
    SDL_Color    white = {255, 255, 255, 255};
    u8          *mask;
    int          i, row, col;

    text_cache_clock++;
    for (i = 0; i < TEXT_CACHE_SIZE; i++) {
        e = &text_cache[i];
        if (e->text && e->font == font && strcmp(e->text, text) == 0) {
            e->used = text_cache_clock;
            return e;
        }
        if (!e->text) victim = e;
        else if (victim->text && e->used < victim->used) victim = e;
    }

    s = TTF_RenderUTF8_Solid(font, text, white);
    if (!s) return NULL;
    mask = SDL_malloc((size_t)(s->w * s->h));
    if (!mask) { SDL_FreeSurface(s); return NULL; }
    SDL_LockSurface(s);
    for (row = 0; row < s->h; row++)
        for (col = 0; col < s->w; col++)
            mask[row * s->w + col] = ((Uint8 *)s->pixels)[row * s->pitch + col] != 0;
    SDL_UnlockSurface(s);

    SDL_free(victim->text);
    SDL_free(victim->mask);
    victim->font = font;
    victim->text = SDL_strdup(text);
    victim->mask = mask;
    victim->w    = s->w;
    victim->h    = s->h;
    if (TTF_SizeUTF8(font, text, &victim->advance, NULL) != 0)
        victim->advance = s->w;
    victim->used = text_cache_clock;
    SDL_FreeSurface(s);
    return victim;
}

static void text_cache_clear(void)
{
    int i;
    for (i = 0; i < TEXT_CACHE_SIZE; i++) {
        SDL_free(text_cache[i].text);
        SDL_free(text_cache[i].mask);
        text_cache[i].text = NULL;
        text_cache[i].mask = NULL;
    }
}

// Draws a cached text mask into the game's own pixel buffer at (x, y) with the given ink colour,
// visiting only the part of the mask that lies on screen.
static void text_blit(const TEXT_CACHE *e, int x, int y, u8 ink)
{
    int row, col, r0, r1, c0, c1;

    r0 = y < 0 ? -y : 0;
    r1 = e->h < HEIGHT - y ? e->h : HEIGHT - y;
    c0 = x < 0 ? -x : 0;
    c1 = e->w < WIDTH - x ? e->w : WIDTH - x;
    for (row = r0; row < r1; row++) {
        const u8 *m = e->mask + row * e->w;
        for (col = c0; col < c1; col++) {
            if (m[col])
                system_set_pixel((y + row) * WIDTH + (x + col), ink);
        }
    }
}

static void ttf_render(TTF_Font *font, int x, int y, const char *text, u8 ink)
{
    const TEXT_CACHE *e;

    e = text_cache_get(font, text);
    if (e) text_blit(e, x, y, ink);
}

static int ttf_width(TTF_Font *font, const char *text)
{
    int w;
    TTF_SizeUTF8(font, text, &w, NULL);
    return w;
}

static TTF_Font *get_font(VIDEO_FONT font) {
    return ((unsigned)font < VIDEO_FONT_COUNT) ? fonts[font] : fonts[VIDEO_FONT_SMALL];
}

int video_write_f(int row, int x, u8 ink, const char *text, VIDEO_FONT font) {
    TTF_Font *f = get_font(font);
    ttf_render(f, x, row, text, ink);
    return x + ttf_width(f, text);
}

int video_text_width_f(const char *text, VIDEO_FONT font) {
    return ttf_width(get_font(font), text);
}

int video_text_height_f(VIDEO_FONT font) {
    TTF_Font *f = get_font(font);
    return f ? TTF_FontHeight(f) : 0;
}

void video_air_bar(int pixel, u8 ink) {
    system_set_pixel(pixel, ink);
    system_set_pixel(pixel += WIDTH, ink);
    system_set_pixel(pixel += WIDTH, ink);
    system_set_pixel(pixel += WIDTH, ink);
}

// Recolours a tile's ink pixels (ink_pixels set) or its paper pixels (ink_pixels clear).
static void video_tile_fill(int tile, u8 colour, int ink_pixels) {
    int point, pos;
    int pixel = TILE2PIXEL(tile);
    u8  want = ink_pixels ? B_INK : 0;

    for (point = 0; point < 64; point++) {
        pos = pixel + tile_pixel_offset[point];
        if ((video_pixel[pos] & B_INK) == want) {
            system_set_pixel(pos, colour);
        }
    }
}

void video_tile_paper(int tile, u8 paper) { video_tile_fill(tile, paper, 0); }

void video_level_paper_fill(u8 paper) {
    int dest;

    for (dest = 0; dest < 512; dest++) {
        video_tile_paper(dest, paper);
    }
}

void video_tile_ink(int tile, u8 ink) { video_tile_fill(tile, ink, 1); }

void video_level_ink_fill(u8 ink) {
    int dest;

    for (dest = 0; dest < 512; dest++) {
        video_tile_ink(dest, ink);
    }
}

void video_pixel_fill(int pixel, int size, u8 ink) {
    for ( ; size > 0; size--, pixel++) {
        video_pixel[pixel] = 0;
        system_set_pixel(pixel, ink);
    }
}

int video_write(int pos, u8 ink, const char *text, VIDEO_FONT font) {
    return video_write_f(pos / WIDTH, pos & 0xff, ink, text, font);
}

void video_ticker(const TICKER_SEG *segs, int start_x, int y) {
    TTF_Font *f = fonts[VIDEO_FONT_LARGE];
    int x = start_x;
    if (!f) return;
    for ( ; segs->text; segs++) {
        const TEXT_CACHE *e = text_cache_get(f, segs->text);
        if (!e) continue;
        if (x + e->advance > 0 && x < WIDTH)
            text_blit(e, x, y, segs->ink);
        x += e->advance;
        if (x >= WIDTH) break;
    }
}

int video_ticker_width(const TICKER_SEG *segs) {
    TTF_Font *f = fonts[VIDEO_FONT_LARGE];
    int w = 0;
    for ( ; segs->text; segs++)
        w += ttf_width(f, segs->text);
    return w;
}

void video_copy_bytes(u8 *src) {
    u8      *pixel = &video_pixel[0];
    int     size, bit;
    u8      byte;

    for (size = 0; size < 2048 + 256; size++, src++) {
        byte = *src;
        for (bit = 0; bit < 8; bit++, byte <<= 1, pixel++) {
            *pixel = byte >> 7;
        }
    }
}

void video_copy_colour(u8 *src, int dest, int size) {
    for ( ; size > 0; size--, dest++, src++) {
        video_tile_paper(dest, *src >> 4);
        video_tile_ink(dest, *src & 0x0f);
    }
}
