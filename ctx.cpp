#include <immintrin.h>

#include "ctx.h"

#if defined(__AVX2__)
void Ctx::draw_rect(int x, int y, int width, int height, const color3 &color)
{
    // alternating bytemasks to filter out each color component
    // assumes little endianness
    alignas(32) static const std::uint64_t mask1[] {
        0x00FF0000FF0000FF, 0xFF0000FF0000FF00, 0x0000FF0000FF0000,
        0x00FF0000FF0000FF};
    alignas(32) static const std::uint64_t mask2[] = {
        0xFF0000FF0000FF00, 0x0000FF0000FF0000, 0x00FF0000FF0000FF,
        0xFF0000FF0000FF00};
    alignas(32) static const std::uint64_t mask3[] = {
        0x0000FF0000FF0000, 0x00FF0000FF0000FF, 0xFF0000FF0000FF00,
        0x0000FF0000FF0000};
    alignas(32) static std::uint64_t scratch[2];

    static const __m256i m1 = _mm256_load_si256(reinterpret_cast<const __m256i *>(mask1));
    static const __m256i m2 = _mm256_load_si256(reinterpret_cast<const __m256i *>(mask2));
    static const __m256i m3 = _mm256_load_si256(reinterpret_cast<const __m256i *>(mask3));

    std::uint8_t r = color.r();
    std::uint8_t g = color.g();
    std::uint8_t b = color.b();

    __m256i rv = _mm256_set1_epi8(r);
    __m256i gv = _mm256_set1_epi8(g);
    __m256i bv = _mm256_set1_epi8(b);

    __m256i c1 = rv;
    __m256i c2 = rv;
    __m256i c3 = rv;

    c1 = _mm256_blendv_epi8(c1, gv, m2);
    c1 = _mm256_blendv_epi8(c1, bv, m3);
    c2 = _mm256_blendv_epi8(c2, bv, m1);
    c2 = _mm256_blendv_epi8(c2, gv, m3);
    c3 = _mm256_blendv_epi8(c3, gv, m1);
    c3 = _mm256_blendv_epi8(c3, bv, m2);

    _mm_storeu_si128(reinterpret_cast<__m128i_u *>(scratch), _mm256_castsi256_si128(c1));

    const int x0 = std::max(0, x);
    const int x1 = std::min(this->width, x + width);

    const int y0 = std::max(0, y);
    const int y1 = std::min(this->height, y + height);

    if (x0 >= x1 || y0 >= y1)
        return;

    const int nbytes = 3 * (x1 - x0);

    for (int iy = y0; iy < y1; ++iy) {
        std::uint8_t *p = this->buf + 3 * (iy * this->width + x0);

        int remaining = nbytes;

        // first try ymm stores
        // we use a phase of 96 = lcm(3, 32) for rgb color format
        // TODO: use a color format that divides register width
        // TODO: gcc currently outperforms this
        while (remaining >= 96) {
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), c1);
            p += 32;
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), c2);
            p += 32;
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), c3);
            p += 32;
            remaining -= 96;
        }

        // remainder with xmm stores
        while (remaining >= 48) {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(p), _mm256_castsi256_si128(c1));
            p += 16;
            _mm_storeu_si128(reinterpret_cast<__m128i*>(p), _mm256_castsi256_si128(c3));
            p += 16;
            _mm_storeu_si128(reinterpret_cast<__m128i*>(p), _mm256_castsi256_si128(c2));
            p += 16;
            remaining -= 48;
        }

        // remainder with scalar copy
        std::memcpy(p, scratch, remaining);
    }
}
#else
void Ctx::draw_rect(int x, int y, int width, int height, const color3 &color)
{
    const int x0 = std::max(0, x);
    const int x1 = std::min(this->width, x + width);

    const int y0 = std::max(0, y);
    const int y1 = std::min(this->height, y + height);

    if (x0 >= x1 || y0 >= y1)
        return;

    std::uint8_t col[] = {color.r(), color.g(), color.b()};

    for (auto iy = y0; iy < y1; ++iy) {

        std::uint8_t *p = this->buf + 3 * (x + iy * this->width);
        for (auto ix = x; ix < x1; ++ix, p += 3) {
            std::memcpy(p, col, sizeof(col));
        }
    }
}
#endif
