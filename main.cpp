#include <iostream>
#include <cstdint>
#include <raylib.h>
#include <stdio.h>
#include "ctx.h"

#define WIDTH 1280
#define HEIGHT 720

static std::uint8_t image_buf[WIDTH * HEIGHT * 3];

void write_buf_to_file(std::uint8_t *buf, int width, int height)
{
	static int idx = 0;

	const char *name = ("image" + std::to_string(idx++) + ".ppm").c_str();
	FILE *image_file = fopen(name, "wb");
	if(!image_file) {
		throw std::runtime_error("failed to create image file");
	}

	fprintf(image_file, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
	fwrite(image_buf, 1, WIDTH * HEIGHT * 3, image_file);
	fclose(image_file);
}

constexpr float sphere_position

int main()
{
	sphere_p = point3(0, 0, -5);
	sphere_r = 3;

	

	return 0;
}
