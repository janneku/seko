/*
 * SEKO
 *
 * Copyright 2012 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 *
 * Main menu, highscores
 */
#include "menu.h"
#include "gl.h"
#include "stage.h"
#include "effects.h"
#include "skeleton.h"
#include "sound.h"
#include "game.h"
#include "system.h"
#include <SDL.h>
#include <stdexcept>
#include <sstream>
#include <curl/curl.h>
#include <curl/easy.h>
#include <openssl/sha.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* from main.cc */
extern GLFont vera;

#define HIGHSCORE_SERVER "http://localhost/"

namespace {

const char HIGHSCORE_SECRET[] = "";

const size_t MAX_HIGHSCORES = 5;

const Color white(1, 1, 1);

Skeleton dancer;

struct Highscore {
	int score;
	std::string name;
};

const double FLOOR = 0;

const Plane floor_wall[] = {
	{vec3(0, 1, 0), FLOOR}
};

const Block floor = {
	1,
	floor_wall,
};

std::list<Highscore> highscores;
bool enter_name;
std::string name_buffer;
std::string error_message;
bool show_credits;
bool show_help;

std::string urlencode(const std::string &s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); ++i) {
		if (isalnum(s[i]))
			out += s[i];
		else
			out += strf("%%%02X", (unsigned char) s[i]);
	}
	return out;
}

size_t write_func(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	std::string *str = (std::string *) userdata;
	str->append((char *) ptr, size * nmemb);
	return size * nmemb;
}

#ifdef __linux__
#define OS "Linux"
#endif

#ifdef _WIN32
#define OS "Windows"
#endif

void fetch_highscores()
{
	highscores.clear();
	error_message.clear();

	std::string url = HIGHSCORE_SERVER "/highscores.txt";

	char error[CURL_ERROR_SIZE];
	std::string reply;
	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "SEKO " OS);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error);
	int res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res) {
		error_message = error;
		return;
	}

	std::string line;
	std::istringstream is(reply);
	while (std::getline(is, line) && highscores.size() < MAX_HIGHSCORES) {
		std::istringstream parser(line);
		Highscore hs;
		parser >> hs.score;
		if (!parser) {
			continue;
		}
		hs.name = line.substr(parser.tellg());
		highscores.push_back(hs);
	}
}

void submit_highscore()
{
	error_message.clear();

	if (score_verify != score * secret) {
		throw std::runtime_error("Wow, why are you trying to crack this effectspy game?");
	}
	std::string signature =
		strf("%d%s%s", score, HIGHSCORE_SECRET, name_buffer.c_str());
	uint8_t digest[SHA_DIGEST_LENGTH];
	SHA1((const uint8_t *) signature.data(), signature.size(), digest);

	std::string verify((char *) digest, 4);
	std::string data = strf("name=%s&score=%d&verify=%s",
				urlencode(name_buffer).c_str(), score,
				urlencode(verify).c_str());

	char error[CURL_ERROR_SIZE];
	CURL *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "SEKO " OS);
	curl_easy_setopt(curl, CURLOPT_URL, HIGHSCORE_SERVER "/submit.php");
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.data());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
	int res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res) {
		error_message = error;
		return;
	}
	score = 0;
	fetch_highscores();
}

void draw_highscores()
{
	static GLuint logo = INVALID_TEXTURE, text;
	if (logo == INVALID_TEXTURE) {
		logo = load_png("logo.png");
		text = load_png("highscores.png");
	}

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, screen->w, screen->h, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	{
		GLState state;
		state.enable(GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		state.enable(GL_BLEND);

		glColor(white);
		vec2 pos(screen->w/2 - 256, 50);
		glBindTexture(GL_TEXTURE_2D, logo);
		draw_quad(pos, vec2(512, 256));

		pos = vec2(screen->w/2 - 256, screen->h/2 - 100);
		glBindTexture(GL_TEXTURE_2D, text);
		draw_quad(pos, vec2(512, 64));
	}

	Color active(1, 1, 0);

	if (!error_message.empty()) {
		glColor(white);
		vera.draw_text(vec2(100, screen->h/2), error_message);
	}

	int y = screen->h/2;
	bool enter_drawn = false;
	FOR_EACH_CONST(std::list<Highscore>, hs, highscores) {
		if (enter_name && score >= hs->score && !enter_drawn) {
			glColor(active);
			draw_number(vec2(screen->w/2 - DIGIT_WIDTH*8, y), score, 8);
			vera.draw_text(vec2(screen->w/2 + 10, y + 40), name_buffer + "_");
			y += DIGIT_HEIGHT + 10;
			enter_drawn = true;
		}
		glColor(white);
		draw_number(vec2(screen->w/2 - DIGIT_WIDTH*8, y), hs->score, 8);
		vera.draw_text(vec2(screen->w/2 + 10, y + 40), hs->name);
		y += DIGIT_HEIGHT + 10;
	}

	if (enter_name && !enter_drawn) {
		glColor(active);
		draw_number(vec2(screen->w/2 - DIGIT_WIDTH*8, y), score, 8);
		vera.draw_text(vec2(screen->w/2 + 10, y + 40), name_buffer + "_");
	}
}

void highscores_menu()
{
	static Sound awwyeah;
	if (awwyeah.data.empty()) {
		load_sound(&awwyeah, "awwyeah.ogg");
	}
	bool quit = false;
	enter_name = false;
	name_buffer.clear();
	error_message.clear();
	fetch_highscores();

	FOR_EACH(std::list<Highscore>, i, highscores) {
		if (score >= i->score) {
			enter_name = true;
			break;
		}
	}
	if (!highscores.empty() && score > 0 && highscores.size() < MAX_HIGHSCORES) {
		enter_name = true;
	}

	if (enter_name) {
		play_sound(&awwyeah);
		SDL_EnableUNICODE(1);
	}

	while (!quit) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_QUIT:
				throw std::runtime_error("window closed");
			case SDL_KEYDOWN:
				if (e.key.keysym.sym == SDLK_ESCAPE) {
					if (enter_name)
						enter_name = false;
					else
						quit = true;
				} else if (e.key.keysym.sym == SDLK_RETURN &&
					   enter_name) {
					enter_name = false;
					SDL_EnableUNICODE(0);
					submit_highscore();
				} else if (e.key.keysym.sym == SDLK_BACKSPACE &&
					   enter_name && !name_buffer.empty()) {
					name_buffer.resize(name_buffer.size() - 1);
				} else if (e.key.keysym.unicode >= ' ' &&
					   e.key.keysym.unicode < 256 &&
					   enter_name) {
					name_buffer +=
						char(e.key.keysym.unicode);
				}
				break;
			case SDL_VIDEORESIZE:
				resize(&e);
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (e.button.button == 1) {
					quit = true;
				}
				break;
			}
		}
		draw_highscores();

		SDL_GL_SwapBuffers();
		SDL_Delay(5);
		check_gl_errors();
	}
	SDL_EnableUNICODE(0);
}

void draw_intermission()
{
	static GLuint logo = INVALID_TEXTURE, score_tex, nextlevel;
	if (logo == INVALID_TEXTURE) {
		logo = load_png("logo.png");
		score_tex = load_png("score.png");
		nextlevel = load_png("nextlevel.png");
	}

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, screen->w, screen->h, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	{
		GLState state;
		state.enable(GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		state.enable(GL_BLEND);

		glColor(white);
		vec2 pos(screen->w/2 - 256, 50);
		glBindTexture(GL_TEXTURE_2D, logo);
		draw_quad(pos, vec2(512, 256));

		pos = vec2(screen->w/2 - 256, screen->h/2 - 64);
		glBindTexture(GL_TEXTURE_2D, score_tex);
		draw_quad(pos, vec2(512, 64));

		if (level->name != NULL) {
			pos = vec2(screen->w/2 - 256, screen->h/2 + 100);
			glBindTexture(GL_TEXTURE_2D, nextlevel);
			draw_quad(pos, vec2(512, 64));
		}
	}

	draw_number(vec2(screen->w/2 - DIGIT_WIDTH*4, screen->h/2), score, 8);

	if (level->name != NULL) {
		Color inactive(0.5, 0.8, 1);

		glColor(inactive);
		vera.draw_text(vec2(screen->w/2 - 300, screen->h/2 + 100 + 80),
				level->name);
		glColor(white);
		vera.draw_text(vec2(screen->w/2 - 300, screen->h/2 + 100 + 110),
				level->descr);
	}
}

void intermission_menu()
{
	bool quit = false;
	while (!quit) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_QUIT:
				throw std::runtime_error("window closed");
			case SDL_KEYDOWN:
				if (e.key.keysym.sym == SDLK_ESCAPE) {
					quit = true;
				}
				break;
			case SDL_VIDEORESIZE:
				resize(&e);
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (e.button.button == 1) {
					quit = true;
				}
				break;
			}
		}
		draw_intermission();

		SDL_GL_SwapBuffers();
		SDL_Delay(5);
		check_gl_errors();
	}
}

void simulate_menu()
{
	const double STEP = 0.001;

	double full_dt = get_dt();

	move_smoke(full_dt);

	add_smoke(vec3(uniform() * 10 - 5, 0, uniform() * 10 - 5),
		Color(0.5, 0, 0.2, 0.2), 10, full_dt * 20);
	add_smoke(vec3(0, 0, 0), white, 10, full_dt * 10, 0.1);

	while (full_dt > 0) {
		double dt = std::min(full_dt, STEP);

		double y = sin(SDL_GetTicks() * 0.003) * 5;
		vec3 accel = (vec3(-7, 10 + y, 0) - dancer.joints[LEFT_HAND].pos) * 100;
		dancer.joints[LEFT_HAND].accel += accel;
		accel = (vec3(7, 10 - y, 0) - dancer.joints[RIGHT_HAND].pos) * 100;
		dancer.joints[RIGHT_HAND].accel += accel;

		/* keep the body in above floor */
		accel = (vec3(0, FLOOR + 12, 0) - dancer.joints[BACK].pos) * 200;
		dancer.joints[BACK].accel += accel;
		accel = (vec3(0, FLOOR + 7, 0) - dancer.joints[ASS].pos) * 200;
		dancer.joints[ASS].accel += accel;

		dancer.joints[LEFT_FOOT].accel.y -= 20;
		dancer.joints[RIGHT_FOOT].accel.y -= 20;

		apply_block(&dancer, &floor, 0);
		animate(&dancer, dt);
		full_dt -= STEP;
	}
}

enum {
	MENU_PLAY,
	MENU_FREE_PLAY,
	MENU_HIGHSCORES,
	MENU_HELP,
	MENU_CREDITS,
	MENU_QUIT,
	MAX_MENU
};

const char *menu_picnames[] = {
	"menu_play.png",
	"menu_freeplay.png",
	"menu_highscores.png",
	"menu_help.png",
	"menu_credits.png",
	"menu_quit.png",
};

int menu_selected()
{
	int x, y;
	SDL_GetMouseState(&x, &y);
	vec2 mouse(x, y);

	vec2 pos(screen->w - 256 - 50, screen->h/2);
	if (mouse > pos && mouse < pos + vec2(256, 64 * MAX_MENU)) {
		return int(y - pos.y) / 64;
	}
	return -1;
}

void draw_scene()
{
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45, screen->w * 1.0 / screen->h, 0.01, 1000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gluLookAt(0, 10, 20, -5, 10, 0, 0, 1, 0);

	glRotatef(SDL_GetTicks() * 0.1, 0, 1, 0);

	const float light1[] = {0.6, 0.4, -0.7, 0};
	glLightfv(GL_LIGHT0, GL_POSITION, light1);

	const float light2[] = {0.7, 0.6, 0.4, 0};
	glLightfv(GL_LIGHT1, GL_POSITION, light2);

	const float light[] = {1, 0.3, 1, 1};
	glLightfv(GL_LIGHT1, GL_DIFFUSE, light);

	{
		GLState state;
		state.enable(GL_LIGHTING);
		state.enable(GL_LIGHT0);
		state.enable(GL_LIGHT1);
		state.enable(GL_DEPTH_TEST);
		calc_posture(&dancer);
		draw(&dancer);
	}
	draw_smoke();
}

void draw_menu()
{
	static GLuint logo = INVALID_TEXTURE, help;
	static GLuint menu_pics[MAX_MENU];
	if (logo == INVALID_TEXTURE) {
		logo = load_png("logo.png");
		help = load_png("help.png");
		for (int i = 0; i < MAX_MENU; ++i) {
			menu_pics[i] = load_png(menu_picnames[i]);
		}
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

	{
		GLState state;
		state.enable(GL_TEXTURE_2D);
		state.enable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		Color color(1, 1, 1);
		if (uniform() < 0.05)
			color.r = 0;
		glColor(color);
		vec2 pos(screen->w/2 - 256, 50);
		if (uniform() < 0.02)
			pos.x += 10;
		glBindTexture(GL_TEXTURE_2D, logo);
		draw_quad(pos, vec2(512, 256));
	}

	if (show_credits) {
		glColor(white);
		vera.draw_text(vec2(screen->w/2 - 300, screen->h/2 - 50),
			"SEKO\nfor Assembly 2012\n\n"
			"Pizzalaatikko is:\n\n"
			"Janne Kulmala <janne.t.kulmala@iki.fi> - Code\n"
			"Antti Rajamäki <amikaze@gmail.com> - Models\n\n"
			"Written in a real programming language - 4000 lines of C++");
		return;
	}
	if (show_help) {
		GLState state;
		state.enable(GL_TEXTURE_2D);
		state.enable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glColor(white);
		vec2 pos(screen->w/2 - 256, screen->h/2);
		glBindTexture(GL_TEXTURE_2D, help);
		draw_quad(pos, vec2(512, 256));
		vera.draw_text(vec2(screen->w/2 - 300, screen->h/2 + 256),
			"Move the dancer with mouse (or a finger) and follow the moves\n"
			"on the right hand side of the screen. You will get bonuses\n"
			"and scare the zombies by matching the beat.");
		return;
	}

	int sel = menu_selected();

	Color inactive(0.7, 0.7, 0.7);

	GLState state;
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	state.enable(GL_TEXTURE_2D);
	state.enable(GL_BLEND);

	for (int i = 0; i < MAX_MENU; ++i) {
		if (sel == i) {
			glColor(white);
		} else {
			glColor(inactive);
		}
		vec2 pos(screen->w - 256 - 50, screen->h/2 + i * 64);
		glBindTexture(GL_TEXTURE_2D, menu_pics[i]);
		draw_quad(pos, vec2(256, 64));
	}
}

}

void play()
{
	score = 0;
	secret = rand() & 0xffffff;
	level = levels;
	intermission_menu();
	while (level->name != NULL) {
		if (!game())
			break;
		level++;
		intermission_menu();
	}
	play_music("menu.ogg");
	highscores_menu();
}

void menu()
{
	bool quit = false;
	if (dancer.joints.empty()) {
		load_skeleton(&dancer, "human.obj", vec3(0, 7, 0));
	}
	show_credits = false;
	show_help = false;
	play_music("menu.ogg");
	clear_smoke();
	while (!quit) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_QUIT:
				throw std::runtime_error("window closed");
			case SDL_KEYDOWN:
				if (e.key.keysym.sym == SDLK_ESCAPE) {
					if (show_credits)
						show_credits = false;
					else if (show_help)
						show_help = false;
					else
						quit = true;
				}
				break;
			case SDL_VIDEORESIZE:
				resize(&e);
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (show_credits) {
					show_credits = false;
					break;
				}
				if (show_help) {
					show_help = false;
					break;
				}
				switch (menu_selected()) {
				case MENU_PLAY:
					play();
					play_music("menu.ogg");
					clear_smoke();
					break;
				case MENU_FREE_PLAY:
					level = NULL;
					score = 0;
					game();
					score = 0;
					clear_smoke();
					break;
				case MENU_HELP:
					show_help = true;
					break;
				case MENU_CREDITS:
					show_credits = true;
					break;
				case MENU_HIGHSCORES:
					highscores_menu();
					break;
				case MENU_QUIT:
					quit = true;
					break;
				default:
					break;
				}
				break;
			}
		}
		simulate_menu();
		draw_menu();

		SDL_GL_SwapBuffers();
		SDL_Delay(5);
		check_gl_errors();
	}
}

