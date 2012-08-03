/*
 * SEKO
 *
 * Copyright 2012 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 */
#include "utils.h"
#include "gl.h"
#include "sound.h"
#include "system.h"
#include "menu.h"
#include <SDL.h>
#include <stdexcept>
#include <unistd.h>
#include <fenv.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

bool debug_enabled = false;
GLFont vera;

int main(int argc, char **argv)
try {
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "-window") {
			fullscreen = false;
		} else if (arg == "-debug") {
			debug_enabled = true;
		} else {
			printf("Uknown argument: %s\n", argv[i]);
		}
	}

	srand(time(NULL));

	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)) {
		throw std::runtime_error(strf("Can not initialize SDL: %s",
					      SDL_GetError()));
	}
	open_window(1024, 768);

	glewInit();

	if (!GLEW_VERSION_1_5) {
		throw std::runtime_error("OpenGL 1.5 required!");
	}

	chdir("data");

#ifdef __linux__
	std::string opengl_vendor = (char *) glGetString(GL_VENDOR);
	if (debug_enabled && 0 && opengl_vendor != "ATI Technologies Inc.") {
		feenableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW);
	}
#endif

	const float black[] = {0, 0, 0, 1};
	const float specular[] = {0.2, 0.2, 0.2, 1};
	glEnable(GL_CULL_FACE);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, black);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 30);
	glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);

	init_sound();

	vera.open("Vera.ttf", 16);

	menu();
	return 0;

} catch (const std::runtime_error &e) {
	SDL_Quit();
#ifdef _WIN32
	MessageBox(NULL, e.what(), "Error", MB_OK|MB_ICONERROR);
#else
	fprintf(stderr, "ERROR: %s\n", e.what());
#endif
	return 1;
}

