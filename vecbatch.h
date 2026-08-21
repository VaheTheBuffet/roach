#ifndef VECBATCH_H
#define VECBATCH_H

#include <immintrin.h>

#include "vec.h"

struct vec3batch {
    __m256 e[3];

    vec3batch() = default;
    vec3batch(__m256 xs, __m256 ys, __m256 zs) : e{xs, ys, zs} {}

    // soa to soa
    vec3batch(const float *xs, const float *ys, const float *zs, const int *indices, int n = 8) {
        __m256i idx = _mm256_loadu_epi32(indices);

        this->e[0] = _mm256_i32gather_ps(xs, idx, sizeof(*xs));
        this->e[1] = _mm256_i32gather_ps(ys, idx, sizeof(*xs));
        this->e[2] = _mm256_i32gather_ps(zs, idx, sizeof(*xs));
    }

    // aos to soa
    vec3batch(const vec3 *batch, const int *indices, int n = 8) {
        __m256i idx = _mm256_loadu_epi32(indices);
        __m256i scalev = _mm256_set1_epi32(sizeof(*batch));
        __m256i offsetv = _mm256_set1_epi32(sizeof(batch->x()));

        idx = _mm256_mullo_epi32(idx, scalev);
        this->e[0] = _mm256_i32gather_ps(batch, idx, 1);
        idx = _mm256_add_epi32(idx, offsetv);
        this->e[1] = _mm256_i32gather_ps(batch, idx, 1);
        idx = _mm256_add_epi32(idx, offsetv);
        this->e[2] = _mm256_i32gather_ps(batch, idx, 1);
    }

    __m256 xs() {
        return this->e[0];
    }

    __m256 ys() {
        return this->e[1];
    }

    __m256 zs() {
        return this->e[2];
    }

    vec3batch &operator+=(const vec3batch &other) {
        this->e[0] = _mm256_add_ps(this->e[0], other.e[0]);
        this->e[1] = _mm256_add_ps(this->e[1], other.e[1]);
        this->e[2] = _mm256_add_ps(this->e[2], other.e[2]);

        return *this;
    }

    vec3batch &operator-=(const vec3batch &other) {
        this->e[0] = _mm256_sub_ps(this->e[0], other.e[0]);
        this->e[1] = _mm256_sub_ps(this->e[1], other.e[1]);
        this->e[2] = _mm256_sub_ps(this->e[2], other.e[2]);

        return *this;
    }

    vec3batch &operator*=(const __m256 &f) {
        this->e[0] = _mm256_mul_ps(this->e[0], f);
        this->e[1] = _mm256_mul_ps(this->e[0], f);
        this->e[2] = _mm256_mul_ps(this->e[0], f);

        return *this;
    }
};

#endif
