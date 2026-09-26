#ifndef MISC_H
#define MISC_H

#include "common.h"

#define TICKRATE    60

#define SAMPLERATE  22050

typedef struct {
    u8      r;
    u8      g;
    u8      b;
    u8      padding;    
} COLOUR;

static const COLOUR video_colour[16] =
{
    {.r = 0x00, .g = 0x00, .b = 0x00},      
    {.r = 0x00, .g = 0x00, .b = 0xff},      
    {.r = 0xff, .g = 0x00, .b = 0x00},      
    {.r = 0xff, .g = 0x00, .b = 0xff},      
    {.r = 0x00, .g = 0xff, .b = 0x00},      
    {.r = 0x00, .g = 0xaa, .b = 0xff},      
    {.r = 0xff, .g = 0xff, .b = 0x00},      
    {.r = 0xff, .g = 0xff, .b = 0xff},      
    {.r = 0x80, .g = 0x80, .b = 0x80},      
    {.r = 0x00, .g = 0x55, .b = 0xff},      
    {.r = 0xaa, .g = 0x00, .b = 0x00},      
    {.r = 0x55, .g = 0x00, .b = 0x00},      
    {.r = 0x00, .g = 0xaa, .b = 0x00},      
    {.r = 0x00, .g = 0x55, .b = 0x00},      
    {.r = 0xff, .g = 0x80, .b = 0x00},      
    {.r = 0x80, .g = 0x40, .b = 0x00}       
};

#define C_BLACK      0
#define C_BLUE       1
#define C_RED        2
#define C_MAGENTA    3
#define C_GREEN      4
#define C_LIGHT_BLUE 5
#define C_YELLOW     6
#define C_WHITE      7
#define C_GREY       8
#define C_MID_BLUE   9
#define C_MID_RED   10
#define C_DARK_RED  11
#define C_MID_GREEN 12
#define C_DARK_GREEN 13
#define C_ORANGE    14
#define C_BROWN     15

typedef struct {
    int rate;
    int acc;
    int remainder;
    int divisor;
} TIMER;

void timer_set(TIMER *, int, int);
int timer_update(TIMER *);

int sprite_pixels(int pos, const u16 *line, int mirror, int pixel[16 * 16]);

int video_viewport(int, int, int *, int *, int *, int *);

void system_border(int);
void system_set_pixel(int, int);
void system_blit_surface(void *, int, int);
int system_poll_key(int);
int system_is_key(int);

#endif 
