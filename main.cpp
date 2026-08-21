#define NDEBUG
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ostream>
#include <pthread.h>
#include <raylib.h>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <chrono>
#include <iostream>
#include <cstring>

#include "material.h"
#include "sphere.h"
#include "ray.h"
#include "vec.h"
#include "util.h"

constexpr float fast_tan(float x) {
    float x_3 = x * x * x;
    float x_5 = x_3 * x * x;
    float x_7 = x_5 * x * x;

    return x + x_3 / 3 + x_5 / 15 + 17 * x_7 / 315;
}

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

static std::uint8_t image_buf[WIDTH * HEIGHT * 3];

void write_buf_to_file(std::uint8_t *buf, int width, int height) {
    static int idx = 0;

    std::string name = "image" + std::to_string(idx++) + ".ppm";
    FILE *image_file = fopen(name.c_str(), "wb");

    if (!image_file) {
        throw std::runtime_error("failed to create image file");
    }

    fprintf(image_file, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
    fwrite(image_buf, 1, WIDTH * HEIGHT * 3, image_file);
    fclose(image_file);
}

struct state {
    sphere spheres[10] = {};
    int n_spheres;
    point3 light_sources[10] = {};
    int n_light_sources;
};

void draw_frame(const state &state) {
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
                image_buf[3 * (x + y_t * WIDTH)] = color.r();
                image_buf[1 + 3 * (x + y_t * WIDTH)] = color.g();
                image_buf[2 + 3 * (x + y_t * WIDTH)] = color.b();
            }
        }
    }

    write_buf_to_file(image_buf, WIDTH, HEIGHT);
}

void draw_scene_with_refraction() {
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

                image_buf[3 * (x + y_t * WIDTH)] += global_color.r();
                image_buf[1 + 3 * (x + y_t * WIDTH)] += global_color.g();
                image_buf[2 + 3 * (x + y_t * WIDTH)] += global_color.b();
            }
        }

        std::cout<<'\n';
    }
    write_buf_to_file(image_buf, WIDTH, HEIGHT);
}

void clear_buffer() {
    std::memset(image_buf, 0, sizeof(image_buf));
}

int main() {
    state state{};

    state.light_sources[0] = point3{0, 10, 0};

    state.spheres[0] = sphere{point3{0, 0, 5}, 1.1, material::lambertian(color3::from_rgb(0xAA12D0))};
    state.spheres[1] = sphere{point3{-5, 0, 5}, 1.1, material::lambertian(color3::from_rgb(0xAABBCC))};
    state.spheres[2] = sphere{point3{5, 0, 5}, 1.1, material::lambertian(color3::from_rgb(0x943118))};
    state.n_spheres = 3;
    state.n_light_sources = 1;

    auto start = std::chrono::high_resolution_clock::now();
    draw_frame(state);
    clear_buffer();
    state.light_sources[0].e[0] += 5;
    draw_frame(state);
    clear_buffer();
    draw_scene_with_refraction();
    auto end = std::chrono::high_resolution_clock::now();
    std::cout<<"elapsed time is " << std::chrono::duration<float, std::milli>{end - start}.count() << "\n";

    return 0;
}
