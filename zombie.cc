/*
 * SEKO
 *
 * Copyright 2012 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 *
 * Zombies
 */
#include "zombie.h"
#include "system.h"
#include "game.h"
#include "stage.h"
#include "sound.h"
#include "gl.h"

std::list<Zombie> zombies;

namespace {

const vec3 zero(0, 0, 0);

void apply_block(Zombie *zombie, const Block *block)
{
	vec3 nearest = zombie->pos;
	double nearest_d = -1e30;
	const Plane *inside = NULL;
	for (size_t i = 0; i < block->num_walls; ++i) {
		const Plane *wall = &block->walls[i];
		double d = dot(nearest, wall->normal) - wall->d;
		if (d > 0) {
			nearest -= wall->normal * d;
		}
		if (d > nearest_d) {
			inside = &*wall;
			nearest_d = d;
		}
	}
	vec3 d = zombie->pos - nearest;
	double l = length(d);
	if (l < 1e-10) {
		/* Inside wall */
		assert(inside != NULL);
		l = dot(zombie->pos, inside->normal) - inside->d;
		d = inside->normal;
	} else {
		d = d * (1 / l);
	}
	if (l < ZOMBIE_SIZE) {
		if (inside != NULL && inside->normal.y > 0.5) {
			zombie->on_ground = true;
		}
		/* Friction */
		zombie->accel -= zombie->vel * 10;

		/* Bounce */
		vec3 accel = d * ((ZOMBIE_SIZE - l) * 1000);
		zombie->accel += accel;
	}
}

}

Zombie *get_selected_zombie()
{
	int x, y;
	SDL_GetMouseState(&x, &y);
	int viewport[4] = {0, 0, screen->w, screen->h};

	vec3 nearp, farp;
	gluUnProject(x, screen->h - y, 0.01,
		    modelMatrix, projMatrix, viewport,
		    &nearp.x, &nearp.y, &nearp.z);
	gluUnProject(x, screen->h - y, 1,
		    modelMatrix, projMatrix, viewport,
		    &farp.x, &farp.y, &farp.z);
	vec3 d = farp - nearp;

	double nearest_d = ZOMBIE_SIZE;
	Zombie *nearest = NULL;
	FOR_EACH(std::list<Zombie>, zombie, zombies) {
		double x = dot(zombie->pos - nearp, d) / dot(d, d);
		x = std::max(x, 0.0);
		double dist = length(zombie->pos - (nearp + d * x));
		if (dist < nearest_d) {
			nearest_d = dist;
			nearest = &*zombie;
		}
	}
	return nearest;
}

void move_zombies(const vec3 &player, double dt)
{
	static Sound zombiesound[2];
	if (zombiesound[0].data.empty()) {
		load_sound(&zombiesound[0], "zombie1.ogg");
		load_sound(&zombiesound[1], "zombie2.ogg");
	}

	FOR_EACH(std::list<Zombie>, a, zombies) {
		FOR_EACH(std::list<Zombie>, b, zombies) {
			if (a == b)
				break;

			vec3 d = b->pos - a->pos;
			double l = length(d);
			if (l < ZOMBIE_SIZE*2) {
				/* Bounce */
				vec3 accel = d * ((ZOMBIE_SIZE*2 - l) * 500 / l);
				a->accel -= accel;
				b->accel += accel;
			}
		}
	}


	FOR_EACH(std::list<Zombie>, zombie, zombies) {
		/* Gravity and friction */
		zombie->accel.y -= 20;
		zombie->accel -= zombie->vel;

		if (zombie->on_ground) {
			/* Walk towards player */
			vec3 d = player - zombie->pos;
			d.y = 0;
			if (msg_visible > 0 && msg != TRYAGAIN) {
				d = d * -1;
			}
			zombie->accel += d * (30 / length(d));
		}

		if (length(player - zombie->pos) < ZOMBIE_SIZE) {
			quit_game = true;
		}

		if (uniform() < dt * 0.1) {
			vec3 d = player - zombie->pos;
			play_sound(&zombiesound[rand() & 1],
				   std::min(1000 / dot(d, d), 5.0));
		}

		zombie->on_ground = false;
		for (size_t i = 0; i < stage->num_blocks; ++i) {
			apply_block(&*zombie, &stage->blocks[i]);
		}
		if (zombie->on_ground) {
			if (zombie->fly_anim > 0) {
				play_sound(&zombiesound[rand() & 1]);
			}
			zombie->fly_anim = 0;
		} else {
			zombie->fly_anim += length(zombie->vel) * dt;
		}
		zombie->vel += zombie->accel * dt;
		zombie->pos += zombie->vel * dt;
		if (zombie->on_ground) {
			zombie->anim += length(zombie->vel) * dt;
		}
		zombie->accel = zero;
	}
}

void throw_zombie(Zombie *zombie)
{
	int x, y;
	SDL_GetMouseState(&x, &y);
	int viewport[4] = {0, 0, screen->w, screen->h};

	vec3 nearp, farp;
	gluUnProject(x, screen->h - y, 0.01,
		    modelMatrix, projMatrix, viewport,
		    &nearp.x, &nearp.y, &nearp.z);
	gluUnProject(x, screen->h - y, 1,
		    modelMatrix, projMatrix, viewport,
		    &farp.x, &farp.y, &farp.z);
	vec3 d = farp - nearp;

	double xx = dot(zombie->pos - nearp, d) / dot(d, d);
	vec3 pos = nearp + d * xx;
	vec3 accel = (pos - zombie->pos) * 20;
	zombie->accel += accel;
}

void draw_zombies(const vec3 &player)
{
	static Model luxzombie;
	if (luxzombie.empty()) {
		luxzombie.load("luxzombie.obj", 2);
	}

	FOR_EACH_CONST(std::list<Zombie>, zombie, zombies) {
		glPushMatrix();
		glTranslatef(zombie->pos.x, zombie->pos.y, zombie->pos.z);
		vec3 d = player - zombie->pos;
		double a = atan2(d.x, d.z) + (sin(zombie->anim) * 0.4 - 0.2);
		glRotatef(a * 360 / (M_PI * 2), 0, 1, 0);
		if (zombie->fly_anim > 0) {
			glRotatef(zombie->fly_anim*2, 0, 0, 1);
			glRotatef(zombie->fly_anim, 0, 1, 0);
		}
		glTranslatef(0, -4, 0);
		luxzombie.draw(false);
		glPopMatrix();
	}
}
