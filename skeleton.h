#ifndef __skeleton_h
#define __skeleton_h

#include "gl.h"

struct Joint {
	vec3 pos, vel, accel;
	vec3 max_vel;
	double width;
	bool on_ground;
};

struct JointDef {
	vec3 pos;
};

struct BoneDef {
	int a, b;
	double width, strength;
	int prio;
};

struct Bone;

struct Locked {
	Bone *other;
	int direction;
};

struct Bone {
	bool disabled;
	Joint *a, *b;
	double width, strength;
	int prio;
	vec3 d;
	double len;
	vec3 up, perp;
	std::list<Locked> locked;
};

/* Must match jointdefs */
enum {
	LEFT_HAND,
	RIGHT_HAND,
	LEFT_ELBOW,
	RIGHT_ELBOW,
	LEFT_SHOULDER,
	RIGHT_SHOULDER,
	NECK,
	BACK,
	HEAD,

	/* lower body */
	LEFT_FOOT,
	RIGHT_FOOT,
	LEFT_KNEE,
	RIGHT_KNEE,
	LEFT_HIP,
	RIGHT_HIP,
	ASS,

	MAX_JOINT
};

struct Attach {
	double x, u, v, w;
	double nx, nu, nv;
	Bone *bone;
};

struct SkelVertex {
	std::vector<Attach> attach;
	vec3 pos, normal;
};

struct Skeleton {
	std::vector<Joint> joints;
	std::vector<SkelVertex> vertices;
	std::list<Bone> bones;
	group_map_t groups;
};

struct Block;

void reset_skeleton(Skeleton *skeleton, const vec3 &origo);
void load_skeleton(Skeleton *skeleton, const char *fname, const vec3 &origo);
void animate(Skeleton *skeleton, double dt);
double apply_block(Skeleton *skeleton, const Block *block, double dt);
void calc_posture(Skeleton *skeleton);
void draw(const Skeleton *skeleton);

#endif
