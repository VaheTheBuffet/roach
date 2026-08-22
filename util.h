#ifndef UTIL_H
#define UTIL_H

#include <stdexcept>

struct offset_table {
    // x, y alternating
    float table[128];

    offset_table() = default;
    offset_table(offset_table &ot) = default;
    offset_table(offset_table &&ot) = default;

    // generates n points symmetrically around the rectangle of width dx / 2 height dy / 2
    offset_table(unsigned int n, float dx, float dy) : table{} {
        if (n > 63) {
            throw std::runtime_error("you don't need that many samples");
        }

        if (n == 1)
            return;

        unsigned int points = std::max<unsigned int>((0xFFFFFFFF >> __builtin_clz(n)) + 1, 4);
        unsigned int points_per_side = points >> 4;
        for (auto i = 2, j = 0; j < points_per_side; j++) {
            float x = 2.0f * dx / points_per_side;
            float y = 2.0f * dy / points_per_side;

            table[i++] = dx;
            table[i++] = -dy + y;

            table[i++] = dx - x;
            table[i++] = dy;

            table[i++] = -dx;
            table[i++] = dy - y;

            table[i++] = -dx + x;
            table[i++] = -dy;
        }
    }

    float operator[](size_t i) {
        return this->table[i];
    }
};

static void write_buf_to_file(unsigned char *buf, int width, int height) {
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

constexpr float fast_tan(float x) {
    float x_3 = x * x * x;
    float x_5 = x_3 * x * x;
    float x_7 = x_5 * x * x;

    return x + x_3 / 3 + x_5 / 15 + 17 * x_7 / 315;
}

#endif
