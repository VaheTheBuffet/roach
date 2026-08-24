#define NDEBUG
#include <algorithm>
#include <cmath>
#include <ostream>
#include <pthread.h>
#include <raylib.h>
#include <stdio.h>
#include <chrono>
#include <iostream>
#include <cstring>

#include "main.h"
#include "material.h"
#include "sphere.h"
#include "ray.h"
#include "vec.h"
#include "util.h"
#include "ctx.h"

void draw_scene_with_refraction_avx(Ctx &ctx) {
    // volumes
    sphere spheres[] = {
        sphere{point3{0.0f, -2.0f, 8.0f}, 2.0f, material::refractive(1.0f / 1.33f)},
        sphere{point3{0.0f, -2.0f, 8.0f}, 1.9f, material::refractive(1.33f)},
        sphere{point3{0.0f, 0.0f, 20.0f}, 8.0f, material::lambertian(color3::from_rgb(0x0000A0))},
        sphere{point3{4.1f, -2.0f, 8.0f}, 2.0f, material::lambertian(color3::from_rgb(0x00A000))},
        sphere{point3{0.0f, -10008.0f, 0.0f}, 10000.0f, material::lambertian(color3::from_rgb(0xA0A0A0))},
    };

    // path state
    ray rays[STACK_SIZE]{};
    color3 colors[STACK_SIZE]{};
    int depths[STACK_SIZE]{};
    int enclosing_spheres[STACK_SIZE]{};
    size_t stack_top = 0;

    int N = 1;
    float dx = static_cast<float>(VIEWPORT_WIDTH) / WIDTH;
    float dy = static_cast<float>(VIEWPORT_HEIGHT) / HEIGHT;

    offset_table offsets(N, dx / 2, dy / 2);

    for (auto n = 0; n < N; ++n) {
        for (auto y = 0; y < HEIGHT; ++y) {
            int y_t = HEIGHT - y - 1;

            std::cout << "\r                    \r"
                      << static_cast<float>(y) / HEIGHT
                      << std::flush;

            for (auto x = 0; x < WIDTH; ++x) {
                color3 global_color{};

//                float px = VW * (x / W - 1/2) + offsets[n]
//                         = VW / W * x - VW/2 + offsets[n]
//                         = dx * x + offsets[n] - VW / 2

                float px = VIEWPORT_WIDTH * (static_cast<float>(x) / WIDTH - 0.5f) + offsets[n];
                float py = VIEWPORT_HEIGHT * (static_cast<float>(y) / HEIGHT - 0.5f) + offsets[n + 1];

                rays[stack_top] = ray{point3{0.0f, 0.0f, 0.0f}, vec3{px, py, NEAR}.normalize()};
                colors[stack_top] = color3{1.0f, 1.0f, 1.0f};
                depths[stack_top++] = 1;

                bool front;

                while (stack_top > 0) {
                    assert(stack_top <= STACK_SIZE);

                    int path = stack_top - 1;
                    int hit_sphere = -1;
                    float min_factor = 1000.0f;

                    for (auto i = 0; i < sizeof(spheres) / sizeof(sphere); ++i) {
                        bool new_min;
                        quadratic_result qr = spheres[i].intersection_ray(rays[path]);

                        new_min = !qr.nan && qr.low > 0.0f && qr.low < min_factor;
                        min_factor = new_min ? qr.low : min_factor;
                        hit_sphere = new_min ? i : hit_sphere;
                        front |= new_min;

                        new_min = !qr.nan && qr.high > 0.0f && qr.high < min_factor;
                        min_factor = new_min ? qr.high : min_factor;
                        hit_sphere = new_min ? i : hit_sphere;
                        front &= !new_min;
                    }

                    //int target_depth = 2;
                    //if (depths[path] == target_depth) {
                    //    --stack_top;
                    //    global_color += colors[path];
                    //    continue;
                    //} else if (hit_sphere == -1) {
                    //    --stack_top;
                    //    continue;
                    //} else if (depths[path] == 4) {
                    //    --stack_top;
                    //    continue;
                    //}
                    //

                    if (hit_sphere == -1) {
                        colors[path] *= background_color;
                        global_color += colors[path];
                        --stack_top;
                        continue;
                    } else if (depths[path] == 4) {
                        --stack_top;
                        global_color += colors[path] * background_color * spheres[hit_sphere].mat.albedo;
                        continue;
                    }

                    switch (spheres[hit_sphere].mat.ty) {
                        case material_type::lambertian: {
                            rays[path].orig += min_factor * rays[path].dir;
                            vec3 normal = (rays[path].orig - spheres[hit_sphere].center) / spheres[hit_sphere].r;
                            rays[path].orig += normal * 1e-5;
                            //assert(almost_eq((rays[path].orig - spheres[hit_sphere].center).length(), spheres[hit_sphere].r));


                            rays[path].dir = vec3::malley_random(normal);
                            assert(dot(rays[path].dir, normal) >= 0.0f);

                            rays[stack_top] = rays[path];
                            rays[stack_top].dir = vec3::malley_random(normal);
                            colors[stack_top] = 0.5f * colors[path] * spheres[hit_sphere].mat.albedo;
                            depths[stack_top++] = ++depths[path];

                            colors[path] *= 0.5f * spheres[hit_sphere].mat.albedo;

                            break;
                        }

                        case material_type::refractive: {
                            float orientation = static_cast<float>(front) * 2 - 1.0f;

                            rays[path].orig += min_factor * rays[path].dir;
                            vec3 normal = orientation * (rays[path].orig - spheres[hit_sphere].center).normalize();
                            rays[path].orig -= normal * 1e-5;
                            assert(almost_eq((rays[path].orig - spheres[hit_sphere].center).length(), spheres[hit_sphere].r));

                            float ior = std::pow(spheres[hit_sphere].mat.refractive_index, orientation);
                            float r0 = spheres[hit_sphere].mat.reflectance;

                            vec3 reflected_dir = reflect(rays[path].dir, normal);
                            refract_result rr = refract(rays[path].dir, normal, ior);

                            float cos = -dot(rays[path].dir, normal);
                            assert(cos > 0.0f);
                            cos = std::clamp(cos, 0.0f, 1.0f);

                            float f_schlick = r0 + (1.0f-r0) * std::pow(1.0f - cos, 5);
                            f_schlick = std::clamp(f_schlick, 0.0f, 1.0f);
                            assert(f_schlick <= 1.0f && f_schlick >= 0.0f);

                            rays[stack_top].dir = reflected_dir;
                            rays[stack_top].orig = rays[path].orig;
                            colors[stack_top] = colors[path] * f_schlick;
                            depths[stack_top++] = depths[path] + 1;

                            depths[path] = rr.tir ? depths[path] + 1 : depths[path];
                            rays[path].dir = rr.tir ? reflected_dir : rr.v;
                            colors[path] *= 1.0f - f_schlick;

                            break;
                        }

                        case material_type::reflective: {
                            rays[path].orig += min_factor * rays[path].dir;
                            vec3 normal = (rays[path].orig - spheres[hit_sphere].center).normalize();

                            rays[path].dir = reflect(rays[path].dir, normal).normalize();
                            ++depths[path];

                            break;
                        }

                        default: {
                            break;
                        }
                    }
                }

                global_color.e[0] = std::sqrt(std::min(global_color[0], 1.0f)) / N;
                global_color.e[1] = std::sqrt(std::min(global_color[1], 1.0f)) / N;
                global_color.e[2] = std::sqrt(std::min(global_color[2], 1.0f)) / N;

                ctx.buf[3 * (x + y_t * WIDTH)] += global_color.r();
                ctx.buf[1 + 3 * (x + y_t * WIDTH)] += global_color.g();
                ctx.buf[2 + 3 * (x + y_t * WIDTH)] += global_color.b();
            }
        }

        std::cout<<'\n';
    }
}

void draw_frame(const state &state, Ctx &ctx) {
    for (auto x = 0; x < WIDTH; ++x) {
        for (auto y = 0; y < HEIGHT; ++y) {
            //calculate corresponding ray through the viewport
            float px = VIEWPORT_WIDTH * (static_cast<float>(x) / WIDTH - 0.5);
            float py = VIEWPORT_HEIGHT * (static_cast<float>(y) / HEIGHT - 0.5);
            ray look_ray{{0.0f, 0.0f, 0.0f}, {px, py, NEAR}};

            for (int i = 0; i < state.n_spheres; ++i) {
                const sphere &sphere = state.spheres[i];
                quadratic_result qr = sphere.intersection_ray(look_ray);

                if (qr.nan) {
                    continue;
                }

                float factor = qr.low;
                point3 intersection = look_ray.dir * factor;
                float color_factor = 0.4;

                for (int j = 0; j < state.n_light_sources; ++j) {
                    const point3 &ls = state.light_sources[j];
                    if (ls == point3{0, 0, 0}) {
                        continue;
                    }

                    vec3 light_dir = ls - intersection;
                    vec3 sphere_normal = intersection - sphere.center;
                    float illumination = dot(light_dir, sphere_normal) / (light_dir.length() * sphere_normal.length());
                    color_factor += std::max(illumination, 0.0f);
                }

                color3 color;
                color.e[0] = std::min(1.0f, color_factor * sphere.mat.albedo.e[0]);
                color.e[1] = std::min(1.0f, color_factor * sphere.mat.albedo.e[1]);
                color.e[2] = std::min(1.0f, color_factor * sphere.mat.albedo.e[2]);

                int y_t = HEIGHT - y - 1;
                ctx.buf[3 * (x + y_t * WIDTH)] = color.r();
                ctx.buf[1 + 3 * (x + y_t * WIDTH)] = color.g();
                ctx.buf[2 + 3 * (x + y_t * WIDTH)] = color.b();
            }
        }
    }
}

void draw_scene_with_refraction(Ctx &ctx) {
    // volumes
    sphere spheres[] = {
        sphere{point3{0.0f, -2.0f, 8.0f}, 2.0f, material::refractive(1.0f / 1.33f)},
        sphere{point3{0.0f, -2.0f, 8.0f}, 1.9f, material::refractive(1.33f)},
        sphere{point3{0.0f, 0.0f, 20.0f}, 8.0f, material::lambertian(color3::from_rgb(0x0000A0))},
        sphere{point3{4.1f, -2.0f, 8.0f}, 2.0f, material::lambertian(color3::from_rgb(0x00A000))},
        sphere{point3{0.0f, -10008.0f, 0.0f}, 10000.0f, material::lambertian(color3::from_rgb(0xA0A0A0))},
    };

    // path state
    ray rays[STACK_SIZE]{};
    color3 colors[STACK_SIZE]{};
    int depths[STACK_SIZE]{};
    int enclosing_spheres[STACK_SIZE]{};
    size_t stack_top = 0;

    int N = 1;
    float dx = static_cast<float>(VIEWPORT_WIDTH) / WIDTH;
    float dy = static_cast<float>(VIEWPORT_HEIGHT) / HEIGHT;

    offset_table offsets(N, dx / 2, dy / 2);

    for (auto n = 0; n < N; ++n) {
        for (auto y = 0; y < HEIGHT; ++y) {
            int y_t = HEIGHT - y - 1;

            std::cout << "\r                    \r"
                      << static_cast<float>(y) / HEIGHT
                      << std::flush;

            for (auto x = 0; x < WIDTH; ++x) {
                color3 global_color{};

//                float px = VW * (x / W - 1/2) + offsets[n]
//                         = VW / W * x - VW/2 + offsets[n]
//                         = dx * x + offsets[n] - VW / 2

                float px = VIEWPORT_WIDTH * (static_cast<float>(x) / WIDTH - 0.5f) + offsets[n];
                float py = VIEWPORT_HEIGHT * (static_cast<float>(y) / HEIGHT - 0.5f) + offsets[n + 1];

                rays[stack_top] = ray{point3{0.0f, 0.0f, 0.0f}, vec3{px, py, NEAR}.normalize()};
                colors[stack_top] = color3{1.0f, 1.0f, 1.0f};
                depths[stack_top++] = 1;

                bool front;

                while (stack_top > 0) {
                    assert(stack_top <= STACK_SIZE);

                    int path = stack_top - 1;
                    int hit_sphere = -1;
                    float min_factor = 1000.0f;

                    for (auto i = 0; i < sizeof(spheres) / sizeof(sphere); ++i) {
                        bool new_min;
                        quadratic_result qr = spheres[i].intersection_ray(rays[path]);

                        new_min = !qr.nan && qr.low > 0.0f && qr.low < min_factor;
                        min_factor = new_min ? qr.low : min_factor;
                        hit_sphere = new_min ? i : hit_sphere;
                        front |= new_min;

                        new_min = !qr.nan && qr.high > 0.0f && qr.high < min_factor;
                        min_factor = new_min ? qr.high : min_factor;
                        hit_sphere = new_min ? i : hit_sphere;
                        front &= !new_min;
                    }

                    //int target_depth = 2;
                    //if (depths[path] == target_depth) {
                    //    --stack_top;
                    //    global_color += colors[path];
                    //    continue;
                    //} else if (hit_sphere == -1) {
                    //    --stack_top;
                    //    continue;
                    //} else if (depths[path] == 4) {
                    //    --stack_top;
                    //    continue;
                    //}
                    //

                    if (hit_sphere == -1) {
                        colors[path] *= background_color;
                        global_color += colors[path];
                        --stack_top;
                        continue;
                    } else if (depths[path] == 4) {
                        --stack_top;
                        global_color += colors[path] * background_color * spheres[hit_sphere].mat.albedo;
                        continue;
                    }

                    switch (spheres[hit_sphere].mat.ty) {
                        case material_type::lambertian: {
                            rays[path].orig += min_factor * rays[path].dir;
                            vec3 normal = (rays[path].orig - spheres[hit_sphere].center) / spheres[hit_sphere].r;
                            rays[path].orig += normal * 1e-5;
                            //assert(almost_eq((rays[path].orig - spheres[hit_sphere].center).length(), spheres[hit_sphere].r));


                            rays[path].dir = vec3::malley_random(normal);
                            assert(dot(rays[path].dir, normal) >= 0.0f);

                            rays[stack_top] = rays[path];
                            rays[stack_top].dir = vec3::malley_random(normal);
                            colors[stack_top] = 0.5f * colors[path] * spheres[hit_sphere].mat.albedo;
                            depths[stack_top++] = ++depths[path];

                            colors[path] *= 0.5f * spheres[hit_sphere].mat.albedo;

                            break;
                        }

                        case material_type::refractive: {
                            float orientation = static_cast<float>(front) * 2 - 1.0f;

                            rays[path].orig += min_factor * rays[path].dir;
                            vec3 normal = orientation * (rays[path].orig - spheres[hit_sphere].center).normalize();
                            rays[path].orig -= normal * 1e-5;
                            assert(almost_eq((rays[path].orig - spheres[hit_sphere].center).length(), spheres[hit_sphere].r));

                            float ior = std::pow(spheres[hit_sphere].mat.refractive_index, orientation);
                            float r0 = spheres[hit_sphere].mat.reflectance;

                            vec3 reflected_dir = reflect(rays[path].dir, normal);
                            refract_result rr = refract(rays[path].dir, normal, ior);

                            float cos = -dot(rays[path].dir, normal);
                            assert(cos > 0.0f);
                            cos = std::clamp(cos, 0.0f, 1.0f);

                            float f_schlick = r0 + (1.0f-r0) * std::pow(1.0f - cos, 5);
                            f_schlick = std::clamp(f_schlick, 0.0f, 1.0f);
                            assert(f_schlick <= 1.0f && f_schlick >= 0.0f);

                            rays[stack_top].dir = reflected_dir;
                            rays[stack_top].orig = rays[path].orig;
                            colors[stack_top] = colors[path] * f_schlick;
                            depths[stack_top++] = depths[path] + 1;

                            depths[path] = rr.tir ? depths[path] + 1 : depths[path];
                            rays[path].dir = rr.tir ? reflected_dir : rr.v;
                            colors[path] *= 1.0f - f_schlick;

                            break;
                        }

                        case material_type::reflective: {
                            rays[path].orig += min_factor * rays[path].dir;
                            vec3 normal = (rays[path].orig - spheres[hit_sphere].center).normalize();

                            rays[path].dir = reflect(rays[path].dir, normal).normalize();
                            ++depths[path];

                            break;
                        }

                        default: {
                            break;
                        }
                    }
                }

                global_color.e[0] = std::sqrt(std::min(global_color[0], 1.0f)) / N;
                global_color.e[1] = std::sqrt(std::min(global_color[1], 1.0f)) / N;
                global_color.e[2] = std::sqrt(std::min(global_color[2], 1.0f)) / N;

                ctx.buf[3 * (x + y_t * WIDTH)] += global_color.r();
                ctx.buf[1 + 3 * (x + y_t * WIDTH)] += global_color.g();
                ctx.buf[2 + 3 * (x + y_t * WIDTH)] += global_color.b();
            }
        }

        std::cout<<'\n';
    }
}

void draw_frame_ctx(Ctx &ctx) {
    ctx.draw_rect(0, 0, 300, 300, color3::from_rgb(0xFF0000));
}

int main() {
    state state{};

    state.light_sources[0] = point3{0, 10, 0};

    state.spheres[0] = sphere{point3{0, 0, 5}, 1.1, material::lambertian(color3::from_rgb(0xAA12D0))};
    state.spheres[1] = sphere{point3{-5, 0, 5}, 1.1, material::lambertian(color3::from_rgb(0xAABBCC))};
    state.spheres[2] = sphere{point3{5, 0, 5}, 1.1, material::lambertian(color3::from_rgb(0x943118))};
    state.n_spheres = 3;
    state.n_light_sources = 1;

    Ctx ctx(WIDTH, HEIGHT);
    ctx.buf[HEIGHT * WIDTH - 1] = 0;

    auto start = std::chrono::high_resolution_clock::now();
    draw_frame(state, ctx);
    ctx.clear();

    state.light_sources[0].e[0] += 5;
    draw_frame(state, ctx);
    write_buf_to_file(ctx.buf, WIDTH, HEIGHT);
    ctx.clear();

    draw_scene_with_refraction(ctx);
    write_buf_to_file(ctx.buf, WIDTH, HEIGHT);
    ctx.clear();

    auto a = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        draw_frame_ctx(ctx);
    }
    auto b = std::chrono::high_resolution_clock::now();
    std::cout << "drawing square took " << std::chrono::duration<float, std::milli>{b - a}.count() / 1000 << "\n";

    write_buf_to_file(ctx.buf, WIDTH, HEIGHT);
    ctx.clear();

    auto end = std::chrono::high_resolution_clock::now();
    std::cout<<"elapsed time is " << std::chrono::duration<float, std::milli>{end - start}.count() << "\n";

    return 0;
}
