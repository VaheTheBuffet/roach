#ifndef CTX_H
#define CTX_H

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <emmintrin.h>
#include <sys/mman.h>
#include <immintrin.h>

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


    void draw_rect(int x, int y, int width, int height, const color3 &color);


    void clear()
    {
        std::memset(this->buf, 0, this->width * this->height * 3);
    }

    void clear(const color3 &color) {

    }
};

#endif
