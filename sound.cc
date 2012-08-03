/*
 * Sound system
 */
#include "sound.h"
#include "utils.h"
#include <vorbis/vorbisfile.h>
#include <SDL.h>
#include <list>
#include <stdexcept>

namespace {

double music_pos = 0;
Uint32 music_time = 0;

struct Playing {
	const Sound *sound;
	size_t pos;
	double volume;
};

const int SAMPLERATE = 44100;

std::list<Playing> playing;
OggVorbis_File music;
FILE *music_file = NULL;

void fill_audio(void *userdata, Uint8 *_buf, int _len)
{
	size_t len = _len / 2;
	int16_t *buf = (int16_t *) _buf;

	for (size_t i = 0; i < len;) {
		int bitstream = 0;
		long got = ov_read(&music, (char *) &buf[i],
				   (len - i) * 2, 0, 2, 1, &bitstream);
		if (got == OV_HOLE) {
			continue;
		}
		if (got <= 0) {
			ov_time_seek(&music, 0);
			continue;
		}
		got /= 2;
		vorbis_info *vi = ov_info(&music, -1);
		assert(vi->channels == 2);
		assert(vi->rate == SAMPLERATE);
		i += got;
	}

	FOR_EACH_SAFE(std::list<Playing>, p, playing) {
		size_t chunk = std::min(p->sound->data.size() - p->pos, len);
		const int16_t *data = p->sound->data.data();
		if (p->volume != 1) {
			int s = p->volume * 256.0;
			for (size_t i = 0; i < chunk; ++i) {
				int output = buf[i] + data[p->pos++] * s / 256;
				buf[i] = std::max(std::min(output, 0x7FFF), -0x8000);
			}
		} else {
			for (size_t i = 0; i < chunk; ++i) {
				int output = buf[i] + data[p->pos++];
				buf[i] = std::max(std::min(output, 0x7FFF), -0x8000);
			}
		}
		if (p->pos >= p->sound->data.size()) {
			playing.erase(p);
		}
	}

	music_pos += len * 0.5 / SAMPLERATE;
	music_time = SDL_GetTicks();
}

}

double get_music_time()
{
	return music_pos + (SDL_GetTicks() - music_time) * 0.001;
}

void play_music(const char *fname)
{
	SDL_LockAudio();
	if (music_file != NULL) {
		ov_clear(&music);
	}
	music_file = fopen(fname, "rb");
	if (music_file == NULL) {
		throw std::runtime_error(strf("Can not open %s", fname));
	}
	if (ov_open(music_file, &music, NULL, 0)) {
		throw std::runtime_error(strf("Invalid vorbis file: %s",
					      fname));
	}
	music_pos = 0;
	music_time = SDL_GetTicks();
	SDL_UnlockAudio();
	SDL_PauseAudio(0);
}

void play_sound(const Sound *sound, double volume)
{
	Playing p;
	p.sound = sound;
	p.pos = 0;
	p.volume = volume;
	SDL_LockAudio();
	playing.push_back(p);
	SDL_UnlockAudio();
}

void load_sound(Sound *sound, const char *fname)
{
	debug("loading %s\n", fname);
	FILE *f = fopen(fname, "rb");
	if (f == NULL) {
		throw std::runtime_error(strf("Can not open %s", fname));
	}
	OggVorbis_File vf;
	if (ov_open(f, &vf, NULL, 0)) {
		throw std::runtime_error(strf("Invalid vorbis file: %s",
					      fname));
	}
	size_t pos = 0;
	while (1) {
		sound->data.resize(pos + 4096);

		int bitstream = 0;
		long got = ov_read(&vf, (char *) &sound->data[pos], 4096 * 2,
				   0, 2, 1, &bitstream);
		if (got <= 0) {
			sound->data.resize(pos);
			break;
		}
		got /= 2;
		sound->data.resize(pos + got);
		vorbis_info *vi = ov_info(&vf, -1);
		if (vi->channels != 2 || vi->rate != SAMPLERATE) {
			throw std::runtime_error(strf("Invalid sound format: %s",
						      fname));
		}
		pos += got;
	}
	debug("%zd samples\n", pos);
	ov_clear(&vf);
}

void init_sound()
{
	SDL_AudioSpec spec;
	SDL_AudioSpec obtained;
	spec.freq = SAMPLERATE;
	spec.format = AUDIO_S16SYS;
	spec.channels = 2;
	spec.samples = SAMPLERATE / 40;
	spec.callback = fill_audio;
	if (SDL_OpenAudio(&spec, &obtained)) {
		throw std::runtime_error(strf("Can not intialize audio: %s",
					      SDL_GetError()));
	}
}

