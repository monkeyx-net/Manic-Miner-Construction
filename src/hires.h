#ifndef HIRES_H
#define HIRES_H
#include <SDL.h>

void hires_init(SDL_Renderer *renderer);
void hires_render(SDL_Renderer *renderer, SDL_Texture *gameTexture, const SDL_Rect *viewport);
void hires_draw_title_miner(SDL_Renderer *renderer, const SDL_Rect *viewport);
void hires_draw_fps_overlay(SDL_Renderer *renderer, const SDL_Rect *viewport);
void hires_quit(void);
void hires_check_sheets(void);

#endif 
