#ifndef RAY_TRACER_H
#define RAY_TRACER_H

#include "vec.h"
#include "ctx.h"
#include "sphere.h"

#define MAX_DEPTH 4
#define STACK_SIZE 64
#define N_LANES 8

constexpr color3 background_color = color3::from_rgb(0x87CEEB);

struct scene_t {
    sphere spheres[10];
    int n_spheres;
    point3 light_sources[10];
    int n_light_sources;
};

void rt_draw_frame(scene_t &scene);

#endif
