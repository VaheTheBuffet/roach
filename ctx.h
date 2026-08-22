#ifndef CTX_H
#define CTX_H

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <sys/mman.h>
#include <immintrin.h>

#include "vec.h"

struct Ctx {
    std::uint8_t *buf;
    int width;
    int height;
    bool dyn: 1;

    Ctx(int width, int height, std::uint8_t *buf) : width(width), height(height), buf(buf), dyn{false} {}
    Ctx(int width, int height) : width{width}, height{height}, dyn{true} {
        this->buf = reinterpret_cast<std::uint8_t *>(mmap(NULL, width * height * 3, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0));
    }
    ~Ctx() {
        if (this->dyn) {
            munmap(this->buf, this->width * this->height * 3);
        }
    }

    void draw_rect(int x, int y, int width, int height, const color3 &color)
    {
        static uint64_t maskr[] = {0x00FF0000FF0000FF, 0xFF0000FF0000FF00, 0x00FF0000FF0000, 0x0000FF0000FF00};
        static __m256i maskrv = _mm256_loadu_epi8(maskr);
        static uint64_t maskg[] = {0xFF0000FF0000FF00, 0x0000FF0000FF0000, 0xFF0000FF0000FF, 0x00FF0000FF0000};
        static __m256i maskgv = _mm256_loadu_epi8(maskr);
        static uint64_t maskb[] = {0x0000FF0000FF0000, 0x00FF0000FF0000FF, 0x0000FF0000FF00, 0xFF0000FF0000FF};
        static __m256i maskbv = _mm256_loadu_epi8(maskr);

        auto y_min = std::max(0, y);
        auto y_max = std::min(y + height, this->height);
        auto x_max = std::min(x + width, this->width);

        std::uint8_t r = color.r();
        std::uint8_t g = color.g();
        std::uint8_t b = color.b();

        __m256i rv = _mm256_set1_epi8(r);
        __m256i gv = _mm256_set1_epi8(g);
        __m256i bv = _mm256_set1_epi8(b);

        __m256i col = rv;
        col = _mm256_blendv_epi8(col, gv, maskgv);
        col = _mm256_blendv_epi8(col, bv, maskbv);

        for(auto iy = y; iy < y_max; iy++) {
            for(auto ix = x; ix < x_max; ix += 10) {
                auto idx = 3 * (ix + iy * this->width);
                _mm256_storeu_epi8(this->buf + idx, col);
                std::memcpy(this->buf + idx + 18, this->buf + idx, 6);
            }

            if (x_max % 8 != 0) {

            }
        }
    }

    void clear()
    {
        std::memset(this->buf, 0, this->width * this->height * 3);
    }

    void clear(const color3 &color) {

    }
};

#endif
