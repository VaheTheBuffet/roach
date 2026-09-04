//#define NDEBUG
#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <raylib.h>
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

void parse_config() {
    FILE* f = std::fopen("settings.json", "r");
    if (!f) {
        f = fopen("settings.json", "wb");
        //TODO:
        //write default_config
        fclose(f);
        return;
    }

    int size;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        return;
    }

    char *json_string = new char[size + 1];
    std::memset(json_string, 0, size + 1);

    fread(json_string, 1, size, f);
    fclose(f);

    cJSON *json = cJSON_Parse(json_string);
    cJSON *spheres = cJSON_GetObjectItemCaseSensitive(json, "spheres");
    cJSON *sphere;
    int n = 0;

    if (!spheres)
        goto cleanup;

    sphere = spheres->child;

    for (; sphere; ++n, sphere=sphere->next) {
        assert(n < SPHERE_POOL_SIZE - 1);

        cJSON *sphere_p = cJSON_GetObjectItemCaseSensitive(sphere, "position");
        cJSON *sphere_r = cJSON_GetObjectItemCaseSensitive(sphere, "radius");
        cJSON *sphere_mat = cJSON_GetObjectItemCaseSensitive(sphere, "material");
        cJSON *sphere_ior = cJSON_GetObjectItemCaseSensitive(sphere, "ior");
        cJSON *sphere_albedo = cJSON_GetObjectItemCaseSensitive(sphere, "albedo");

        float x = cJSON_GetNumberValue(cJSON_GetArrayItem(sphere_p, 0));
        float y = cJSON_GetNumberValue(cJSON_GetArrayItem(sphere_p, 1));
        float z = cJSON_GetNumberValue(cJSON_GetArrayItem(sphere_p, 2));

        sphere_pool[n].center = {x, y, z};
        sphere_pool[n].r = cJSON_GetNumberValue(sphere_r);

        cJSON_GetObjectItemCaseSensitive(sphere, "material");
        const char *sphere_mat_s = cJSON_GetStringValue(sphere_mat);

        if (!sphere_mat || std::strcmp(sphere_mat_s, "lambertian") == 0) {
            int rgb = strtol(cJSON_GetStringValue(sphere_albedo), NULL, 16);
            sphere_pool[n].mat = material::lambertian(color3::from_rgb(rgb));

            continue;
        }

        if (std::strcmp(sphere_mat_s, "dielectric") == 0) {
            assert(n < SPHERE_POOL_SIZE - 1);

            cJSON *sphere_width = cJSON_GetObjectItemCaseSensitive(sphere, "width");

            sphere_pool[n++].mat = material::refractive(1.f / cJSON_GetNumberValue(sphere_ior));

            sphere_pool[n].center.e[0] = cJSON_GetNumberValue(cJSON_GetArrayItem(sphere_p, 0));
            sphere_pool[n].center.e[1] = cJSON_GetNumberValue(cJSON_GetArrayItem(sphere_p, 1));
            sphere_pool[n].center.e[2] = cJSON_GetNumberValue(cJSON_GetArrayItem(sphere_p, 2));
            sphere_pool[n].r = cJSON_GetNumberValue(sphere_r) - cJSON_GetNumberValue(sphere_width);
            sphere_pool[n].mat = material::refractive(cJSON_GetNumberValue(sphere_ior));

            continue;
        }

        if (std::strcmp(sphere_mat_s, "reflective") == 0) {
            sphere_pool[n].mat = material::reflective();

            continue;
        }
    }

    scene.spheres = sphere_pool;
    scene.n_spheres = n;

    cleanup:
    cJSON_Delete(json);
    delete[] json_string;
}

int main() {
    parse_config();

    Ctx ctx(v_width, v_height);
    rt_init(ctx);

    auto start = std::chrono::high_resolution_clock::now();

    //rt_draw_frame(scene);
    rt_multisample_draw(scene, 63);
    write_buf_to_file(ctx.buf, v_width, v_height);
    ctx.clear();

    auto end = std::chrono::high_resolution_clock::now();
    std::cout<<"elapsed time is " << std::chrono::duration<float, std::milli>{end - start}.count() << "\n";

    return 0;
}
