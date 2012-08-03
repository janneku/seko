#include "system.h"
#include "utils.h"
#include "gl.h"
#include <stdexcept>

SDL_Surface *screen = NULL;
bool fullscreen = true;

void open_window(int w, int h)
{
	int flags = SDL_OPENGL|SDL_RESIZABLE;
	if (fullscreen) {
		/* Use system's resolution */
		flags |= SDL_FULLSCREEN;
		w = 0;
		h = 0;
	}
	screen = SDL_SetVideoMode(w, h, 0, flags);
	if (screen == NULL) {
		throw std::runtime_error(strf("Unable to create window: %s",
					      SDL_GetError()));
	}
	glViewport(0, 0, screen->w, screen->h);
}

void resize(const SDL_Event *e)
{
#ifdef _WIN32
	/* HACK: Don't discard GL context */
	screen->w = e->resize.w;
	screen->h = e->resize.h;
	glViewport(0, 0, e->resize.w, e->resize.h);
#else
	open_window(e->resize.w, e->resize.h);
#endif
}

double get_dt()
{
	static Uint32 last_ticks = 0;
	Uint32 ticks = SDL_GetTicks();
	double dt = std::max(std::min((ticks - last_ticks) * 0.001, 0.1), 0.0);
	last_ticks = ticks;
	return dt;
}
