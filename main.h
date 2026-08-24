#ifndef MAIN_H
#define MAIN_H

#include "sphere.h"
#include "vec.h"

struct state {
    sphere spheres[10] = {};
    int n_spheres;
    point3 light_sources[10] = {};
    int n_light_sources;
};

constexpr color3 background_color = color3::from_rgb(0x87CEEB);

#define WIDTH 1280
#define HEIGHT 720
#define INV_AR (static_cast<float>(HEIGHT) / WIDTH)
#define NEAR 0.1
#define FOV 90
#define FOV_TAN fast_tan(FOV * PI / 360)
#define VIEWPORT_WIDTH (2 * NEAR * FOV_TAN)
#define VIEWPORT_HEIGHT (VIEWPORT_WIDTH * INV_AR)

#define STACK_SIZE 64

#endif
