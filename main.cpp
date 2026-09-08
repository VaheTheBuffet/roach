//#define NDEBUG
#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <chrono>
#include <iostream>

#include "material.h"
#include "sphere.h"
#include "vec.h"
#include "video.h"
#include "ctx.h"

#include "cjson/cJSON.h"

#include "ray_tracer.h"

//object pool
#define SPHERE_POOL_SIZE 30
sphere sphere_pool[SPHERE_POOL_SIZE];
scene_t scene;
int n_samples;

bool parse_config(const char *path) {
    FILE *f = std::fopen(path, "r");
    if (!f) {
        std::cerr << "failed to open config: " << path << "\n";
        return false;
    }

    int size;
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::cerr << "failed to seek config: " << path << "\n";
        std::fclose(f);
        return false;
    }

    size = std::ftell(f);
    if (size < 0) {
        std::cerr << "failed to determine config size: " << path << "\n";
        std::fclose(f);
        return false;
    }
    std::fseek(f, 0, SEEK_SET);

    char *json_string = new char[size + 1];
    std::memset(json_string, 0, size + 1);

    std::fread(json_string, 1, size, f);
    std::fclose(f);

    cJSON *json = cJSON_Parse(json_string);
    delete[] json_string;

    if (!json) {
        std::cerr << "failed to parse config: " << path << "\n";
        return false;
    }

    cJSON *samples = cJSON_GetObjectItemCaseSensitive(json, "samples");
    if (!samples || !cJSON_IsNumber(samples)) {
        std::cerr << "config missing numeric 'samples' field\n";
        cJSON_Delete(json);
        return false;
    }
    n_samples = static_cast<int>(cJSON_GetNumberValue(samples));

    cJSON *spheres = cJSON_GetObjectItemCaseSensitive(json, "spheres");
    cJSON *sphere = nullptr;
    int n = 0;

    if (!spheres || !cJSON_IsArray(spheres)) {
        cJSON_Delete(json);
        return false;
    }

    sphere = spheres->child;

    for (; sphere; ++n, sphere=sphere->next) {
        if (!cJSON_IsObject(sphere)) {
            continue;
        }

        assert(n < SPHERE_POOL_SIZE - 1);

        cJSON *sphere_p = cJSON_GetObjectItemCaseSensitive(sphere, "position");
        cJSON *sphere_r = cJSON_GetObjectItemCaseSensitive(sphere, "radius");
        cJSON *sphere_mat = cJSON_GetObjectItemCaseSensitive(sphere, "material");
        cJSON *sphere_ior = cJSON_GetObjectItemCaseSensitive(sphere, "ior");
        cJSON *sphere_albedo = cJSON_GetObjectItemCaseSensitive(sphere, "albedo");
        cJSON *sphere_width = cJSON_GetObjectItemCaseSensitive(sphere, "width");

        if (!sphere_p || !cJSON_IsArray(sphere_p) || !sphere_r || !cJSON_IsNumber(sphere_r)) {
            continue;
        }

        float x = cJSON_GetNumberValue(cJSON_GetArrayItem(sphere_p, 0));
        float y = cJSON_GetNumberValue(cJSON_GetArrayItem(sphere_p, 1));
        float z = cJSON_GetNumberValue(cJSON_GetArrayItem(sphere_p, 2));

        sphere_pool[n].center = {x, y, z};
        sphere_pool[n].r = cJSON_GetNumberValue(sphere_r);

        const char *sphere_mat_s = sphere_mat && cJSON_IsString(sphere_mat)
            ? cJSON_GetStringValue(sphere_mat)
            : nullptr;

        if (!sphere_mat_s || std::strcmp(sphere_mat_s, "lambertian") == 0) {
            if (sphere_albedo && cJSON_IsString(sphere_albedo)) {
                int rgb = std::strtol(cJSON_GetStringValue(sphere_albedo), nullptr, 16);
                sphere_pool[n].mat = material::lambertian(color3::from_rgb(rgb));
            }
            continue;
        }

        if (std::strcmp(sphere_mat_s, "refractive") == 0 ||
            std::strcmp(sphere_mat_s, "dielectric") == 0) {
            assert(sphere_ior != nullptr);
            assert(n < SPHERE_POOL_SIZE - 1);

            sphere_pool[n++].mat = material::refractive(1.f / cJSON_GetNumberValue(sphere_ior));

            sphere_pool[n].center.e[0] = x;
            sphere_pool[n].center.e[1] = y;
            sphere_pool[n].center.e[2] = z;
            sphere_pool[n].r = cJSON_GetNumberValue(sphere_r) - (sphere_width ? cJSON_GetNumberValue(sphere_width) : 0.0f);
            sphere_pool[n].mat = material::refractive(cJSON_GetNumberValue(sphere_ior));
            continue;
        }

        if (std::strcmp(sphere_mat_s, "reflective") == 0) {
            sphere_pool[n].mat = material::reflective();
        }
    }

    scene.spheres = sphere_pool;
    scene.n_spheres = n;
    cJSON_Delete(json);
    return true;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: roach <config.json>\n";
        return 1;
    }

    if (!parse_config(argv[1])) {
        return 1;
    }

    Ctx ctx(v_width, v_height);
    rt_init(ctx);

    auto start = std::chrono::high_resolution_clock::now();

    //rt_draw_frame(scene);
    rt_multisample_draw(scene, n_samples);
    write_buf_to_file(ctx.buf, v_width, v_height);
    ctx.clear();

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "elapsed time is " << std::chrono::duration<float, std::milli>{end - start}.count() << "\n";

    return 0;
}
