#include "vec.h"

vec3 cross(const vec3 &a, const vec3 &b)
{
	return vec3(a.y*b.z - a.z*b.y,
		    a.z*b.x - a.x*b.z,
		    a.x*b.y - a.y*b.x);
}
