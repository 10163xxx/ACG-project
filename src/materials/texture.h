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
        return sample_filtered(u, v, 0.0, 0.0);
    }

    Vec3 value(
        double u,
        double v,
        const Vec3& p,
        double dudx,
        double dvdx,
        double dudy,
        double dvdy
    ) const override {
        (void)p;

        if (!std::isfinite(dudx) ||
            !std::isfinite(dvdx) ||
            !std::isfinite(dudy) ||
            !std::isfinite(dvdy)) {
            return sample_filtered(u, v, 0.0, 0.0);
        }

        const double width_u =
            std::abs(dudx) + std::abs(dudy);

        const double width_v =
            std::abs(dvdx) + std::abs(dvdy);

        return sample_filtered(u, v, width_u, width_v);
    }

    Vec3 value_with_cone(
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
    ) const override {
        if (has_uv_differentials) {
            return value(
                u, v, p,
                dudx, dvdx, dudy, dvdy
            );
        }

        if (!has_ray_cone ||
            !has_uv_parameterization ||
            !std::isfinite(cone_width) ||
            cone_width < 0.0) {
            return sample_filtered(u, v, 0.0, 0.0);
        }

        const double g00 = dot(dpdu, dpdu);
        const double g01 = dot(dpdu, dpdv);
        const double g11 = dot(dpdv, dpdv);

        const double determinant =
            g00 * g11 - g01 * g01;

        if (!std::isfinite(g00) ||
            !std::isfinite(g01) ||
            !std::isfinite(g11) ||
            !std::isfinite(determinant) ||
            g00 <= 0.0 ||
            g11 <= 0.0 ||
            determinant <= 1e-20) {
            return sample_filtered(u, v, 0.0, 0.0);
        }

        const double width_u =
            cone_width * std::sqrt(g11 / determinant);

        const double width_v =
            cone_width * std::sqrt(g00 / determinant);

        return sample_filtered(
            u, v, width_u, width_v
        );
    }

private:
    double frequency_;
    Vec3 color_a_;
    Vec3 color_b_;

    static double square_wave_primitive(double x) {
        // A periodic primitive of the +/-1 square wave.
        const double period_position =
            x - 2.0 * std::floor(x / 2.0);

        return period_position <= 1.0
            ? period_position
            : 2.0 - period_position;
    }

    static double average_square_wave(
        double coordinate,
        double width
    ) {
        if (!std::isfinite(coordinate) ||
            !std::isfinite(width)) {
            return 0.0;
        }

        if (width <= 1e-8) {
            const auto cell =
                static_cast<long long>(
                    std::floor(coordinate)
                );

            return cell % 2 == 0 ? 1.0 : -1.0;
        }

        const double left =
            coordinate - 0.5 * width;

        const double right =
            coordinate + 0.5 * width;

        return std::clamp(
            (
                square_wave_primitive(right) -
                square_wave_primitive(left)
            ) / width,
            -1.0,
            1.0
        );
    }

    Vec3 sample_filtered(
        double u,
        double v,
        double width_u,
        double width_v
    ) const {
        const double coordinate_u = u * frequency_;
        const double coordinate_v = v * frequency_;

        const double filtered_u =
            average_square_wave(
                coordinate_u,
                std::abs(frequency_) * width_u
            );

        const double filtered_v =
            average_square_wave(
                coordinate_v,
                std::abs(frequency_) * width_v
            );

        // The checker sign is the product of the two 1D
        // square waves. Their rectangular-footprint averages
        // therefore multiply.
        const double checker_average =
            filtered_u * filtered_v;

        const double weight_a =
            0.5 * (1.0 + checker_average);

        return
            weight_a * color_a_ +
            (1.0 - weight_a) * color_b_;
    }
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
        return evaluate_normal(u, v, 0.0, 0.0);
    }

    Vec3 value(
        double u,
        double v,
        const Vec3& p,
        double dudx,
        double dvdx,
        double dudy,
        double dvdy
    ) const override {
        (void)p;

        if (!std::isfinite(dudx) ||
            !std::isfinite(dvdx) ||
            !std::isfinite(dudy) ||
            !std::isfinite(dvdy)) {
            return evaluate_normal(u, v, 0.0, 0.0);
        }

        const double width_u =
            std::abs(dudx) + std::abs(dudy);

        const double width_v =
            std::abs(dvdx) + std::abs(dvdy);

        return evaluate_normal(u, v, width_u, width_v);
    }

    Vec3 value_with_cone(
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
    ) const override {
        if (has_uv_differentials) {
            return value(
                u, v, p,
                dudx, dvdx, dudy, dvdy
            );
        }

        if (!has_ray_cone ||
            !has_uv_parameterization ||
            !std::isfinite(cone_width) ||
            cone_width < 0.0) {
            return evaluate_normal(u, v, 0.0, 0.0);
        }

        const double g00 = dot(dpdu, dpdu);
        const double g01 = dot(dpdu, dpdv);
        const double g11 = dot(dpdv, dpdv);

        const double determinant =
            g00 * g11 - g01 * g01;

        if (!std::isfinite(g00) ||
            !std::isfinite(g01) ||
            !std::isfinite(g11) ||
            !std::isfinite(determinant) ||
            g00 <= 0.0 ||
            g11 <= 0.0 ||
            determinant <= 1e-20) {
            return evaluate_normal(u, v, 0.0, 0.0);
        }

        const double width_u =
            cone_width * std::sqrt(g11 / determinant);

        const double width_v =
            cone_width * std::sqrt(g00 / determinant);

        return evaluate_normal(
            u, v, width_u, width_v
        );
    }

private:
    double frequency_;

    static double sinc(double x) {
        if (std::abs(x) < 1e-6) {
            return 1.0 - x * x / 6.0;
        }

        return std::sin(x) / x;
    }

    Vec3 evaluate_normal(
        double u,
        double v,
        double width_u,
        double width_v
    ) const {
        constexpr double PI = 3.14159265358979323846;

        const double phase_u =
            2.0 * PI * frequency_ * u;

        const double phase_v =
            2.0 * PI * frequency_ * v;

        const double filter_u =
            sinc(
                PI * frequency_ *
                std::abs(width_u)
            );

        const double filter_v =
            sinc(
                PI * frequency_ *
                std::abs(width_v)
            );

        const double nx =
            0.75 * std::sin(phase_u) * filter_u;

        const double ny =
            0.75 * std::sin(phase_v) * filter_v;

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
};