#ifndef RAY_TRACER_H
#define RAY_TRACER_H

#include "vec.h"
#include "ctx.h"
#include "sphere.h"

#define STACK_SIZE 64
#define MAX_DEPTH 4

constexpr color3 background_color = color3::from_rgb(0x87CEEB);

void draw_scene_with_refraction(Ctx &ctx);

struct scene_t {
    sphere spheres[10] = {};
    int n_spheres;
    point3 light_sources[10] = {};
    int n_light_sources;
};

void draw_frame(const scene_t &state, Ctx &ctx);

#endif
