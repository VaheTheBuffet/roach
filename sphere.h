#ifndef SPHERE_H
#define SPHERE_H

#include <immintrin.h>

#include "ray.h"
#include "material.h"

struct quadratic_result {
    float low;
    float high;
    unsigned int nan: 1;
};

struct sphere {
    point3 center;
    float r;
    material mat;

    sphere() = default;
    sphere(const point3 &center, const float &r, material mat) : center(center), r(r), mat(mat) {}

    quadratic_result intersection_ray(const ray &ray) const {
        // computes both roots of the quadratic
        // nan solutions get converted to extarneous negative solutions by bit manipulation
        point3 relative_center = this->center - ray.orig;
        float a = ray.dir.length_squared();
        float h = dot(ray.dir, relative_center);
        float c = relative_center.length_squared() - (this->r * this->r);
        float disc_2 = h*h - a*c;
        float disc = std::sqrt(disc_2);

        float r1 = (h - disc) / a;
        float r2 = (h + disc) / a;

        return quadratic_result{r1, r2, std::signbit(disc_2)};
    }
};
#endif
