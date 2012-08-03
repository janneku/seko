/*
 * SEKO
 *
 * Copyright 2012 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 *
 * Effects (yay!)
 */
#include "effects.h"
#include "gl.h"
#include <SDL.h>

extern SDL_Surface *screen;

namespace {

const size_t FOG_COUNT = 500;
vec3 fog[FOG_COUNT];
FBO bloom1, bloom2, bloom_out;

const Color white(1, 1, 1);

struct Particle {
	Color color;
	double time, duration, size;
	vec3 pos, vel;
};

std::list<Particle> smoke;

}

void draw_number(vec2 pos, int v, int len)
{
	static GLuint digits = INVALID_TEXTURE;
	if (digits == INVALID_TEXTURE) {
		digits = load_png("digits.png");
	}

	assert(v >= 0);

	GLState state;
	state.enable(GL_BLEND);
	state.enable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, digits);

	glBegin(GL_QUADS);
	pos.x += DIGIT_WIDTH * len;
	for (int i = 0; i < len; ++i) {
		pos.x -= DIGIT_WIDTH;

		float x = (v % 10) * DIGIT_WIDTH / 512;
		glTexCoord2f(x, 0);
		glVertex3f(pos.x, pos.y, 0);
		glTexCoord2f(x, 1.0);
		glVertex3f(pos.x, pos.y + DIGIT_HEIGHT, 0);
		glTexCoord2f(x + DIGIT_WIDTH / 512, 1.0);
		glVertex3f(pos.x + DIGIT_WIDTH, pos.y + DIGIT_HEIGHT, 0);
		glTexCoord2f(x + DIGIT_WIDTH / 512, 0);
		glVertex3f(pos.x + DIGIT_WIDTH, pos.y, 0);

		v /= 10;
	}
	glEnd();
}

void move_smoke(double full_dt)
{
	const double MAX_STEP = 0.02;

	while (full_dt > 0) {
		double dt = std::min(full_dt, MAX_STEP);

		FOR_EACH_SAFE(std::list<Particle>, p, smoke) {
			vec3 accel = p->vel * -0.1 + vec3(0, 1, 0);
			if (uniform() < dt) {
				vec3 random = vec3(uniform(), uniform(), uniform()) -
				      vec3(0.5, 0.5, 0.5);
				accel += random;
			}
			p->pos += p->vel * dt;
			p->vel += accel * dt;
			p->time += dt;
			if (p->time > p->duration) {
				smoke.erase(p);
			}
		}
		full_dt -= MAX_STEP;
	}
}

void draw_smoke()
{
	static GLuint smoke_tex = INVALID_TEXTURE;
	if (smoke_tex == INVALID_TEXTURE) {
		smoke_tex = load_png("smoke.png");
	}

	GLdouble mdl[16];
	glGetDoublev(GL_MODELVIEW_MATRIX, mdl);

	vec3 camera;
	camera.x = -(mdl[0] * mdl[12] + mdl[1] * mdl[13] + mdl[2] * mdl[14]);
	camera.y = -(mdl[4] * mdl[12] + mdl[5] * mdl[13] + mdl[6] * mdl[14]);
	camera.z = -(mdl[8] * mdl[12] + mdl[9] * mdl[13] + mdl[10] * mdl[14]);

	GLState state;
	state.enable(GL_BLEND);
	state.enable(GL_DEPTH_TEST);
	state.enable(GL_TEXTURE_2D);
	glDepthMask(GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, smoke_tex);
	glBegin(GL_QUADS);

	FOR_EACH_CONST (std::list<Particle>, p, smoke) {
		vec3 n = normalize(camera - p->pos);
		vec3 a = normalize(cross(n, vec3(0, 1, 0)))
			 * (p->time / p->duration * p->size);
		vec3 b = cross(a, n);

		Color c = p->color;
		c.a *= 1 - p->time / p->duration;
		glColor(c);
		glTexCoord2f(0, 0);
		glVertex(p->pos - a - b);
		glTexCoord2f(1, 0);
		glVertex(p->pos - a + b);
		glTexCoord2f(1, 1);
		glVertex(p->pos + a + b);
		glTexCoord2f(0, 1);
		glVertex(p->pos + a - b);
	}

	glEnd();
	glDepthMask(GL_TRUE);
}

void move_fog(double dt)
{
	for (size_t i = 0; i < FOG_COUNT; ++i) {
		fog[i].x += dt;
		if (fog[i].x > 50) {
			fog[i].x -= 100;
		}
	}
}

void draw_fog()
{
	static GLuint smoke_tex = INVALID_TEXTURE;
	if (smoke_tex == INVALID_TEXTURE) {
		smoke_tex = load_png("smoke.png");
		for (size_t i = 0; i < FOG_COUNT; ++i) {
			fog[i] = vec3(uniform() * 100 - 50, uniform() * 20,
				      uniform() * 100 - 50);
		}
	}

	GLdouble mdl[16];
	glGetDoublev(GL_MODELVIEW_MATRIX, mdl);

	vec3 camera;
	camera.x = -(mdl[0] * mdl[12] + mdl[1] * mdl[13] + mdl[2] * mdl[14]);
	camera.y = -(mdl[4] * mdl[12] + mdl[5] * mdl[13] + mdl[6] * mdl[14]);
	camera.z = -(mdl[8] * mdl[12] + mdl[9] * mdl[13] + mdl[10] * mdl[14]);

	GLState state;
	state.enable(GL_BLEND);
	state.enable(GL_DEPTH_TEST);
	state.enable(GL_TEXTURE_2D);
	glDepthMask(GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, smoke_tex);
	glBegin(GL_QUADS);

	Color c(0, 0.01, 0.03, 0.1);
	glColor(c);
	for (size_t i = 0; i < FOG_COUNT; ++i) {
		vec3 n = normalize(camera - fog[i]);
		vec3 a = normalize(cross(n, vec3(0, 1, 0))) * 5;
		vec3 b = cross(a, n);

		glTexCoord2f(0, 0);
		glVertex(fog[i] - a - b);
		glTexCoord2f(1, 0);
		glVertex(fog[i] - a + b);
		glTexCoord2f(1, 1);
		glVertex(fog[i] + a + b);
		glTexCoord2f(0, 1);
		glVertex(fog[i] + a - b);
	}

	glEnd();
	glDepthMask(GL_TRUE);
}

void add_smoke(const vec3 &pos, const Color &color, double duration,
		double count, double size)
{
	if (count < 1) {
		if (uniform() < count)
			count += 1;
	}

	for (int i = 0; i < int(count); ++i) {
		vec3 random = vec3(uniform(), uniform(), uniform()) -
			      vec3(0.5, 0.5, 0.5);
		Particle p;
		p.time = 0.1;
		p.pos = pos + random;
		p.vel = random;
		p.duration = duration;
		p.color = color;
		p.size = size;
		p.color.r += uniform() * 0.2 - 0.1;
		p.color.g += uniform() * 0.2 - 0.1;
		p.color.b += uniform() * 0.2 - 0.1;
		p.color.a += uniform() * 0.4 - 0.2;
		smoke.push_back(p);
	}
}

void clear_smoke()
{
	smoke.clear();
}

void begin_bloom()
{
	static int bloom_w = 0, bloom_h = 0;
	if (bloom_w != screen->w || bloom_h != screen->h) {
		create_fbo(&bloom1, screen->w/2, screen->h/2, false);
		create_fbo(&bloom2, screen->w/2, screen->h/2, false);
		create_fbo(&bloom_out, screen->w/2, screen->h/2, true);
		bloom_w = screen->w;
		bloom_h = screen->h;
	}

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bloom1.fbo);
	glViewport(0, 0, screen->w/2, screen->h/2);
}

const char *simple_vs =
"varying vec2 tc;\
void main(void)\
{\
	gl_Position = ftransform();\
	tc = gl_MultiTexCoord0.xy;\
}";

const char *bloom1_source =
"#extension GL_ARB_texture_rectangle : enable\n\
uniform sampler2DRect tex;\
varying vec2 tc;\
void main(void)\
{\
	vec4 sum = vec4(0.0);\
	int i;\
	float s;\
	for (i = -9; i < 10; i++) {\
		s = 1.0 - abs(float(i) * 0.1);\
		sum = max(sum, texture2DRect(tex, tc + vec2(float(i), 0.0)) * vec4(s));\
	}\
	gl_FragColor = sum;\
}";

const char *bloom2_source =
"#extension GL_ARB_texture_rectangle : enable\n\
uniform sampler2DRect tex;\
varying vec2 tc;\
void main(void)\
{\
	vec4 sum = vec4(0.0);\
	int i;\
	float s;\
	for (i = -9; i < 10; i++) {\
		s = 1.0 - abs(float(i) * 0.1);\
		sum = max(sum, texture2DRect(tex, tc + vec2(0.0, float(i))) * vec4(s));\
	}\
	gl_FragColor = sum;\
}";

/* Will mess matrixes and viewport */
void end_bloom()
{
	static GLuint bloom1_program, bloom2_program;
	if (bloom1_program == 0) {
		bloom1_program = load_program(simple_vs, bloom1_source);
		bloom2_program = load_program(simple_vs, bloom2_source);
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 1, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bloom2.fbo);
	glUseProgram(bloom1_program);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, bloom1.texture);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3f(0, 0, 0);
	glTexCoord2f(0, screen->h/2);
	glVertex3f(0, 1, 0);
	glTexCoord2f(screen->w/2, screen->h/2);
	glVertex3f(1, 1, 0);
	glTexCoord2f(screen->w/2, 0);
	glVertex3f(1, 0, 0);
	glEnd();

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bloom_out.fbo);
	glUseProgram(bloom2_program);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, bloom2.texture);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3f(0, 0, 0);
	glTexCoord2f(0, screen->h/2);
	glVertex3f(0, 1, 0);
	glTexCoord2f(screen->w/2, screen->h/2);
	glVertex3f(1, 1, 0);
	glTexCoord2f(screen->w/2, 0);
	glVertex3f(1, 0, 0);
	glEnd();

	glUseProgram(0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glViewport(0, 0, screen->w, screen->h);
}

void draw_bloom()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 1, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	Color c(1, 1, 1, 0.3);
	glColor(c);

	GLState state;
	state.enable(GL_BLEND);
	state.enable(GL_TEXTURE_RECTANGLE_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, bloom_out.texture);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3f(0, 1, 0);
	glTexCoord2f(screen->w/2, 0);
	glVertex3f(1, 1, 0);
	glTexCoord2f(screen->w/2, screen->h/2);
	glVertex3f(1, 0, 0);
	glTexCoord2f(0, screen->h/2);
	glVertex3f(0, 0, 0);
	glEnd();
}

