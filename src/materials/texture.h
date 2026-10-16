#pragma once

#include <cmath>

#include "../core/vec3.h"

// Base interface for procedural textures.
class Texture {
public:
    virtual ~Texture() = default;

    virtual Vec3 value(
        double u,
        double v,
        const Vec3& p
    ) const = 0;
};

// Constant-color texture.
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

// Checker pattern evaluated in UV space.
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

// Gradient interpolated along the v coordinate.
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

// Checker pattern evaluated in world-space coordinates.
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