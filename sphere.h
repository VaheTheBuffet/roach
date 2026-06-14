#ifndef SPHERE_H
#define SPHERE_H

#include "vec.h"
#include "ray.h"

struct sphere {
	point3 p;
	float r;

	sphere() {}
    sphere(const point &c, const float &r) : p(p), r(r) {}

	vec3 intersection(ray &r) {

		return vec3();
	}
};

#define SPHERE_H	
