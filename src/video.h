#include <SDL.h>

// Per-pixel flag in the low-res pixel buffer: the pixel is ink rather than paper. Set by the
// title picture, so its colours can be applied per 8×8 cell and recoloured by the death and
// level-transition effects on The Final Barrier.
#define B_INK    1

// Tile-data flags for the Solar Power Generator beam: what is standing in a beam tile.
#define B_NPC    2
#define B_MINER  4
#define B_BEAM   8

// The playfield is 32×16 tiles of 8×8 pixels; a pixel index is y * WIDTH + x.
#define TILE2PIXEL(t)   (((t) / 32) * 8 * WIDTH + ((t) % 32) * 8)

#define KEYBOARD    128 * WIDTH

typedef struct { u8 ink; const char *text; } TICKER_SEG;

typedef enum { VIDEO_FONT_SMALL, VIDEO_FONT_LARGE, VIDEO_FONT_TITLE, VIDEO_FONT_EXTRA_LARGE, VIDEO_FONT_COUNT } VIDEO_FONT;

Uint32 *video_load_sprite_png(const char *path, int *out_w, int *out_h);

void video_init(void);
void video_quit(void);
void video_draw_piano_key(int pos, int note, int ink);
void video_copy_colour(u8 *, int, int);
void video_copy_bytes(u8 *);
int  video_write_f(int, int, u8, const char *, VIDEO_FONT);
int  video_text_width_f(const char *, VIDEO_FONT);
int  video_text_height_f(VIDEO_FONT);
int  video_write(int, u8, const char *, VIDEO_FONT);
void video_pixel_fill(int, int, u8);
void video_level_ink_fill(u8);
void video_level_paper_fill(u8);
void video_tile_ink(int, u8);
void video_tile_paper(int, u8);
void video_air_bar(int, u8);
void video_set_font(VIDEO_FONT which, void *f);
void *video_get_font(VIDEO_FONT which);
void video_ticker(const TICKER_SEG *, int, int);
int  video_ticker_width(const TICKER_SEG *);
