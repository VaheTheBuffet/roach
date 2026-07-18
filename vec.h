#ifndef VEC_H
#define VEC_H

#include <cmath>
#include <cstdlib>

#include <cstdint>

inline bool almost_eq(const float &f1, const float &f2) {
    return std::abs(f1 - f2) < 0.0001;
}

struct vec3 {
    float e[3];

    vec3() = default;
    constexpr vec3(float x, float y, float z) : e{x, y, z} {}

    static vec3 random() {
        float x = static_cast<float>(std::rand()) / RAND_MAX;
        float y = static_cast<float>(std::rand()) / RAND_MAX;
        float z = static_cast<float>(std::rand()) / RAND_MAX;

        return vec3{x, y, z}.normalize();
    }

    constexpr static vec3 from_rgb(uint32_t color) {
        float b = (color & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        return vec3{r, g, b};
    }

    std::uint8_t r() const {
        return this->x() * 0xFF;
    }

    std::uint8_t g() const {
        return this->y() * 0xFF;
    }
    
    std::uint8_t b() const {
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
    return vec3{v1.x() * v2.x(), v1.y() * v2.y(), v1.z() * v2.z()};
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
    // normal has opposite direction to projection
    return 2 * dot(incident, normal) * normal - incident;
}

inline vec3 refract(const vec3 &incident, const vec3 &normal, float snells_factor) {
    // normal has length 1
    float cos_theta_i = dot(incident, normal);
    float sin2_theta_r = snells_factor * snells_factor * (1 - cos_theta_i * cos_theta_i);
    float sin_theta_r = std::sqrt(sin2_theta_r);

    return incident * snells_factor * sin_theta_r
        + incident * (std::sqrt(1.0 - snells_factor * snells_factor * sin2_theta_r) - snells_factor * sin_theta_r);
}

using point3 = vec3;
using color3 = vec3;
#endif
