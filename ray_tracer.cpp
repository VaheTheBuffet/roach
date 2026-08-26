// #define NDEBUG
#define DEBUG_LOG

#include <iostream>

#include "ctx.h"
#include "ray.h"
#include "sphere.h"
#include "util.h"
#include "vec.h"
#include "video.h"

#include "ray_tracer.h"

// per draw call
struct settings_t {
    float color_factor;
    float offset_x;
    float offset_y;

    sphere *spheres;
    int n_spheres;

    vec3 *lights;
    int n_lights;
};

struct state_t {
    // thread local
    ray rays[STACK_SIZE];
    color3 colors[STACK_SIZE];
    int depths[STACK_SIZE];
    int enclosing_spheres[STACK_SIZE];
    size_t stack_top = 0;

    // per coord
    color3 coord_color;

    // per path
    int path;
    int hit_sphere;
    float min_factor;
    bool forward;
};

// ----------------------global----------------------------
settings_t settings{};
Ctx *ctx;
float dx;
float dy;
// ----------------------global----------------------------
// let's hope this is a good idea

void rt_init(Ctx &c) {
    ctx = &c;
    dx = static_cast<float>(v_viewport_width) / v_width;
    dy = static_cast<float>(v_viewport_height) / v_height;
}

// stop execution if ray is at target depth
bool debug_catch_ray_depth(state_t &s, int d) {
    if (s.depths[s.path] == d) {
        --s.stack_top;
        s.coord_color += s.colors[s.path];
        return true;
    } else if (s.hit_sphere == -1) {
        --s.stack_top;
        return true;
    } else if (s.depths[s.path] == 4) {
        --s.stack_top;
        return true;
    }

    return false;
}

void scatter_ray(state_t &s) {
    switch (settings.spheres[s.hit_sphere].mat.ty) {
        case material_type::lambertian: {
            s.rays[s.path].orig += s.min_factor * s.rays[s.path].dir;
            vec3 normal =
                (s.rays[s.path].orig - settings.spheres[s.hit_sphere].center) /
                settings.spheres[s.hit_sphere].r;
            s.rays[s.path].orig += normal * 1e-2;

            assert((s.rays[s.path].orig - settings.spheres[s.hit_sphere].center)
                .length() >= settings.spheres[s.hit_sphere].r);

            s.rays[s.path].dir = vec3::malley_random(normal);

            assert(dot(s.rays[s.path].dir, normal) >= 0.0f);

            s.rays[s.stack_top] = s.rays[s.path];
            s.rays[s.stack_top].dir = vec3::malley_random(normal);
            s.colors[s.stack_top] =
                0.5f * s.colors[s.path] * settings.spheres[s.hit_sphere].mat.albedo;
            s.depths[s.stack_top++] = ++s.depths[s.path];

            s.colors[s.path] *= 0.5f * settings.spheres[s.hit_sphere].mat.albedo;

            break;
        }

        case material_type::refractive: {
            float orientation = static_cast<float>(s.forward) * 2 - 1.0f;

            s.rays[s.path].orig += s.min_factor * s.rays[s.path].dir;
            vec3 normal = orientation *
                (s.rays[s.path].orig - settings.spheres[s.hit_sphere].center)
                    .normalize();

            s.rays[s.path].orig -= normal * 1e-2;

            assert(orientation *
                ((s.rays[s.path].orig - settings.spheres[s.hit_sphere].center)
                    .length() -
                settings.spheres[s.hit_sphere].r) <=
                0);

            float ior = std::pow(settings.spheres[s.hit_sphere].mat.refractive_index,
                orientation);
            float r0 = settings.spheres[s.hit_sphere].mat.reflectance;

            vec3 reflected_dir = reflect(s.rays[s.path].dir, normal);
            refract_result rr = refract(s.rays[s.path].dir, normal, ior);

            float cos = -dot(s.rays[s.path].dir, normal);
            cos = std::clamp(cos, 0.0f, 1.0f);
            assert(cos >= 0.0f);

            float f_schlick = r0 + (1.0f - r0) * std::pow(1.0f - cos, 5);
            f_schlick = std::clamp(f_schlick, 0.0f, 1.0f);
            assert(f_schlick <= 1.0f && f_schlick >= 0.0f);

            s.rays[s.stack_top].dir = reflected_dir;
            s.rays[s.stack_top].orig = s.rays[s.path].orig;
            s.colors[s.stack_top] = s.colors[s.path] * f_schlick;
            s.depths[s.stack_top++] = s.depths[s.path] + 1;

            s.depths[s.path] = rr.tir ? s.depths[s.path] + 1 : s.depths[s.path];
            s.rays[s.path].dir = rr.tir ? reflected_dir : rr.v;
            s.colors[s.path] *= 1.0f - f_schlick;

            break;
        }

        case material_type::reflective: {
            s.rays[s.path].orig += s.min_factor * s.rays[s.path].dir;
            vec3 normal = (s.rays[s.path].orig - settings.spheres[s.hit_sphere].center)
                .normalize();
            s.rays[s.path].orig += normal * 1e-2;

            s.rays[s.path].dir = reflect(s.rays[s.path].dir, normal).normalize();
            ++s.depths[s.path];

            break;
        }

        default: {
            break;
        }
    }
}

void trace_ray(state_t &s) {
    s.hit_sphere = -1;
    s.min_factor = 1000.0f;

    for (auto i = 0; i < settings.n_spheres; ++i) {
        bool new_min;
        quadratic_result qr = settings.spheres[i].intersection_ray(s.rays[s.path]);

        new_min = !qr.nan && qr.low > 0.0f && qr.low < s.min_factor;
        s.min_factor = new_min ? qr.low : s.min_factor;
        s.hit_sphere = new_min ? i : s.hit_sphere;
        s.forward |= new_min;

        new_min = !qr.nan && qr.high > 0.0f && qr.high < s.min_factor;
        s.min_factor = new_min ? qr.high : s.min_factor;
        s.hit_sphere = new_min ? i : s.hit_sphere;
        s.forward &= !new_min;
    }
}

void rt_draw_frame(const scene_t &scene) {
    state_t s{};
    settings.spheres = scene.spheres;
    settings.n_spheres = scene.n_spheres;

    for (auto y = 0; y < v_height; ++y) {
        int y_t = v_height - y - 1;

        #ifdef DEBUG_LOG
        std::cout << "\r                    \r" << static_cast<float>(y) / v_height
            << std::flush;
        #endif

        for (auto x = 0; x < v_width; ++x) {

            s.coord_color = {0, 0, 0};

            float px = v_viewport_width * (static_cast<float>(x) / v_width - 0.5f) +
                settings.offset_x;
            float py = v_viewport_height * (static_cast<float>(y) / v_height - 0.5f) +
                settings.offset_y;

            s.rays[s.stack_top] =
                ray{point3{0.0f, 0.0f, 0.0f}, vec3{px, py, v_near}.normalize()};
            s.colors[s.stack_top] = color3{1.0f, 1.0f, 1.0f};
            s.depths[s.stack_top++] = 1;

            while (s.stack_top > 0) {
                assert(s.stack_top <= STACK_SIZE);

                s.path = s.stack_top - 1;
                trace_ray(s);

                if (s.hit_sphere == -1) {
                    s.colors[s.path] *= background_color;
                    s.coord_color += s.colors[s.path];
                    --s.stack_top;
                    continue;
                } else if (s.depths[s.path] == MAX_DEPTH) {
                    --s.stack_top;
                    // s.coord_color += s.colors[s.path];
                    s.colors[s.path] * background_color * settings.spheres[s.hit_sphere].mat.albedo;
                    continue;
                }

                scatter_ray(s);
            }

            s.coord_color.e[0] = std::sqrt(std::min(s.coord_color[0], 1.0f));
            s.coord_color.e[1] = std::sqrt(std::min(s.coord_color[1], 1.0f));
            s.coord_color.e[2] = std::sqrt(std::min(s.coord_color[2], 1.0f));
            s.coord_color *= settings.color_factor;

            ctx->add_pixel(x, y_t, s.coord_color);
        }
    }

    #ifdef DEBUG_LOG
    std::cout << '\n';
    #endif
}

void rt_multisample_draw(const scene_t &scene, int n) {
    settings.color_factor = 1.0 / n;
    offset_table ot(n, dx, dy);

    for (int i = 0; i < n; ++i) {
        settings.offset_x = ot[i];
        settings.offset_y = ot[i + 1];
        rt_draw_frame(scene);
    }
    settings.color_factor = 1.0f;
    settings.offset_x = 0.0f;
    settings.offset_y = 0.0f;
}
