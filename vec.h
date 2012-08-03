#ifndef __vec_h
#define __vec_h

#include <math.h>

class vec3 {
public:
	double x, y, z;

	vec3() {}
	vec3(double i_x, double i_y, double i_z) :
		x(i_x), y(i_y), z(i_z)
	{
	}
};

class vec2 {
public:
	double x, y;

	vec2() {}
	vec2(double i_x, double i_y) :
		x(i_x), y(i_y)
	{
	}
};

struct Plane {
	vec3 normal;
	double d;
};

extern inline vec3 operator + (const vec3 &a, const vec3 &b)
{
	return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

extern inline vec3 operator - (const vec3 &a, const vec3 &b)
{
	return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

extern inline void operator += (vec3 &a, const vec3 &b)
{
	a = a + b;
}

extern inline void operator -= (vec3 &a, const vec3 &b)
{
	a = a - b;
}

extern inline vec3 operator * (const vec3 &v, double s)
{
	return vec3(v.x * s, v.y * s, v.z * s);
}

extern inline vec2 operator + (const vec2 &a, const vec2 &b)
{
	return vec2(a.x + b.x, a.y + b.y);
}

extern inline vec2 operator - (const vec2 &a, const vec2 &b)
{
	return vec2(a.x - b.x, a.y - b.y);
}

extern inline void operator += (vec2 &a, const vec2 &b)
{
	a = a + b;
}

extern inline void operator -= (vec2 &a, const vec2 &b)
{
	a = a - b;
}

extern inline vec2 operator * (const vec2 &v, double s)
{
	return vec2(v.x * s, v.y * s);
}

extern inline bool operator > (const vec2 &a, const vec2 &b)
{
	return a.x > b.x && a.y > b.y;
}

extern inline bool operator < (const vec2 &a, const vec2 &b)
{
	return a.x < b.x && a.y < b.y;
}

extern inline double dot(const vec3 &a, const vec3 &b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

extern inline double dot(const vec2 &a, const vec2 &b)
{
	return a.x*b.x + a.y*b.y;
}

extern inline double length(const vec3 &v)
{
	return sqrt(dot(v, v));
}

extern inline vec3 normalize(const vec3 &v)
{
	double l = length(v);
	if (l < 1e-10)
		return vec3(0, 0, 0);
	return v * (1 / l);
}

extern inline double length(const vec2 &v)
{
	return sqrt(dot(v, v));
}

extern inline vec2 normalize(const vec2 &v)
{
	double l = length(v);
	if (l < 1e-10)
		return vec2(0, 0);
	return v * (1 / l);
}

vec3 cross(const vec3 &a, const vec3 &b);

#endif
