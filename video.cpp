#include <raylib.h>

#include "util.h"
#define VIDEO_CPP
#include "video.h"

int v_width = WIDTH;

int v_height = HEIGHT;

float v_inv_ar = static_cast<float>(v_height) / v_width;

float v_near = NEAR;

float v_fov = FOV;

float v_fov_tan = fast_tan(v_fov * PI / 360.f);

float v_viewport_width = 2.f * v_near * v_fov_tan;

float v_viewport_height = v_viewport_width * v_inv_ar;

void write_buf_to_file(unsigned char *buf, int width, int height)
{
    static int idx = 0;

    std::string name = "image" + std::to_string(idx++) + ".ppm";
    FILE *image_file = fopen(name.c_str(), "wb");

    if (!image_file) {
        throw std::runtime_error("failed to create image file");
    }

    fprintf(image_file, "P6\n%d %d\n255\n", width, height);
    fwrite(buf, 1, width * height * 3, image_file);
    fclose(image_file);
}
