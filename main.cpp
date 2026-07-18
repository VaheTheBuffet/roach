#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <ostream>
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
    sphere spheres[] = {
        sphere{point3{0.0f, 0.0f, 8.0f}, 2.0f, color3::from_rgb(0xFF0000), material::transparent},
        sphere{point3{8.2f, 0.0f, 20.0f}, 8.0f, color3::from_rgb(0x00FF12), material::diffuse},
        sphere{point3{-8.2f, 0.0f, 20.0f}, 8.0f, color3::from_rgb(0x0012FF), material::diffuse},
    };

    for (auto y = 0; y < HEIGHT; ++y) {
        int y_t = HEIGHT - y - 1;

        std::cout << "\r                    \r" 
            << static_cast<float>(y) / HEIGHT;
        std::flush(std::cout);

        for (auto x = 0; x < WIDTH; ++x) {

            color3 color{1.0f, 1.0f, 1.0f};
            float px = VIEWPORT_WIDTH * (static_cast<float>(x) / WIDTH - 0.5f);
            float py = VIEWPORT_HEIGHT * (static_cast<float>(y) / HEIGHT - 0.5f);
            ray look_ray{point3{0.0f, 0.0f, 0.0f}, vec3{px, py, NEAR}.normalize()};

            //propagate ray
            while (true) {
                float min_factor = FLT_MAX;
                const sphere *front_sphere = NULL;

                for (const auto &sphere: spheres) {
                    float ray_factor = sphere.intersection_ray(look_ray);
                    if (ray_factor > 0.0f && ray_factor < min_factor) {
                       min_factor = ray_factor;
                       front_sphere = &sphere;
                    }
                }

                if (!front_sphere) {
                    color *= background_color;

                    image_buf[3 * (x + y_t * WIDTH)] = color.r();
                    image_buf[1 + 3 * (x + y_t * WIDTH)] = color.g();
                    image_buf[2 + 3 * (x + y_t * WIDTH)] = color.b();
                    break;
                }

                if (front_sphere->mat == material::diffuse) {
                    look_ray.orig += min_factor * look_ray.dir;

                    vec3 normal = (look_ray.orig - front_sphere->center) / front_sphere->r;

                    look_ray.dir = vec3::malley_random(normal);
                    assert(dot(look_ray.dir, normal) >= 0.0f);
                    color *= 0.5 * front_sphere->color;

                } else if (front_sphere->mat == material::transparent) {
                    look_ray.orig += min_factor * look_ray.dir;

                    vec3 normal = (look_ray.orig - front_sphere->center).normalize();
                    vec3 inner_dir = refract(look_ray.dir, normal, 1.0f, 1.1f);

                    assert(dot(inner_dir, look_ray.dir) > 0.0f);
                    look_ray.dir = inner_dir;

                    float inner_factor = front_sphere->intersection_ray(look_ray, true);

                    //TODO: there's a shortcut we can use for spherical refraction
                    look_ray.orig += inner_dir * inner_factor;
                    normal = (front_sphere->center - look_ray.orig).normalize();

                    // make sure we're out of the sphere
                    look_ray.orig -= 1e-3 * normal; 
                    assert((look_ray.orig - front_sphere->center).length() > front_sphere->r);

                    look_ray.dir.normalize();
                    vec3 out_dir = refract(look_ray.dir, normal, 1.1f, 1.0f);

                    assert(dot(out_dir, look_ray.dir) > 0.0f);
                    look_ray.dir = out_dir;
                }
            }
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
