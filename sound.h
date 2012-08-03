#ifndef __sound_h
#define __sound_h

#include <stdint.h>
#include <vector>

struct Sound {
	std::vector<int16_t> data;
};

double get_music_time();
void play_music(const char *fname);
void play_sound(const Sound *sound, double volume=1);
void load_sound(Sound *sound, const char *fname);
void init_sound();

#endif
