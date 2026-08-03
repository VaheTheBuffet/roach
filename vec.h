#ifndef VEC_H
#define VEC_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>

#include <cstdint>
#include <raylib.h>

inline bool almost_eq(const float &f1, const float &f2) {
    return std::abs(f1 - f2) < 0.0001;
}

struct vec3 {
    float e[3];

    vec3() = default;
    constexpr vec3(float x, float y, float z) : e{x, y, z} {}

    static vec3 random();
    static vec3 malley_random(const vec3 &n);

    constexpr static vec3 from_rgb(uint32_t color) {
        float b = (color & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        return vec3{r, g, b};
    }

    std::uint8_t r() const {
        assert(this->x() >= 0.0f && this->x() <= 1.0f);
        return this->x() * 0xFF;
    }

    std::uint8_t g() const {
        assert(this->y() >= 0.0f && this->y() <= 1.0f);
        return this->y() * 0xFF;
    }

    std::uint8_t b() const {
        assert(this->z() >= 0.0f && this->z() <= 1.0f);
        return this->z() * 0xFF;
    }

    float x() const {
        return this->e[0];
    }

    float y() const {
        return this->e[1];
    }

    float z() const {
        return this->e[2];
    }

    float operator[](size_t i) const {
        return this->e[i];
    }

    vec3 &operator+=(const vec3 &other) {
        this->e[0] += other.e[0];
        this->e[1] += other.e[1];
        this->e[2] += other.e[2];
        return *this;
    }

    vec3 &operator-=(const vec3 &other) {
        this->e[0] -= other.e[0];
        this->e[1] -= other.e[1];
        this->e[2] -= other.e[2];
        return *this;
    }

    vec3 &operator*=(const vec3 &other) {
        this->e[0] *= other.e[0];
        this->e[1] *= other.e[1];
        this->e[2] *= other.e[2];
        return *this;
    }

    vec3 operator-() const {
        return vec3(-this->e[0], -this->e[1], -this->e[2]);
    }

    vec3 &operator*=(float factor) {
        this->e[0] *= factor;
        this->e[1] *= factor;
        this->e[2] *= factor;
        return *this;
    }

    vec3 &operator/=(float factor) {
        return *this *= 1 / factor;
    }

    float length_squared() const {
        return this->e[0]*this->e[0] + this->e[1]*this->e[1] + this->e[2]*this->e[2];
    }

    float length() const {
        return std::sqrt(this->length_squared());
    }

    vec3 &normalize() {
        (*this) /= this->length();
        return *this;
    }
};

inline vec3 operator+(const vec3 &v1, const vec3 &v2) {
    return vec3(v1.e[0] + v2.e[0], v1.e[1] + v2.e[1], v1.e[2] + v2.e[2]);
}

inline vec3 operator*(const vec3 &v, const float c) {
    return vec3(v.e[0] * c, v.e[1] * c, v.e[2] * c);
}

inline vec3 operator*(float c, const vec3 &v) {
    return v * c;
}

inline vec3 operator*(const vec3 &v1, const vec3 &v2) {
    return vec3{v1.e[0] * v2.e[0], v1.e[1] * v2.e[1], v1.e[2] * v2.e[2]};
}

inline vec3 operator/(const vec3 &v, const float c) {
  return v * (1.0 / c);
}

inline vec3 operator-(const vec3 &v1, const vec3 &v2) {
    return vec3(v1.e[0] - v2.e[0], v1.e[1] - v2.e[1], v1.e[2] - v2.e[2]);
}

inline float dot(const vec3 &v1, const vec3 &v2) {
    return v1.e[0] * v2.e[0] + v1.e[1] * v2.e[1] + v1.e[2] * v2.e[2];
}

inline vec3 cross(const vec3 &v1, const vec3 &v2) {
    return vec3(
            v1.e[1]*v2.e[2] - v1.e[2]*v2.e[1],
            v1.e[2]*v2.e[0] - v1.e[0]*v2.e[2],
            v1.e[0]*v2.e[1] - v1.e[1]*v2.e[0]);
}

inline bool operator==(const vec3 &v1, const vec3 &v2) {
    return almost_eq(v1.e[0], v2.e[0]) && almost_eq(v1.e[1], v1.e[2]) && almost_eq(v1.e[2], v2.e[2]);
}

inline bool operator!=(const vec3 &v1, const vec3 &v2) {
    return !(v1 == v2);
}

inline vec3 reflect(const vec3 &incident, const vec3 &normal) {
    // normal has length 1
    assert(almost_eq(normal.length_squared(), 1.0f));
    return incident - 2.0f * dot(incident, normal) * normal;
}

inline vec3 refract(const vec3 &incident, const vec3 &normal, float n1, float n2) {
    // all vectors must be normalized
    // outward facing normal
    assert(almost_eq(incident.length_squared(), 1.0f));
    assert(almost_eq(normal.length(), 1.0f));

    float n1_n2 = n1 / n2;

    float cos_theta_i = std::max(-dot(incident, normal), 0.0f);

    float sin2_theta_r = std::min(n1_n2 * n1_n2 * (1.0f - cos_theta_i * cos_theta_i), 1.0f);

    float sin_theta_r = std::sqrt(sin2_theta_r);

    return incident * n1_n2 + normal * (n1_n2 * cos_theta_i - std::sqrt(1 - sin2_theta_r));
}

// all components chosen unifornly
inline vec3 vec3::random() {
  float x = static_cast<float>(std::rand()) / RAND_MAX;
  float y = static_cast<float>(std::rand()) / RAND_MAX;
  float z = static_cast<float>(std::rand()) / RAND_MAX;

  return vec3{x, y, z}.normalize();
}

// vector chosen from unit disk, then projected onto hemisphere
// gives cosine weighted pdf, useful for some calculations
// pde is cos(theta) / PI
inline vec3 vec3::malley_random(const vec3& n) {
  float r = std::sqrt(static_cast<float>(std::rand()) / RAND_MAX);
  float theta = 2 * PI * static_cast<float>(std::rand()) / RAND_MAX;

  // branchless orthonormal basis algorithm
  // relies on reflections which are orthogonal transformations
  float sign = std::copysign(1.0f, n.z());
  float a = -1.0f / (sign + n.z());
  float b = n.x() * n.y() * a;
  vec3 b1 = vec3(1.0f + sign * n.x() * n.x() * a, sign * b, -sign * n.x());
  vec3 b2 = vec3(b, sign + n.y() * n.y() * a, -n.y());

  float x = r * std::cos(theta);
  float y = r * std::sin(theta);
  float z = std::sqrt(std::max(0.0f, 1.0f - x*x -y*y));

  return x * b1 + y * b2 + z * n;
}

using point3 = vec3;
using color3 = vec3;
#endif
