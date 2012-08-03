/*
 * SEKO
 *
 * Copyright 2012 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 *
 * Ragdoll physics engine
 */
#include "skeleton.h"
#include "utils.h"
#include "stage.h"
#include "effects.h"
#include <stdexcept>
#include <fstream>
#include <sstream>

namespace {

const vec3 zero(0, 0, 0);

const JointDef jointdefs[] = {
	{vec3(-7, 2.5, 1)}, /* HAND */
	{vec3(7, 2.5, 1)},
	{vec3(-4, 3.8, 0)}, /* ELBOW */
	{vec3(4, 3.8, 0)},
	{vec3(-1.8, 4, 0)}, /* SHOULDER */
	{vec3(1.8, 4, 0)},
	{vec3(0, 5.5, 0)}, /* NECK */
	{vec3(0, 4.5, 0)}, /* BACK */
	{vec3(0, 7, 0)}, /* HEAD */

	/* lower body */
	{vec3(-1.5, -6.5, 0)}, /* FOOT */
	{vec3(1.5, -6.5, 0)},
	{vec3(-1, -3, 0)}, /* KNEE */
	{vec3(1, -3, 0)},
	{vec3(-1, -0.5, 0)}, /* HIP */
	{vec3(1, -0.5, 0)},
	{vec3(0, 0, 0)}, /* ASS */
};

const BoneDef bonedefs[] = {
	/* Hands */
	{LEFT_HAND, LEFT_ELBOW, 0.3, 1, 0},
	{LEFT_ELBOW, LEFT_SHOULDER, 0.5, 1, 0},
	{RIGHT_HAND, RIGHT_ELBOW, 0.3, 1, 0},
	{RIGHT_ELBOW, RIGHT_SHOULDER, 0.5, 1, 0},

	/* Torso */
	{ASS, BACK, 0.4, 1, 1},
	{RIGHT_SHOULDER, RIGHT_HIP, 0.4, 1, 1},
	{LEFT_SHOULDER, LEFT_HIP, 0.4, 1, 1},
	{LEFT_SHOULDER, BACK, 0.6, 1, 1},
	{RIGHT_SHOULDER, BACK, 0.6, 1, 1},
	{LEFT_SHOULDER, RIGHT_SHOULDER, 0, 1, 0},

	/* Head */
	{NECK, HEAD, 1, 1, 0},
	{BACK, NECK, 0.5, 1, 0},

	/* Feet */
	{LEFT_FOOT, LEFT_KNEE, 0.5, 1, 0},
	{RIGHT_FOOT, RIGHT_KNEE, 0.5, 1, 0},
	{LEFT_KNEE, LEFT_HIP, 1, 1, 0},
	{RIGHT_KNEE, RIGHT_HIP, 1, 1, 0},

	/* Cross support */
	{LEFT_HIP, BACK, 0, 1, 1},
	{RIGHT_HIP, BACK, 0, 1, 1},
	{ASS, RIGHT_SHOULDER, 0, 1, 1},
	{ASS, LEFT_SHOULDER, 0, 1, 1},

	{LEFT_HIP, RIGHT_HIP, 0, 1, 0},
	{LEFT_HIP, ASS, 0, 1, 0},
	{RIGHT_HIP, ASS, 0, 1, 0},

	/* Keep head up */
	{HEAD, LEFT_HIP, 0, 0.3, 0},
	{HEAD, RIGHT_HIP, 0, 0.3, 0},

	/* Keep arms and legs straight */
	{LEFT_FOOT, LEFT_SHOULDER, 0, 0.2, 0},
	{RIGHT_FOOT, RIGHT_SHOULDER, 0, 0.2, 0},
	{LEFT_HAND, RIGHT_SHOULDER, 0, 0.2, 0},
	{RIGHT_HAND, LEFT_SHOULDER, 0, 0.2, 0},
};

}

void reset_skeleton(Skeleton *skeleton, const vec3 &origo)
{
	for (size_t i = 0; i < ARRAY_SIZE(jointdefs); ++i) {
		skeleton->joints[i].pos = jointdefs[i].pos + origo;
		skeleton->joints[i].vel = zero;
		skeleton->joints[i].accel = zero;
	}

	FOR_EACH(std::list<Bone>, bone, skeleton->bones) {
		bone->d = bone->b->pos - bone->a->pos;
		bone->perp = normalize(cross(bone->d, vec3(0, 0, 1)));
		bone->up = normalize(cross(bone->perp, bone->d));
	}
}

void load_skeleton(Skeleton *skeleton, const char *fname, const vec3 &origo)
{
	debug("loading skeleton %s\n", fname);
	skeleton->joints.clear();
	skeleton->bones.clear();
	skeleton->groups.clear();
	skeleton->vertices.clear();

	Mesh mesh;
	load_mesh(&mesh, fname, 1.0, origo);

	for (size_t i = 0; i < ARRAY_SIZE(jointdefs); ++i) {
		Joint joint;
		joint.pos = jointdefs[i].pos + origo;
		joint.vel = zero;
		joint.accel = zero;
		joint.max_vel = zero;
		joint.width = 0;
		skeleton->joints.push_back(joint);
	}

	for (size_t i = 0; i < ARRAY_SIZE(bonedefs); ++i) {
		Bone bone;
		bone.width = bonedefs[i].width;
		bone.strength = bonedefs[i].strength;
		bone.prio = bonedefs[i].prio;
		bone.a = &skeleton->joints[bonedefs[i].a];
		bone.b = &skeleton->joints[bonedefs[i].b];
		bone.d = bone.b->pos - bone.a->pos;
		bone.len = length(bone.d);
		bone.perp = normalize(cross(bone.d, vec3(0, 0, 1)));
		bone.up = normalize(cross(bone.perp, bone.d));
		skeleton->bones.push_back(bone);

		bone.a->width = std::max(bone.a->width, bone.width);
		bone.b->width = std::max(bone.b->width, bone.width);
	}

	FOR_EACH(std::list<Bone>, bone, skeleton->bones) {
		Bone *nearest = NULL;
		double nearest_d = 100;
		FOR_EACH(std::list<Bone>, other, skeleton->bones) {
			if (bone == other || other->strength < 0.5) {
				continue;
			}
			if (bone->a == other->a ||
			    bone->b == other->a ||
			    bone->a == other->b ||
			    bone->b == other->b) {
				double d = fabs(dot(bone->d, other->d)) /
					   (bone->len * other->len);
				if (d < 0.6) {
					Locked locked;
					locked.other = &*other;
					vec3 v = cross(bone->d, other->d);
					if (dot(v, bone->up) > 0) {
						locked.direction = 1;
					} else {
						locked.direction = -1;
					}
					bone->locked.push_back(locked);
				} else if (d < nearest_d) {
					nearest_d = d;
					nearest = &*other;
				}
			}
		}
		if (bone->locked.empty() && nearest != NULL) {
			Locked locked;
			locked.other = nearest;
			locked.direction = 0;
			bone->locked.push_back(locked);
		}
	}

	std::vector<vec3> normals(mesh.vertices.size());

	/* Map normaps to vertices */
	FOR_EACH_CONST(group_map_t, iter, mesh.groups) {
		const Group *group = &iter->second;
		FOR_EACH_CONST(std::vector<Face>, f, group->faces) {
			for (int i = 0; i < 3; ++i) {
				normals[f->vert[i]] =
					mesh.normals[f->norm[i]];
			}
		}
	}

	skeleton->groups = mesh.groups;
	skeleton->vertices.resize(mesh.vertices.size());

	for (size_t i = 0; i < mesh.vertices.size(); ++i) {
		vec3 p = mesh.vertices[i];
		Bone *nearest = NULL;
		double nearest_d = 1e30;
		FOR_EACH(std::list<Bone>, bone, skeleton->bones) {
			if (bone->width <= 0)
				continue;
			/* calculate nearest position on the bone */
			double x = dot(p - bone->a->pos, bone->d) /
				   dot(bone->d, bone->d);
			x = std::max(std::min(x, 1.0), 0.0);
			vec3 d = p - (bone->a->pos + bone->d * x);
			double dist = length(d);
			if (dist < bone->width) {
				Attach attach;
				attach.w = 1.5 - (dist / bone->width);
				attach.bone = &*bone;
				skeleton->vertices[i].attach.push_back(attach);
			} else if (dist < nearest_d) {
				nearest = &*bone;
				nearest_d = dist;
			}
		}
		if (skeleton->vertices[i].attach.empty() && nearest != NULL) {
			Attach attach;
			attach.w = 1.0;
			attach.bone = nearest;
			skeleton->vertices[i].attach.push_back(attach);
		}
		vec3 n = normals[i];
		FOR_EACH(std::vector<Attach>, attach, skeleton->vertices[i].attach) {
			vec3 a = attach->bone->a->pos;
			attach->x = dot(p - a, attach->bone->d) /
				    dot(attach->bone->d, attach->bone->d);
			attach->u = dot(p - a, attach->bone->up);
			attach->v = dot(p - a, attach->bone->perp);
			attach->nx = dot(n, attach->bone->d) /
				     dot(attach->bone->d, attach->bone->d);
			attach->nu = dot(n, attach->bone->up);
			attach->nv = dot(n, attach->bone->perp);
		}
	}
	debug("done. %zd bones\n", skeleton->bones.size());
}

void animate(Skeleton *skeleton, double dt)
{
	FOR_EACH(std::vector<Joint>, joint, skeleton->joints) {
		if (joint->width <= 0)
			continue;
		FOR_EACH(std::list<Bone>, bone, skeleton->bones) {
			if (&*joint == bone->a || &*joint == bone->b ||
			    bone->width <= 0)
				continue;
			/* calculate nearest position on the bone */
			double x = dot(joint->pos - bone->a->pos, bone->d) /
				   dot(bone->d, bone->d);
			x = std::max(std::min(x, 1.0), 0.0);
			vec3 d = (bone->a->pos + bone->d * x) - joint->pos;
			double l = length(d);
			double min_l = bone->width + joint->width/2;
			if (l < min_l) {
				vec3 bonevel = bone->a->vel * (1 - x) +
					       bone->b->vel * x;

				/* Friction */
				vec3 accel = (bonevel - joint->vel) * 5;
				joint->accel += accel;
				bone->a->accel -= accel * std::min(2 - x * 2, 1.0);
				bone->b->accel -= accel * std::min(x * 2, 1.0);

				/* Bounce */
				if (l < 1e-10) {
					l = 1;
					d.x = uniform();
				}
				accel = d * ((l - min_l) * 500 / l);
				joint->accel += accel;
				bone->a->accel -= accel * std::min(2 - x * 2, 1.0);
				bone->b->accel -= accel * std::min(x * 2, 1.0);
			}
		}
	}

	FOR_EACH(std::list<Bone>, bone, skeleton->bones) {
		bone->d = bone->b->pos - bone->a->pos;

		/* Spring */
		double l = length(bone->d);
		double strength = 2000 * bone->strength;
		vec3 accel = bone->d * ((l - bone->len) * strength / l);
		bone->a->accel += accel;
		bone->b->accel -= accel;

		/* dampening */
		double x = dot(bone->b->vel - bone->a->vel, bone->d);
		accel = bone->d * (x * 10 / dot(bone->d, bone->d));
		bone->a->accel += accel;
		bone->b->accel -= accel;

		x = dot(bone->b->vel - bone->a->vel, bone->up);
		accel = bone->up * (x * 2);
		bone->a->accel += accel;
		bone->b->accel -= accel;

		x = dot(bone->b->vel - bone->a->vel, bone->perp);
		accel = bone->perp * (x * 2);
		bone->a->accel += accel;
		bone->b->accel -= accel;

		/* accumulate rotation from others */
		vec3 rotation(0, 0, 0);
		FOR_EACH(std::list<Locked>, locked, bone->locked) {
			if (locked->direction) {
				vec3 v = normalize(cross(bone->d, locked->other->d));
				rotation += (v - bone->up) *
					(locked->direction * locked->other->strength);
			} else {
				rotation += (locked->other->up - bone->up) *
					    locked->other->strength;
			}
		}
		bone->up += rotation * 5 * dt;

		/* Fix the bone */
		bone->perp = normalize(cross(bone->d, bone->up));
		bone->up = normalize(cross(bone->perp, bone->d));
	}

	FOR_EACH(std::vector<Joint>, joint, skeleton->joints) {
		/* Gravity */
		joint->accel.y -= 40;
		/* Air friction */
		joint->accel -= joint->vel;

		joint->vel += joint->accel * dt;
		if (dot(joint->vel, joint->vel) < 1e-6) {
			/* stop idle joints */
			joint->vel = zero;
		} else if (dot(joint->vel, joint->vel) > 100*100) {
			joint->vel = normalize(joint->vel) * 100;
		}
		joint->pos += joint->vel * dt;
		joint->accel = zero;
	}
}

double apply_block(Skeleton *skeleton, const Block *block, double dt)
{
	double hit = 0;
	FOR_EACH(std::vector<Joint>, joint, skeleton->joints) {
		vec3 nearest = joint->pos;
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
		vec3 d = joint->pos - nearest;
		double l = length(d);
		if (l < 1e-10) {
			/* Inside wall */
			assert(inside != NULL);
			l = dot(joint->pos, inside->normal) - inside->d;
			d = inside->normal;
		} else {
			d = d * (1 / l);
		}
		double min_l = joint->width;
		if (l < min_l) {
			if (inside != NULL && inside->normal.y > 0.5) {
				joint->on_ground = true;
			}
			add_smoke(nearest, Color(0.4, 0.4, 0.4, 0.3), 5,
				dt * dot(joint->vel, joint->vel) * 0.1, 3);

			hit = std::max(hit, dot(joint->vel, joint->vel));

			/* Friction */
			joint->accel -= joint->vel * 10;

			/* Bounce */
			vec3 accel = d * ((min_l - l) * 1000);
			joint->accel += accel;
		}
	}
	return hit;
}

void calc_posture(Skeleton *skeleton)
{
	FOR_EACH(std::vector<SkelVertex>, v, skeleton->vertices) {
		vec3 pos(0, 0, 0);
		vec3 normal(0, 0, 0);
		double sum = 0;
		FOR_EACH_CONST(std::vector<Attach>, attach, v->attach) {
			const Bone *bone = attach->bone;
			pos += (bone->a->pos +
				bone->d * attach->x +
				bone->up * attach->u +
				bone->perp * attach->v) *
				attach->w;
			normal += (bone->d * attach->nx +
				bone->up * attach->nu +
				bone->perp * attach->nv) *
				attach->w;
			sum += attach->w;
		}
		if (sum > 0) {
			sum = 1.0 / sum;
			v->pos = pos * sum;
			v->normal = normal * sum;
		}
	}
}

void draw(const Skeleton *skeleton)
{
	GLClientState state;

	state.enable(GL_VERTEX_ARRAY);
	state.enable(GL_NORMAL_ARRAY);

	FOR_EACH_CONST(group_map_t, iter, skeleton->groups) {
		const Group *group = &iter->second;
		std::vector<GLVertex> buf(group->faces.size() * 3);
		for (size_t j = 0; j < group->faces.size(); ++j) {
			const Face *f = &group->faces[j];
			for (int i = 0; i < 3; ++i) {
				GLVertex gv;
				vec3 p = skeleton->vertices[f->vert[i]].pos;
				vec3 n = skeleton->vertices[f->vert[i]].normal;
				gv.pos[0] = p.x;
				gv.pos[1] = p.y;
				gv.pos[2] = p.z;
				gv.normal[0] = n.x;
				gv.normal[1] = n.y;
				gv.normal[2] = n.z;
				buf[j * 3 + i] = gv;
			}
		}

		glVertexPointer(3, GL_FLOAT, sizeof(GLVertex), buf[0].pos);
		glNormalPointer(GL_FLOAT, sizeof(GLVertex), buf[0].normal);

		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE,
			     &group->diffuse.r);
		glDrawArrays(GL_TRIANGLES, 0, group->faces.size() * 3);
	}

	if (!debug_enabled)
		return;

	glBegin(GL_LINES);
	FOR_EACH_CONST(std::list<Bone>, bone, skeleton->bones) {
		for (int i = 0; i < 6; ++i) {
			if (i == 0)
				glColor4f(1, 1, 0, 1);
			else
				glColor4f(0.3, 0, 0, 1);
			double a = i * (M_PI * 2 / 6);
			vec3 off = bone->up * (cosf(a) * bone->width) +
				bone->perp * (sinf(a) * bone->width);
			glVertex(bone->a->pos + off);
			glVertex(bone->b->pos + off);
			glColor4f(0.3, 0, 0, 1);
			a = (i + 1) * (M_PI * 2 / 6);
			vec3 off2 = bone->up * (cosf(a) * bone->width) +
				bone->perp * (sinf(a) * bone->width);
			glVertex(bone->a->pos + off);
			glVertex(bone->a->pos + off2);
			glVertex(bone->b->pos + off);
			glVertex(bone->b->pos + off2);
		}
	}
	glEnd();
}
