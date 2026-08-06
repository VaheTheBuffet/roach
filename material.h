#ifndef MATERIAL_H
#define MATERIAL_H

#include "vec.h"

enum class material_type {
    lambertian,
    reflective,
    refractive,

    LENGTH,
};

struct material {
    float refractive_index;
    float reflectance;
    color3 albedo;

    material_type ty;

    static material lambertian(const color3 &albedo);
    static material reflective();
    static material refractive(const float &ri);
};

inline material material::lambertian(const color3 &albedo) {
    return material {
        .albedo = albedo,
        .ty = material_type::lambertian,
    };
}

inline material material::reflective() {
    return material {
        .ty = material_type::reflective,
    };
}

inline material material::refractive(const float &ri) {
    return material {
        .refractive_index = ri,
        .reflectance = (ri - 1.0f) * (ri - 1.0f) / ((ri + 1.0f) * (ri + 1.0f)),
        .ty = material_type::refractive,
    };
}

#endif
