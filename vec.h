#ifndef VEC_H
#define VEC_H

#include <cmath>

class vec3 {
public:
	vec3(): e{0.0f, 0.0f, 0.0f} {}
	vec3(float x, float y, float z) : e{x, y, z} {}

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
		
	float e[3];
};

inline vec3 operator+(vec3 &v1, vec3 &v2) {
	return vec3(v1.e[0] + v2.e[0], v1.e[1] + v2.e[1], v1.e[2] + v2.e[2]);
}

inline vec3 operator*(vec3 &v, float c) {
	return vec3(v.e[0] * c, v.e[1] * c, v.e[2] * c);
}

inline vec3 operator*(float c, vec3 &v) {
	return v * c;
}

inline vec3 operator-(vec3 &v1, vec3 &v2) {
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

using point3 = vec3;
using color3 = vec3;
#endif
