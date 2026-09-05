#ifndef RAY_TRACER_H
#define RAY_TRACER_H

#include "vec.h"
#include "ctx.h"
#include "sphere.h"

constexpr color3 background_color = color3::from_rgb(0x87CEEB);

struct scene_t {
    sphere *spheres;
    int n_spheres;
    point3 *light_sources;
    int n_light_sources;
};

void rt_init(Ctx &ctx);
void rt_set_scene(const scene_t &s);
void rt_multisample_draw(const scene_t &s, int n);
void rt_draw_frame(const scene_t &scene);

#endif
