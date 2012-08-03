/*
 * SEKO
 *
 * Copyright 2012 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 *
 * Game logic
 */
#include "game.h"
#include "system.h"
#include "gl.h"
#include "skeleton.h"
#include "stage.h"
#include "sound.h"
#include "zombie.h"
#include "effects.h"
#include <stdexcept>
#include <SDL.h>

extern GLFont vera;

const Stage *stage;
const Level *level;
int score;
int score_verify;
int secret;
GLdouble modelMatrix[16];
GLdouble projMatrix[16];
int msg;
double msg_visible;
bool quit_game;

namespace {

enum {
	MOVE_LEFT_HAND,
	MOVE_RIGHT_HAND,
	MOVE_LEFT_FOOT,
	MOVE_RIGHT_FOOT,
	MOVE_HEAD,
	MOVE_JUMP,
	MOVE_TURN_LEFT,
	MOVE_TURN_RIGHT,
	MOVE_FLIP,
	MAX_MOVE
};

struct Move {
	const char *picname;
	int score;
};

const Move moves[] = {
	{"lefthand.png", 10},
	{"righthand.png", 10},
	{"leftfoot.png", 20},
	{"rightfoot.png", 20},
	{"head.png", 20},
	{"jump.png", 50},
	{"turnleft.png", 50},
	{"turnright.png", 50},
	{"flip.png", 100},
	{NULL, 0}
};

struct Hit {
	int move;
	double t;
};

const vec3 zero(0, 0, 0);
const Color black(0, 0, 0);
const Color white(1, 1, 1);

Joint *selected_joint = NULL;
Zombie *selected_zombie = NULL;
Animator<vec3> camera(zero, zero);

double beat_t;
int pattern_pos;
double camera_x = 30, camera_y = 30, camera_zoom = 20;
double rotation;
double last_turn;
double flip;
double last_pitch;
vec3 mouse_pos;
vec3 rotate_pos(0, 0, 0);
Skeleton player;
const Section *section = NULL;
bool moving;
bool rotating;
bool rotating_camera;
Model stagemodel;
bool standing;
bool jumping;
double interval;
double last_move;
std::list<Hit> hits;

const Pattern pat_begin = {
	Color(0.5, 0.5, 0.5),
	"Get ready...",
	{
		-1
	}
};

const Pattern pat_lefthand = {
	Color(1, 0.3, 0),
	"Get right hand moving",
	{
		MOVE_LEFT_HAND,
		-1
	}
};

const Pattern pat_rotate = {
	Color(0.3, 1, 0),
	"Rotate around!",
	{
		MOVE_TURN_LEFT,
		-1
	}
};

const Pattern pat_wave = {
	Color(0.1, 0.4, 1),
	"Wave your hands!",
	{
		MOVE_LEFT_HAND,
		MOVE_RIGHT_HAND,
		-1
	}
};

const Pattern pat_feet = {
	Color(1, 0.8, 0),
	"Get those feet moving!",
	{
		MOVE_LEFT_FOOT,
		MOVE_RIGHT_FOOT,
		-1
	}
};

const Pattern pat_twist = {
	Color(0, 1, 0.8),
	"Twist left and right",
	{
		MOVE_TURN_LEFT,
		MOVE_TURN_RIGHT,
		-1
	}
};

const Pattern pat_flip = {
	Color(1, 0, 0.8),
	"Do 360 flips over your head!",
	{
		MOVE_FLIP,
		-1
	}
};

const Pattern pat_head = {
	Color(1, 1, 0.8),
	"Jump around",
	{
		MOVE_HEAD,
		MOVE_JUMP,
		-1
	}
};

const Pattern pat_end = {
	Color(0, 0, 0),
	"END",
	{
		-1
	}
};

const Section boltbot[] = {
	{0, 125, &pat_begin},
	{8, 125, &pat_lefthand},
	{30, 125, &pat_rotate}, /* intermission */
	{45, 125, &pat_wave},
	{60, 125, &pat_lefthand},
	{76, 125, &pat_head}, /* intermission */
	{100, 125, &pat_end},
};

const Section pollute[] = {
	{0, 125, &pat_begin},
	{15, 125, &pat_wave},
	{32, 125, &pat_feet},
	{46, 125, &pat_lefthand},
	{61, 125, &pat_rotate}, /* electronic */
	{91, 125, &pat_wave},
	{125, 125, &pat_feet},
	{153, 125, &pat_rotate}, /* ping pong */
	{184, 125, &pat_head}, /* buzzer */
	{210, 125, &pat_end},
};

const Section sorvipop[] = {
	{0, 140, &pat_begin},
	{14, 70, &pat_rotate}, /* worm */
	{27, 140, &pat_twist}, /* screaming */
	{41, 140, &pat_wave},
	{68, 140, &pat_flip},
	{82, 140, &pat_head}, /* xx */
	{96, 140, &pat_wave}, /* back */
	{109, 140, &pat_feet},
	{123, 140, &pat_twist},
	{137, 140, &pat_wave}, /* outro */
	{165, 140, &pat_feet}, /* vocal */
	{170, 140, &pat_end},
};

const Section luxsaberpop[] = {
	{0, 140, &pat_begin},
	{14, 70, &pat_rotate},
	{27, 140, &pat_lefthand},
	{41, 140, &pat_wave},
	{68, 140, &pat_twist},
	{89, 140, &pat_flip},
	{96, 140, &pat_wave},
	{109, 140, &pat_feet},
	{123, 140, &pat_flip},
	{137, 140, &pat_twist},
	{165, 140, &pat_rotate}, /* slow part */
	{170, 140, &pat_end},
};

void add_score(int i)
{
	score = std::max(score + i, 0);
	score_verify = score * secret;
}

Joint *get_selected_joint()
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

	double nearest_d = 1;
	Joint *nearest = NULL;
	FOR_EACH(std::vector<Joint>, joint, player.joints) {
		if (joint->width <= 0)
			continue;
		double x = dot(joint->pos - nearp, d) / dot(d, d);
		x = std::max(x, 0.0);
		double dist = length(joint->pos - (nearp + d * x)) - joint->width;
		if (dist < nearest_d) {
			mouse_pos = nearp + d * x;
			nearest_d = dist;
			nearest = &*joint;
		}
	}
	return nearest;
}

void got_move(int move, int mult = 1)
{
	static Sound notehits[2], wooa;
	if (notehits[0].data.empty()) {
		load_sound(&notehits[0], "notehit1.ogg");
		load_sound(&notehits[1], "notehit2.ogg");
		load_sound(&wooa, "wooa.ogg");
	}

	debug("got move %s\n", moves[move].picname);

	if (move == MOVE_FLIP) {
		play_sound(&wooa);
	}

	if (level == NULL) {
		add_score(moves[move].score);
		return;
	}

	if (move == section->pattern->pattern[pattern_pos]) {
		double dt = get_music_time() - last_move;
		interval += (dt - interval) * 0.2;
		if (fabs(interval - 60 / section->bpm) < 0.1) {
			msg = EXCELLENT;
			msg_visible = 1.0;
			mult += 2;
		} else if (fabs(interval - 60 / section->bpm * 2) < 0.2) {
			msg = GOOD;
			msg_visible = 1.0;
			mult++;
		}
		play_sound(&notehits[rand() & 1]);
		Hit hit;
		hit.move = move;
		hit.t = get_music_time();
		hits.push_back(hit);
		last_move = get_music_time();
		add_score(moves[move].score * mult * mult);
		pattern_pos++;
		if (section->pattern->pattern[pattern_pos] < 0)
			pattern_pos = 0;
	} else {
		add_score(-moves[move].score);
		msg = TRYAGAIN;
		msg_visible = 1.0;
	}
}

void simulate()
{
	static Sound voih[2];
	if (voih[0].data.empty()) {
		load_sound(&voih[0], "voih1.ogg");
		load_sound(&voih[1], "voih2.ogg");
	}

	if (level != NULL) {
	const Section *next = section + 1;
	if (get_music_time() >= next->t) {
		section = next;
		beat_t = section->t;
		pattern_pos = 0;
		if (section->pattern == &pat_end) {
			quit_game = true;
		}
	}

	while (get_music_time() >= beat_t + 60 / section->bpm) {
		beat_t += 60 / section->bpm;
	}

	FOR_EACH_SAFE(std::list<Hit>, hit, hits) {
		if (get_music_time() - hit->t > 30) {
			hits.erase(hit);
		}
	}
	}

	double full_dt = get_dt();

	if (msg_visible > 0) {
		if (msg == GOOD) {
			for (int i = 0; i < MAX_JOINT; ++i) {
				add_smoke(player.joints[i].pos,
					Color(1, 0.3, 0, 0.1), 10, msg_visible * full_dt);
			}
		} else if (msg == EXCELLENT) {
			for (int i = 0; i < MAX_JOINT; ++i) {
				add_smoke(player.joints[i].pos,
					Color(0.3, 0, 1, 0.1), 10, msg_visible * full_dt);
			}
		}
		msg_visible -= full_dt;
	}
	camera.set_target(player.joints[ASS].pos);
	camera.update(full_dt, 100);

	move_smoke(full_dt);
	move_fog(full_dt);

	if (rotating_camera) {
		vec2 p(screen->w - 150, screen->h - 150);
		int x, y;
		SDL_GetMouseState(&x, &y);

		camera_x += (x - p.x) * full_dt;
		camera_y += (y - p.y) * full_dt;
	}

	vec3 mouse_vel(0, 0, 0);
	if (selected_joint != NULL) {
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

		double xx = dot(selected_joint->pos - nearp, d) / dot(d, d);
		vec3 pos = nearp + d * xx;
		mouse_vel = (pos - mouse_pos) * (1 / full_dt);
		if (dot(mouse_vel, mouse_vel) > 100*100) {
			mouse_vel = normalize(mouse_vel) * 100;
		}
		mouse_pos = pos;
	}

	const double STEP = 0.0005;

	while (full_dt > 0) {
		double dt = std::min(full_dt, STEP);

		if (selected_joint != NULL) {
			vec3 d = mouse_pos - selected_joint->pos;
			vec3 accel = d * 200;
			selected_joint->accel += accel;

			/* Dampening */
			accel = (mouse_vel - selected_joint->vel) * 10;
			selected_joint->accel += accel;

			int num = selected_joint - &player.joints[0];
			if (dot(selected_joint->vel, selected_joint->vel) > 500) {
				if (!moving) {
					switch (num) {
					case LEFT_HAND:
					case LEFT_ELBOW:
						got_move(MOVE_LEFT_HAND);
						break;
					case RIGHT_HAND:
					case RIGHT_ELBOW:
						got_move(MOVE_RIGHT_HAND);
						break;
					case LEFT_FOOT:
					case LEFT_KNEE:
						got_move(MOVE_LEFT_FOOT);
						break;
					case RIGHT_FOOT:
					case RIGHT_KNEE:
						got_move(MOVE_RIGHT_FOOT);
						break;
					case NECK:
					case HEAD:
						got_move(MOVE_HEAD);
						break;
					default:
						standing = false;
					}
				}
				moving = true;
			} else if (dot(selected_joint->vel, selected_joint->vel) < 100) {
				moving = false;
			}
		}
		if (selected_zombie != NULL) {
			throw_zombie(selected_zombie);
		}

		if (player.joints[ASS].vel.y > 10) {
			if (!jumping) {
				got_move(MOVE_JUMP);
			}
			jumping = true;
		} else if (player.joints[ASS].vel.y < 5) {
			jumping = false;
		}

		vec3 d = player.joints[RIGHT_SHOULDER].pos -
			 player.joints[LEFT_SHOULDER].pos;
		double a = atan2(d.x, d.z);
		double turn = a - last_turn;
		last_turn = a;
		if (turn > M_PI) {
			turn -= M_PI*2;
		}
		if (turn < -M_PI) {
			turn += M_PI*2;
		}
		turn /= dt;
		if (fabs(rotation) > M_PI/4 && (player.joints[BACK].pos.y >
				       player.joints[ASS].pos.y + 3)) {
			if (!rotating) {
				rotate_pos = player.joints[ASS].pos;
			}
			rotating = true;
		}
		if (fabs(turn) < 0.1) {
			if (rotation > M_PI/2) {
				got_move(MOVE_TURN_LEFT,
					fabs(rotation) / (M_PI/2));
			} else if (rotation < -M_PI/2) {
				got_move(MOVE_TURN_RIGHT,
					fabs(rotation) / (M_PI/2));
			}
			rotating = false;
			rotation = 0;
		}
		rotation += turn*dt;

		d = player.joints[BACK].pos -
		    player.joints[ASS].pos;
		a = atan2(d.x + d.z, d.y);
		double pitch = a - last_pitch;
		last_pitch = a;
		if (pitch > M_PI) {
			pitch -= M_PI*2;
		}
		if (pitch < -M_PI) {
			pitch += M_PI*2;
		}
		pitch /= dt;
		if (fabs(flip) > M_PI*2) {
			got_move(MOVE_FLIP);
			flip = 0;
		}
		if (fabs(pitch) < 0.1) {
			flip = 0;
		}
		flip += pitch*dt;

		double floor = -1e30;
		for (int i = HEAD+1; i < MAX_JOINT; ++i) {
			const Joint *joint = &player.joints[i];
			if (joint->on_ground) {
				floor = std::max(joint->pos.y, floor);
			}
		}

		if (rotating || (standing && floor > -20)) {
			double strength = 200;
			vec3 p = player.joints[ASS].pos;
			p.y = floor + 7;
			if (rotating) {
				strength = 500;
				p = rotate_pos;
			}

			/* keep the body in above floor */
			vec3 accel = (p - player.joints[ASS].pos) * strength;
			player.joints[ASS].accel += accel;
			p.y += 5;
			accel = (p - player.joints[BACK].pos) * strength;
			player.joints[BACK].accel += accel;

			player.joints[LEFT_FOOT].accel.y -= 20;
			player.joints[RIGHT_FOOT].accel.y -= 20;
		}

		FOR_EACH(std::vector<Joint>, joint, player.joints) {
			joint->on_ground = false;
		}
		for (size_t i = 0; i < stage->num_blocks; ++i) {
			double hit = apply_block(&player, &stage->blocks[i], dt);
			if (uniform() < dt*hit*0.01) {
				play_sound(&voih[rand() & 1]);
			}
		}

		move_zombies(player.joints[ASS].pos, dt);

		vec3 p = player.joints[ASS].pos;
		if (fabs(p.x) > stage->size || fabs(p.z) > stage->size) {
			reset_skeleton(&player, stage->origo);
		}

		animate(&player, dt);
		full_dt -= STEP;
	}
}

void draw_scroller()
{
	static GLuint cursor = INVALID_TEXTURE, bar;
	static GLuint move_pictures[MAX_MOVE];
	if (cursor == INVALID_TEXTURE) {
		cursor = load_png("cursor.png");
		bar = load_png("bar.png");
		for (int i = 0; i < MAX_MOVE; ++i) {
			move_pictures[i] = load_png(moves[i].picname);
		}
	}

	/* Sections */
	for (int i = 0; level->sections[i].pattern != &pat_end; ++i) {
		const Section *sec = &level->sections[i];
		const Section *next = &level->sections[i + 1];

		double x = (sec->t - get_music_time()) * 100;
		if (x > screen->w/2)
			break;

		glPushMatrix();
		glTranslatef(x + screen->w/2, screen->h - 80, 0);

		{
			GLState state;
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			state.enable(GL_BLEND);
			state.enable(GL_TEXTURE_2D);

			Color c = sec->pattern->color;
			c.a = 1 - (get_music_time() - beat_t);
			glColor(c);

			double len = next->t - sec->t;

			glBindTexture(GL_TEXTURE_2D, bar);
			glBegin(GL_QUADS);
			glTexCoord2f(0, 0);
			glVertex2f(0, 0);
			glTexCoord2f(0, 1);
			glVertex2f(0, 50);
			glTexCoord2f(len / (60 / sec->bpm), 1);
			glVertex2f(len * 100, 50);
			glTexCoord2f(len / (60 / sec->bpm), 0);
			glVertex2f(len * 100, 0);
			glEnd();
		}
		glColor(black);
		vera.draw_text(vec2(6, 31), sec->pattern->descr);
		glColor(white);
		vera.draw_text(vec2(5, 30), sec->pattern->descr);

		glPopMatrix();
	}

	/* Hits */
	{
		GLState state;
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		state.enable(GL_BLEND);
		state.enable(GL_TEXTURE_2D);

		FOR_EACH(std::list<Hit>, hit, hits) {
			double x = (hit->t - get_music_time()) * 100;

			glColor(white);
			vec2 pos(x + screen->w/2 - 32, screen->h - 64);
			glBindTexture(GL_TEXTURE_2D, move_pictures[hit->move]);
			draw_quad(pos, vec2(64, 64));
		}
	}

	/* Cursor */
	{
		GLState state;
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		state.enable(GL_BLEND);
		state.enable(GL_TEXTURE_2D);

		Color color(1, 1, 1, 1 - (get_music_time() - beat_t));
		glColor(color);

		vec2 pos(screen->w/2 - 64, screen->h - 128);
		glBindTexture(GL_TEXTURE_2D, cursor);
		draw_quad(pos, vec2(128, 128));
	}

	const Color inactive(1, 1, 1, 0.3);

	GLState state;
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	state.enable(GL_BLEND);
	state.enable(GL_TEXTURE_2D);

	const Pattern *pattern = section->pattern;
	for (int i = 0; pattern->pattern[i] >= 0; ++i) {
		if (pattern_pos == i) {
			glColor(white);
		} else {
			glColor(inactive);
		}
		vec2 pos(screen->w - 150, 20 + i * 130);
		glBindTexture(GL_TEXTURE_2D,
			      move_pictures[pattern->pattern[i]]);
		draw_quad(pos, vec2(128, 128));
	}
}

void draw_scene()
{
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45, screen->w * 1.0 / screen->h, 0.01, 1000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gluLookAt(0, 0, camera_zoom, 0, 0, 0, 0, 1, 0);
	glRotatef(camera_y, 1, 0, 0);
	glRotatef(camera_x, 0, 1, 0);

	vec3 p = camera.value();
	glTranslatef(-p.x, -p.y, -p.z);

	glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
	glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);

	const float light1[] = {0.6, 0.4, 0.7, 0};
	glLightfv(GL_LIGHT0, GL_POSITION, light1);

	const float light2[] = {-0.2, 0.8, -0.6, 0};
	glLightfv(GL_LIGHT1, GL_POSITION, light2);

	float light[] = {1, 0.7, 0.4, 1};
	if (level != NULL) {
		light[0] = 1.0 - (get_music_time() - beat_t);
	}
	glLightfv(GL_LIGHT1, GL_DIFFUSE, light);

	glFogf(GL_FOG_DENSITY, 0.01);

	{
		GLState state;
		state.enable(GL_DEPTH_TEST);
		state.enable(GL_LIGHTING);
		state.enable(GL_LIGHT0);
		state.enable(GL_LIGHT1);
		state.enable(GL_FOG);

		draw_zombies(player.joints[ASS].pos);
		stagemodel.draw(true);

		state.enable(GL_NORMALIZE);
		calc_posture(&player);
		draw(&player);
	}

	if (debug_enabled) {
		GLState state;
		state.enable(GL_DEPTH_TEST);
		state.enable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(1, 1, 1, 0.5);
		for (size_t i = 0; i < stage->num_blocks; ++i) {
			draw_block(stage->blocks[i].walls, stage->blocks[i].num_walls);
		}
	}

	draw_smoke();
	draw_fog();
}

void draw()
{
	static GLuint msgpics[MAX_MESSAGE];
	static GLuint camera_tex = INVALID_TEXTURE, standup;
	if (camera_tex == INVALID_TEXTURE) {
		msgpics[GOOD] = load_png("good.png");
		msgpics[TRYAGAIN] = load_png("tryagain.png");
		msgpics[EXCELLENT] = load_png("excellent.png");
		camera_tex = load_png("camera.png");
		standup = load_png("standup.png");
	}

	if (GLEW_VERSION_3_0) {
		begin_bloom();
		draw_scene();
		end_bloom();
	}

	draw_scene();

	if (GLEW_VERSION_3_0) {
		draw_bloom();
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, screen->w, screen->h, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glColor(white);
	draw_number(vec2(10, 10), score, 8);

	if (level != NULL) {
		draw_scroller();
	}

	{
		GLState state;
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		state.enable(GL_BLEND);
		state.enable(GL_TEXTURE_2D);

		glColor(white);
		glBindTexture(GL_TEXTURE_2D, camera_tex);
		draw_quad(vec2(screen->w - 150 - 64, screen->h - 150 - 64),
			  vec2(128, 128));

		if (!standing) {
			glBindTexture(GL_TEXTURE_2D, standup);
			draw_quad(vec2(10, screen->h - 150), vec2(128, 64));
		}
	}

	if (msg_visible > 0) {
		GLState state;
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		state.enable(GL_BLEND);
		state.enable(GL_TEXTURE_2D);

		Color color(1, 1, 1, msg_visible);
		glColor(color);
		vec2 pos(screen->w/2 - 128, 10);
		glBindTexture(GL_TEXTURE_2D, msgpics[msg]);
		draw_quad(pos, vec2(256, 128));
	}

	if (rotating) {
		glColor(white);
		int degrees = fabs(rotation) * 360 / (M_PI * 2);
		vera.draw_text(vec2(screen->w/2 - 64, 100), strf("%d\xB0", degrees));
	}
}

}

bool game()
{
	quit_game = false;
	clear_smoke();
	hits.clear();
	zombies.clear();
	camera.set_value(zero, zero);
	rotation = 0;
	last_turn = 0;
	flip = 0;
	last_pitch = 0;
	pattern_pos = 0;
	beat_t = 0;
	msg_visible = 0;
	moving = false;
	rotating = false;
	rotating_camera = false;
	jumping = false;
	standing = true;
	selected_joint = NULL;
	selected_zombie = NULL;
	interval = 0;
	last_move = 0;
	if (level == NULL) {
		/* Free play */
		stage = (uniform() < 0.5) ? &saha : &graveyard;
	} else {
		stage = level->stage;
		for (int i = 0; i < level->num_zombies; ++i) {
			double a = i * (M_PI * 2) / level->num_zombies;
			Zombie zombie;
			zombie.anim = 0;
			zombie.pos = stage->origo +
				vec3(cosf(a) * 100, ZOMBIE_SIZE, sin(a) * 100);
			zombie.vel = zero;
			zombie.accel = zero;
			zombies.push_back(zombie);
		}
	}
	stagemodel.load(stage->model, stage->scale);
	if (player.joints.empty()) {
		load_skeleton(&player, "human.obj", stage->origo);
	} else {
		reset_skeleton(&player, stage->origo);
	}
	if (level != NULL) {
		section = level->sections;
		play_music(level->music);
	}
	while (!quit_game) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_QUIT:
				throw std::runtime_error("window closed");
			case SDL_KEYDOWN:
				if (e.key.keysym.sym == SDLK_ESCAPE) {
					quit_game = true;
				} else if (e.key.keysym.sym == SDLK_SPACE) {
					standing = !standing;
				} else if (debug_enabled && e.key.keysym.sym == SDLK_F5) {
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				} else if (debug_enabled && e.key.keysym.sym == SDLK_F6) {
					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				} else if (debug_enabled && e.key.keysym.sym == SDLK_F7) {
					return true;
				}
				break;
			case SDL_VIDEORESIZE:
				resize(&e);
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (e.button.button == 1) {
					vec2 mouse(e.button.x, e.button.y);
					vec2 p(screen->w - 150 - 64, screen->h - 150 - 64);
					vec2 p2(10, screen->h - 150);
					if (mouse > p && mouse < p + vec2(128, 128)) {
						rotating_camera = true;
					} else if (mouse > p2 && mouse < p2 + vec2(128, 64)) {
						standing = true;
					} else {
						selected_joint = get_selected_joint();
						if (selected_joint == NULL) {
							selected_zombie = get_selected_zombie();
						}
					}
				} else if (e.button.button == SDL_BUTTON_WHEELUP) {
					camera_zoom = std::max(camera_zoom - 2, 1.0);
				} else if (e.button.button == SDL_BUTTON_WHEELDOWN) {
					camera_zoom = std::min(camera_zoom + 2, 100.0);
				}
				break;
			case SDL_MOUSEMOTION:
				if (e.motion.state & 4) {
					camera_x += e.motion.xrel * 0.1;
					camera_y += e.motion.yrel * 0.1;
				}
				break;
			case SDL_MOUSEBUTTONUP:
				if (e.button.button == 1) {
					rotating_camera = false;
					selected_joint = NULL;
					selected_zombie = NULL;
				}
				break;
			}
		}
		simulate();
		draw();

		SDL_GL_SwapBuffers();
		SDL_Delay(5);
		check_gl_errors();
	}
	return level == NULL || (section->pattern == &pat_end);
}

const Level levels[] = {
	{"1/4: Graveyard", "You lost your job due to recent layoffs.\n"
	"To blow off the steam, you are dancing on the grave of your old boss.",
	boltbot, "boltbot.ogg", &graveyard, 0},

	{"2/4: Ex work place", "That wasn't enough! It's time to visit the old\n"
	"work place.",
	pollute, "pollute.ogg", &saha, 1},

	{"3/4: Back to graveyard", "140 beats per minute means more adrenaline",
	sorvipop, "sorvipop.ogg", &graveyard, 2},

	{"4/4: Final showdown", "Difficulty++",
	luxsaberpop, "luxsaberpop.ogg", &saha, 2},

	{NULL, NULL, NULL, NULL, NULL}
};

