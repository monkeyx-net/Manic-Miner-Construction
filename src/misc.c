#include "misc.h"

// Initialises a fixed-point sub-tick accumulator so the engine fires at an exact rational
// rate (numerator/divisor ticks per frame) without floating-point drift.
void timer_set(TIMER *timer, int numerator, int divisor) {
    timer->acc = 0;
    timer->rate = numerator / divisor;
    timer->remainder = numerator - timer->rate * divisor;
    timer->divisor = divisor;
}

// Returns base rate or base+1 when the fractional accumulator overflows a divisor period,
// distributing remainder ticks evenly across frames.
int timer_update(TIMER *timer) {
    timer->acc += timer->remainder;
    if (timer->acc < timer->divisor) {
        return timer->rate;
    }
    timer->acc -= timer->divisor;
    return timer->rate + 1;
}

// Sprite rows are 16-bit words drawn most-significant bit leftmost; mirror draws them
// right-to-left instead, so no bit reversal of the sprite data is needed.

// Lists the pixel index of every set pixel of a 16×16 sprite whose top-left pixel is pos,
// row by row; returns how many were written to pixel[].
int sprite_pixels(int pos, const u16 *line, int mirror, int pixel[16 * 16]) {
    int     row, bit, at, count = 0;
    int     step = mirror ? 1 : -1;
    u16     word;

    if (!mirror) pos += 15;

    for (row = 0; row < 16; row++, pos += WIDTH, line++) {
        at = pos;
        word = *line;
        for (bit = 0; bit < 16; bit++, at += step, word >>= 1) {
            if (word & 1) {
                pixel[count++] = at;
            }
        }
    }

    return count;
}

int video_viewport(int width, int height, int *x, int *y, int *w, int *h) {
    int multiply = 1;

    while ((multiply + 1) * WIDTH <= width && (multiply + 1) * HEIGHT <= height)
        multiply++;

    *w = WIDTH * multiply;
    *h = HEIGHT * multiply;
    *x = (width - *w) / 2;
    *y = (height - *h) / 2;

    return multiply;
}
