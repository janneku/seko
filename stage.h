#ifndef __stage_h
#define __stage_h

#include "vec.h"
#include <string.h>

struct Block {
	size_t num_walls;
	const Plane *walls;
};

struct Stage {
	const char *model;
	double scale;
	vec3 origo;
	double size;
	size_t num_blocks;
	const Block blocks[10];
};

extern const Stage graveyard;
extern const Stage saha;

#endif

