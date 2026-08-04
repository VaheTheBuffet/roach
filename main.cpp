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
    point3 light_sources[10] = {};
};

void draw_frame(const state &state) {
    for (auto x = 0; x < WIDTH; ++x) {
        for (auto y = 0; y < HEIGHT; ++y) {
            //calculate corresponding ray through the viewport
            float px = VIEWPORT_WIDTH * (static_cast<float>(x) / WIDTH - 0.5);
            float py = VIEWPORT_HEIGHT * (static_cast<float>(y) / HEIGHT - 0.5);
            ray look_ray{{0.0f, 0.0f, 0.0f}, {px, py, NEAR}};

            for (auto &sphere: state.spheres) {
                float factor = sphere.intersection_ray(look_ray);

                if (factor == 0) {
                    continue;
                }

                point3 intersection = look_ray.dir * factor;
                float color_factor = 0.4;
                for (auto &ls : state.light_sources) {
                    if (ls == point3{0, 0, 0}) {
                        continue;
                    }

                    vec3 light_dir = ls - intersection;
                    vec3 sphere_normal = intersection - sphere.center;
                    float illumination = dot(light_dir, sphere_normal) / (light_dir.length() * sphere_normal.length());
                    color_factor += std::max(illumination, 0.0f);
                }

                color3 color;
                color.e[0] = std::min(1.0f, color_factor * sphere.color.e[0]);
                color.e[1] = std::min(1.0f, color_factor * sphere.color.e[1]);
                color.e[2] = std::min(1.0f, color_factor * sphere.color.e[2]);

                int y_t = HEIGHT - y;
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
        sphere{point3{0.0f, 0.0f, 8.0f}, 2.0f, color3::from_rgb(0x00FF00), material::transparent},
        sphere{point3{0.2f, 0.0f, 20.0f}, 8.0f, color3::from_rgb(0x0000FF), material::diffuse},
        sphere{point3{4.1f, 0.0f, 8.0f}, 2.0f, color3::from_rgb(0xFF0000), material::diffuse},
        //sphere{point3{-8.2f, 0.0f, 20.0f}, 8.0f, color3::from_rgb(0x0012FF), material::diffuse},
    };

    // material data
    float n1 = 1.2f;
    float n2 = 1.0f;
    float r_2 = (n2 - 1.0f) / (n2 + 1.0f);
    r_2 *= r_2;
    float r_1 = (n1 - 1.0f) / (n1 + 1.0f);
    r_1 *= r_1;

    // path state
    ray rays[STACK_SIZE]{};
    color3 colors[STACK_SIZE]{};
    int depths[STACK_SIZE]{};
    int bounding_volumes[STACK_SIZE]{};
    size_t stack_top = 0;

    for (auto y = 0; y < HEIGHT; ++y) {
        int y_t = HEIGHT - y - 1;

        std::cout << "\r                    \r"
                  << static_cast<float>(y) / HEIGHT;
        std::flush(std::cout);

        for (auto x = 0; x < WIDTH; ++x) {
            color3 global_color{};

            float px = VIEWPORT_WIDTH * (static_cast<float>(x) / WIDTH - 0.5f);
            float py = VIEWPORT_HEIGHT * (static_cast<float>(y) / HEIGHT - 0.5f);

            rays[stack_top] = ray{point3{0.0f, 0.0f, 0.0f}, vec3{px, py, NEAR}.normalize()};
            colors[stack_top] = color3{1.0f, 1.0f, 1.0f};
            bounding_volumes[stack_top] = -1;
            depths[stack_top++] = 1;

            // TODO: impelment a dynamic bounding volume hierarchy per path or
            // calculate both high and low root in case we are inside, maintain as state or
            // compute bounding volumte hierarchy from arbitrary volume data or
            // enforce bounding volume hierarchy as input

            while (stack_top > 0) {
                assert(stack_top <= STACK_SIZE);

                int path = stack_top - 1;

                int front_sphere = bounding_volumes[path];
                float min_factor = spheres[front_sphere].intersection_ray(rays[path], true);

                for (auto i = 0; i < sizeof(spheres) / sizeof(sphere); ++i) {
                    // basic ternary for condition c is a * c + b * (1 - c)
                    // since we will need to compute a - b anyway, we will represent as c * (a - b) + b
                    // we use the sign bit as the condition
                    float ray_factor = spheres[i].intersection_ray(rays[path]);
                    float diff = ray_factor - min_factor;
                    bool new_min = diff < 0 && ray_factor > 0;

                    min_factor += new_min * diff;
                    front_sphere += new_min * (i - front_sphere);

                    assert(min_factor != 0);
                }

                //int target_depth = 4;
                //if (depth == target_depth) {
                //    stack_top--;
                //    global_color += ray_color * background_color;
                //    continue;
                //} else if (!front_sphere) {
                //    --stack_top;
                //    continue;
                //} else if (depth == 4) {
                //    --stack_top;
                //    continue;
                //}

                if (front_sphere == -1) {
                    colors[path] *= background_color;
                    global_color += colors[path];
                    --stack_top;

                    continue;
                } else if (depths[path] == 4) {
                    --stack_top;
                    global_color += colors[path] * background_color * spheres[front_sphere].color;
                    continue;
                }

                assert(almost_eq(spheres[front_sphere].intersection_ray(rays[path]), min_factor));

                if (spheres[front_sphere].mat == material::diffuse) {
                    rays[path].orig += min_factor * rays[path].dir;
                    assert(almost_eq((rays[path].orig - spheres[front_sphere].center).length(), spheres[front_sphere].r));

                    vec3 normal = (rays[path].orig - spheres[front_sphere].center) / spheres[front_sphere].r;

                    rays[path].dir = vec3::malley_random(normal);
                    assert(dot(rays[path].dir, normal) >= 0.0f);

                    rays[stack_top] = rays[path];
                    rays[stack_top].dir = vec3::malley_random(normal);
                    colors[stack_top] = 0.3 * colors[path] * spheres[front_sphere].color;
                    depths[stack_top++] = ++depths[path];

                    colors[path] *= 0.3 * spheres[front_sphere].color;

                } else if (spheres[front_sphere].mat == material::transparent) {
                    rays[path].orig += min_factor * rays[path].dir;
                    assert(almost_eq((rays[path].orig - spheres[front_sphere].center).length(), spheres[front_sphere].r));

                    vec3 normal = (rays[path].orig - spheres[front_sphere].center).normalize();

                    rays[path].dir.normalize();
                    vec3 f_refracted_dir = refract(rays[path].dir, normal, n1, n2).normalize();
                    vec3 f_reflected_dir = reflect(rays[path].dir, normal).normalize();

                    float f_cos_theta = -dot(rays[path].dir, normal);
                    assert(f_cos_theta > 0.0f);

                    float f_schlick = r_2 + (1.0f-r_2) * std::pow(1.0f - f_cos_theta, 5);
                    f_schlick = std::clamp(f_schlick, 0.0f, 1.0f);
                    assert(f_schlick <= 1.0f && f_schlick >= 0.0f);

                    rays[stack_top].dir = f_reflected_dir;
                    rays[stack_top].orig = rays[path].orig;
                    colors[stack_top] = colors[path] * color3{f_schlick, f_schlick, f_schlick};
                    depths[stack_top++] = depths[path] + 1;

                    rays[path].dir = f_refracted_dir;
                    colors[path] *= 1.0f - f_schlick;
                    // we always seek the high root of the quadratic,
                    // because we are going from entry point through the interior of the sphere for this entire loop
                    float inner_factor = spheres[front_sphere].intersection_ray(rays[path], true);
                    rays[path].orig += f_refracted_dir * inner_factor;
                    assert(almost_eq((rays[path].orig - spheres[front_sphere].center).length(), spheres[front_sphere].r));

                    // handle inside ray and push every outside ray to stack
                    for (auto d = depths[path]; d < 4; ++d) {
                        normal = (spheres[front_sphere].center - rays[path].orig).normalize();

                        vec3 b_refracted_dir = refract(rays[path].dir, normal, n2, n1).normalize();
                        vec3 b_reflected_dir = reflect(rays[path].dir, normal).normalize();

                        float b_cos_theta = -dot(rays[path].dir, normal);
                        float b_schlick = r_1 + (1.0f-r_1) * std::pow(1.0f-b_cos_theta, 5);
                        b_schlick = std::clamp(b_schlick, 0.0f, 1.0f);

                        assert(b_schlick <= 1.0f && b_schlick >= 0.0f);

                        rays[stack_top].dir = b_refracted_dir;
                        // make sure outside ray is really outside
                        rays[stack_top].orig = rays[path].orig - normal * 1e-5f;
                        colors[stack_top] = colors[path] * color3{1.0f-b_schlick, 1.0f-b_schlick, 1.0f-b_schlick};
                        depths[stack_top++] = d;
                        depths[path] = d+1;

                        // continue along the reflected ray because it stays inside sphere
                        rays[path].dir = b_reflected_dir;
                        inner_factor = spheres[front_sphere].intersection_ray(rays[path], true);
                        rays[path].orig += rays[path].dir * inner_factor;
                        colors[path] *= b_schlick;
                    }
                }
            }

            global_color.e[0] = std::min(global_color[0], 1.0f);
            global_color.e[1] = std::min(global_color[1], 1.0f);
            global_color.e[2] = std::min(global_color[2], 1.0f);

            image_buf[3 * (x + y_t * WIDTH)] = global_color.r();
            image_buf[1 + 3 * (x + y_t * WIDTH)] = global_color.g();
            image_buf[2 + 3 * (x + y_t * WIDTH)] = global_color.b();
        }
    }

    std::cout<<'\n';
    write_buf_to_file(image_buf, WIDTH, HEIGHT);
}

void clear_buffer() {
    std::memset(image_buf, 0, sizeof(image_buf));
}

int main() {
    state state{};

    state.light_sources[0] = point3{0, 10, 0};

    state.spheres[0] = sphere{point3{0, 0, 5}, 1.1, color3::from_rgb(0xAA12D0), material::diffuse};
    state.spheres[1] = sphere{point3{-5, 0, 5}, 1.1, color3::from_rgb(0xAABBCC), material::diffuse};
    state.spheres[2] = sphere{point3{5, 0, 5}, 1.1, color3::from_rgb(0x943118), material::diffuse};

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
