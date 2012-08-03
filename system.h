#ifndef __system_h
#define __system_h

#include <SDL.h>

extern SDL_Surface *screen;

extern bool fullscreen;

void open_window(int w, int h);
void resize(const SDL_Event *e);
double get_dt();

#endif
