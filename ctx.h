#ifndef CTX_H
#define CTX_H
#include <cstdint>

class Ctx {
public:
	Ctx(int width, int height, std::uint8_t *buf) : width(width), height(height), buf(buf) {}

	void draw_rect(int x, int y, int width, int height, std::uint32_t color)
	{
		for(auto ix = x; ix < width; ix++) {
			for(auto iy = y; iy < height; iy++) {
				buf[ix*3 + iy*this->width*3] = color >> 16 & 0xFF;
				buf[ix*3 + iy*this->width*3 + 1] = color >> 8 & 0xFF;
				buf[ix*3 + iy*this->width*3 + 2] = color & 0xFF;				
			}
		}
	}
	
private:
	int width;
	int height;
	std::uint8_t *buf;
};
#endif
