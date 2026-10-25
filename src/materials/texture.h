#pragma once

#include <cmath>
#include <algorithm>

#include "../core/vec3.h"

class Texture {
public:
    virtual ~Texture() = default;

    virtual Vec3 value(
        double u,
        double v,
        const Vec3& p
    ) const = 0;

    virtual Vec3 value(
        double u,
        double v,
        const Vec3& p,
        double dudx,
        double dvdx,
        double dudy,
        double dvdy
    ) const {
        (void)dudx;
        (void)dvdx;
        (void)dudy;
        (void)dvdy;

        return value(u, v, p);
    }

    // Sample using screen-space UV differentials when available,
    // otherwise allow derived textures to use a ray-cone estimate.
    virtual Vec3 value_with_cone(
        double u,
        double v,
        const Vec3& p,
        double dudx,
        double dvdx,
        double dudy,
        double dvdy,
        bool has_uv_differentials,
        double cone_width,
        bool has_ray_cone,
        const Vec3& dpdu,
        const Vec3& dpdv,
        bool has_uv_parameterization
    ) const {
        if (has_uv_differentials) {
            return value(
                u, v, p,
                dudx, dvdx, dudy, dvdy
            );
        }

        (void)cone_width;
        (void)has_ray_cone;
        (void)dpdu;
        (void)dpdv;
        (void)has_uv_parameterization;

        return value(u, v, p);
    }
};

class SolidColor final : public Texture {
public:
    explicit SolidColor(const Vec3& color)
        : color_(color) {}

    SolidColor(double r, double g, double b)
        : color_(r, g, b) {}

    Vec3 value(
        double,
        double,
        const Vec3&
    ) const override {
        return color_;
    }

private:
    Vec3 color_;
};

class UVCheckerTexture final : public Texture {
public:
    UVCheckerTexture(
        double frequency,
        const Vec3& color_a,
        const Vec3& color_b
    )
        : frequency_(frequency),
          color_a_(color_a),
          color_b_(color_b) {}

    Vec3 value(
        double u,
        double v,
        const Vec3&
    ) const override {
        const auto iu = static_cast<long long>(
            std::floor(u * frequency_)
        );

        const auto iv = static_cast<long long>(
            std::floor(v * frequency_)
        );

        return ((iu + iv) % 2 == 0)
            ? color_a_
            : color_b_;
    }

private:
    double frequency_;
    Vec3 color_a_;
    Vec3 color_b_;
};

class UVGradientTexture final : public Texture {
public:
    UVGradientTexture(
        const Vec3& bottom,
        const Vec3& top
    )
        : bottom_(bottom),
          top_(top) {}

    Vec3 value(
        double,
        double v,
        const Vec3&
    ) const override {
        const double t = clamp01(v);

        return (1.0 - t) * bottom_ + t * top_;
    }

private:
    static double clamp01(double x) {
        if (x < 0.0) {
            return 0.0;
        }

        if (x > 1.0) {
            return 1.0;
        }

        return x;
    }

    Vec3 bottom_;
    Vec3 top_;
};

class CheckerTexture final : public Texture {
public:
    CheckerTexture(
        double frequency,
        const Vec3& color_a,
        const Vec3& color_b
    )
        : frequency_(frequency),
          color_a_(color_a),
          color_b_(color_b) {}

    Vec3 value(
        double,
        double,
        const Vec3& p
    ) const override {
        const double s =
            std::sin(frequency_ * p.x) *
            std::sin(frequency_ * p.y) *
            std::sin(frequency_ * p.z);

        return (s < 0.0)
            ? color_b_
            : color_a_;
    }

private:
    double frequency_;
    Vec3 color_a_;
    Vec3 color_b_;
};

class UVNormalTexture final : public Texture {
public:
    explicit UVNormalTexture(double frequency)
        : frequency_(frequency) {}

    Vec3 value(
        double u,
        double v,
        const Vec3&
    ) const override {
        constexpr double PI = 3.14159265358979323846;

        const double nx =
            0.75 * std::sin(2.0 * PI * frequency_ * u);

        const double ny =
            0.75 * std::sin(2.0 * PI * frequency_ * v);

        Vec3 n(2.0 * nx, 2.0 * ny, 1.0);

        const double length = n.length();
        if (length > 1e-12) {
            n = n / length;
        } else {
            n = Vec3(0.0, 0.0, 1.0);
        }

        return Vec3(
            0.5 * n.x + 0.5,
            0.5 * n.y + 0.5,
            0.5 * n.z + 0.5
        );
    }

private:
    double frequency_;
};