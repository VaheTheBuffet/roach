#ifndef MATERIAL_H
#define MATERIAL_H

enum class material {
    diffuse,
    reflective,
    transparent,

    LENGTH,
};

struct material_state {
    float albedo;
    float fresnel_factor;
    float refractive_index;
};

#endif
