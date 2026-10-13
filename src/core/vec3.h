#pragma once

#include <cmath>

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3() = default;

    Vec3(double x_, double y_, double z_)
        : x(x_), y(y_), z(z_) {}

    Vec3 operator+ (const Vec3& v) const {
        return {x + v.x, y + v.y, z + v.z};
    }

    Vec3 operator- (const Vec3& v) const {
        return {x - v.x, y - v.y, z - v.z};
    }

    Vec3 operator- () const {
        return {-x, -y, -z};
    }

    Vec3 operator* (double s) const {
        return {x * s, y * s, z * s};
    }

    Vec3 operator* (const Vec3& v) const {
        return {x * v.x, y * v.y, z * v.z};
    }

    Vec3 operator/ (double s) const {
        return {x / s, y / s, z / s};
    }

    double length_squared() const {
        return x * x + y * y + z * z;
    }

    double length() const {
        return std::sqrt(length_squared());
    }

    bool near_zero() const {
        constexpr double eps = 1e-12;
        return std::abs(x) < eps
            && std::abs(y) < eps
            && std::abs(z) < eps;
    }
};

inline Vec3 operator*(double s, const Vec3& v) {
    return v * s;
}

inline double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline Vec3 normalize(const Vec3& v) {
    const double len = v.length();
    if (len == 0.0) {
        return {};
    }
    return v / len;
}

inline Vec3 reflect(const Vec3& v, const Vec3& normal) {
    return v - 2.0 * dot(v, normal) * normal;
}

inline Vec3 refract(
    const Vec3& uv,
    const Vec3& normal,
    double eta_ratio
) {
    const double cos_theta = std::fmin(dot(-uv, normal), 1.0);
    const Vec3 perpendicular =
        eta_ratio * (uv + cos_theta * normal);

    const double parallel_sq =
        std::fmax(0.0, 1.0 - perpendicular.length_squared());

    const Vec3 parallel = -std::sqrt(parallel_sq) * normal;

    return perpendicular + parallel;
}
