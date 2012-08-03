#ifndef __effects_h
#define __effects_h

#include "gl.h"

const double DIGIT_WIDTH = 40;
const double DIGIT_HEIGHT = 64;

void draw_number(vec2 pos, int v, int len);
void move_smoke(double dt);
void draw_smoke();
void add_smoke(const vec3 &pos, const Color &color, double duration,
	double count, double size=5);
void clear_smoke();
void draw_quad(double x, double y, double w, double h);
void move_fog(double dt);
void draw_fog();
void begin_bloom();
void end_bloom();
void draw_bloom();

#endif

