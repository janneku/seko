#ifndef __game_h
#define __game_h

#include "gl.h"

struct Pattern {
	Color color;
	const char *descr;
	int pattern[10];
};

struct Section {
	double t, bpm;
	const Pattern *pattern;
};

struct Stage;

struct Level {
	const char *name;
	const char *descr;
	const Section *sections;
	const char *music;
	const Stage *stage;
	int num_zombies;
};

enum {
	EXCELLENT,
	GOOD,
	TRYAGAIN,
	MAX_MESSAGE
};

extern GLdouble modelMatrix[16];
extern GLdouble projMatrix[16];
extern const Stage *stage;
extern const Level levels[];
extern const Level *level;
extern int score;
extern int score_verify;
extern int secret;
extern int msg;
extern double msg_visible;
extern bool quit_game;

bool game();

#endif
