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

int main()
{
	for(auto i = 0; i < WIDTH * HEIGHT * 3; i+=3) {
		image_buf[i] = static_cast<std::uint8_t>(255);
		image_buf[i+1] = static_cast<std::uint8_t>(0);
		image_buf[i+2] = static_cast<std::uint8_t>(0);		
	}

	write_buf_to_file(image_buf, WIDTH, HEIGHT);
 
	Ctx ctx(WIDTH, HEIGHT, image_buf);
	ctx.draw_rect(0, 0, 200, 200, 0x00FF00);
	write_buf_to_file(image_buf, WIDTH, HEIGHT);
	
	return 0;
}

