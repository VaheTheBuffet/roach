#ifndef RAY_H
#define RAY_H

#include "vec.h"

struct ray {
	point3 orig;
 	vec3 dir;

	ray() {}
	ray(point &p, vec3 &v) : orig(p), dir(v) {}

	point3 at(float t) {
		return this->orig + this->dir * t;
	}
}
#endif
