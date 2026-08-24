#include "ctx.h"

#if defined(__AVX2__)
void Ctx::draw_rect(int x, int y, int width, int height, const color3 &color)
{
    // alternating bytemasks to filter out each color component
    // assumes little endianness

    //static const std::uint64_t maskr[] __attribute__((aligned(32))) = {
    //0x00FF0000FF0000FF, 0xFF0000FF0000FF00, 0x0000FF0000FF0000,
    //0xFF0000FF0000FF00};
    alignas(32) static const std::uint64_t maskg[] = {
        0xFF0000FF0000FF00, 0x0000FF0000FF0000, 0x00FF0000FF0000FF,
        0xFF0000FF0000FF00};
    alignas(32) static const std::uint64_t maskb[] = {
        0x0000FF0000FF0000, 0x00FF0000FF0000FF, 0xFF0000FF0000FF00,
        0x0000FF0000FF0000};
    alignas(32) static std::uint64_t scratch[2];

    static const __m256i maskgv = _mm256_load_si256(reinterpret_cast<const __m256i *>(maskg));
    static const __m256i maskbv = _mm256_load_si256(reinterpret_cast<const __m256i *>(maskb));
    //static __m256i maskrv = _mm256_loadu_epi8(maskr);

    std::uint8_t r = color.r();
    std::uint8_t g = color.g();
    std::uint8_t b = color.b();

    __m256i rv = _mm256_set1_epi8(r);
    __m256i gv = _mm256_set1_epi8(g);
    __m256i bv = _mm256_set1_epi8(b);

    __m256i col = rv;
    col = _mm256_blendv_epi8(col, gv, maskgv);
    col = _mm256_blendv_epi8(col, bv, maskbv);

    _mm_storeu_si128(reinterpret_cast<__m128i_u *>(scratch), _mm256_castsi256_si128(col));

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
        // we use a phase of 30 as it's the largest multiple of 3 less than register width
        // TODO: use a color format that divides register witdth
        // TOTO: maybe unroll 3 iterations so 3 cleanly divides write width
        // TODO: gcc currently outperforms this
        while (remaining >= 32) {
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), col);
            p += 30;
            remaining -= 30;
        }

        // then try xmm stores
        if (remaining >= 16) {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(p), _mm256_castsi256_si128(col));
            p += 15;
            remaining -= 15;
        }

        // scalar copy the remainder
        std::memcpy(p, scratch, remaining);
    }        auto y_min = std::max(0, y);
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
