#ifndef SPHERE_H
#define SPHERE_H

#include <cmath>
#include <random>
#include <raylib.h>

#include "vec.h"
#include "ray.h"
#include "material.h"

struct sphere {
    point3 center;
    color3 color;
    float r;
    material mat;

    sphere() = default;
    ~sphere() = default;

    sphere(const point3 &center, const float &r, const color3 &color, material mat) : center(center), r(r), color(color), mat(mat) {}

    float intersection_ray(const ray &ray, bool high = false) const {
        // find the scale factor of the ray to intersect this sphere, 0 if no intersection
        // high indicates whether the high root of the quadratic should be taken
        point3 relative_center = this->center - ray.orig;
        float a = ray.dir.length_squared();
        float b = -2.0f * dot(ray.dir, relative_center);
        float c = relative_center.length_squared() - (this->r * this->r);

        float disc_squared = b*b - 4.0f*a*c;

        if (disc_squared < 0.0f) {
            return 0;
        }

        return (-b + (high ? std::sqrt(disc_squared) : - std::sqrt(disc_squared))) / (2.0f * a);
    }
};
#endif
