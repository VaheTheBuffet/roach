#ifndef CTX_H
#define CTX_H

#include <cstdint>
#include <cstring>
#include <sys/mman.h>

#include "vec.h"

struct Ctx {
    std::uint8_t *buf;
    int width;
    int height;
    bool dyn: 1;

    Ctx(int width, int height, std::uint8_t *buf) : width(width), height(height), buf(buf), dyn{false} {}
    Ctx(int width, int height)
        : width{width}, height{height}, dyn{true},
          buf{reinterpret_cast<std::uint8_t *>(
              mmap(NULL, width * height * 3 + 32, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0))} {}
    ~Ctx() {
        if (this->dyn) {
            munmap(this->buf, this->width * this->height * 3);
        }
    }

    void clear()
    {
        std::memset(this->buf, 0, this->width * this->height * 3);
    }

    void set_pixel(int x, int y, const color3 &c) {
        int i = 3 * (x + this->width * y);
        this->buf[i] = c.r();
        this->buf[i + 1] = c.g();
        this->buf[i + 2] = c.b();
    }

    void add_pixel(int x, int y, const color3 &c) {
        int i = 3 * (x + this->width * y);
        this->buf[i] += c.r();
        this->buf[i + 1] += c.g();
        this->buf[i + 2] += c.b();
    }

    void draw_rect(int x, int y, int width, int height, const color3 &color);
};

#endif
