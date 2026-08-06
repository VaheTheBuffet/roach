#ifndef SPHERE_H
#define SPHERE_H

#include <cmath>
#include <random>
#include <cstring>
#include <raylib.h>

#include "vec.h"
#include "ray.h"
#include "material.h"

struct quadratic_solution {
    float low;
    float high;
};

struct sphere {
    point3 center;
    float r;
    material mat;

    sphere() = default;
    ~sphere() = default;

    sphere(const point3 &center, const float &r, material mat) : center(center), r(r), mat(mat) {}

    quadratic_solution intersection_ray(const ray &ray) const {
        // find the scale factor of the ray to intersect this sphere, 0 if no intersection
        // high indicates whether the high root of the quadratic should be taken
        point3 relative_center = this->center - ray.orig;
        float a = ray.dir.length_squared();
        float h = dot(ray.dir, relative_center);
        float c = relative_center.length_squared() - (this->r * this->r);
        float disc_2 = h*h - a*c;
        float disc = std::sqrt(std::abs(disc_2));

        float r1 = (h - disc) / a;
        float r2 = (h + disc) / a;

        std::uint32_t r1_bits;
        std::uint32_t r2_bits;
        std::uint32_t mask = std::signbit(disc_2) << 31;

        if (r1 < 0.0f) {
            if (r2 < 1e-2) {
                return {-1.0f, -1.0f};
            }
        }

        std::memcpy(&r1_bits, &r1, 4);
        std::memcpy(&r2_bits, &r2, 4);

        r1_bits |= mask;
        r2_bits |= mask;

        std::memcpy(&r1, &r1_bits, 4);
        std::memcpy(&r2, &r2_bits, 4);

        return quadratic_solution{r1, r2};
    }
};
#endif
