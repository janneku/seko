#include "vec.h"
#include <list>

const double ZOMBIE_SIZE = 4;

struct Zombie {
	vec3 pos, vel, accel;
	double anim;
	double fly_anim;
	bool on_ground;
};

extern std::list<Zombie> zombies;

Zombie *get_selected_zombie();
void move_zombies(const vec3 &player, double dt);
void throw_zombie(Zombie *zombie);
void draw_zombies(const vec3 &player);

