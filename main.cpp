//#define NDEBUG
#include <ostream>
#include <pthread.h>
#include <raylib.h>
#include <chrono>
#include <iostream>

#include "material.h"
#include "sphere.h"
#include "vec.h"
#include "video.h"
#include "ctx.h"
#include "ray_tracer.h"

void draw_frame_ctx(Ctx &ctx) {
    ctx.draw_rect(0, 0, 10, 400, color3::from_rgb(0xFF0000));
}

int main() {
    scene_t state{};

    state.light_sources[0] = point3{0, 10, 0};

    state.spheres[0] = sphere{point3{0, 0, 5}, 1.1, material::lambertian(color3::from_rgb(0xAA12D0))};
    state.spheres[1] = sphere{point3{-5, 0, 5}, 1.1, material::lambertian(color3::from_rgb(0xAABBCC))};
    state.spheres[2] = sphere{point3{5, 0, 5}, 1.1, material::lambertian(color3::from_rgb(0x943118))};
    state.n_spheres = 3;
    state.n_light_sources = 1;

    Ctx ctx(v_width, v_height);

    auto start = std::chrono::high_resolution_clock::now();
    // draw scene 1
    draw_frame(state, ctx);
    write_buf_to_file(ctx.buf, v_width, v_height);
    ctx.clear();

    // draw scene 2
    // move light
    state.light_sources[0].e[0] += 5;
    draw_frame(state, ctx);
    write_buf_to_file(ctx.buf, v_width, v_height);
    ctx.clear();

    // draw scene 3
    draw_scene_with_refraction(ctx);
    write_buf_to_file(ctx.buf, v_width, v_height);
    ctx.clear();

    auto end = std::chrono::high_resolution_clock::now();
    std::cout<<"elapsed time is " << std::chrono::duration<float, std::milli>{end - start}.count() << "\n";

    return 0;
}
