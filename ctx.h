#ifndef CTX_H
#define CTX_H
#include <cstdint>
#include <cstring>
#include <algorithm>

class Ctx {
public:
	Ctx(int width, int height, std::uint8_t *buf) : width(width), height(height), buf(buf) {}

	void draw_rect(int x, int y, int width, int height, std::uint32_t color)
	{
		auto x_min = std::max(0, x);
		auto y_min = std::max(0, y);
		auto y_max = std::min(y + height, this->height);
		auto x_max = std::min(x + width, this->width);

		for(auto iy = y; iy < y_max; iy++) {
			for(auto ix = x; ix < x_max; ix++) {
				buf[ix*3 + iy*this->width*3] = color >> 16 & 0xFF;
				buf[ix*3 + iy*this->width*3 + 1] = color >> 8 & 0xFF;
				buf[ix*3 + iy*this->width*3 + 2] = color & 0xFF;				
			}
		}
	}

	void clear(std::uint32_t color)
	{
		for(auto i = 0; i < this->width * this->height; i++) {
			this->buf[i] = color >> 16 & 0xFF;
			this->buf[i+1] = color >> 8 & 0xFF;
			this->buf[i+1] = color & 0xFF;
		}
	}
	
private:
	int width;
	int height;
	std::uint8_t *buf;
};
#endif
