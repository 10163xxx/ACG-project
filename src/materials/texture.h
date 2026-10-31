#pragma once

#include <cmath>
#include <algorithm>
#include <stdexcept>

#include "../core/vec3.h"

struct TextureSampleContext {
    double dudx = 0.0;
    double dvdx = 0.0;
    double dudy = 0.0;
    double dvdy = 0.0;

    bool has_uv_differentials = false;

    // World-space surface differentials per screen-space pixel.
    Vec3 dpdx;
    Vec3 dpdy;
    bool has_surface_differentials = false;

    double cone_width = 0.0;
    bool has_ray_cone = false;

    Vec3 dpdu;
    Vec3 dpdv;
    bool has_uv_parameterization = false;

    bool has_valid_uv_differentials() const {
        if (!has_uv_differentials ||
            !std::isfinite(dudx) ||
            !std::isfinite(dvdx) ||
            !std::isfinite(dudy) ||
            !std::isfinite(dvdy)) {
            return false;
        }

        const double width_u =
            std::abs(dudx) + std::abs(dudy);

        const double width_v =
            std::abs(dvdx) + std::abs(dvdy);

        return std::isfinite(width_u) &&
               std::isfinite(width_v);
    }

    bool get_uv_footprint(
        double& width_u,
        double& width_v
    ) const {
        width_u = 0.0;
        width_v = 0.0;

        if (has_valid_uv_differentials()) {
            width_u =
                std::abs(dudx) + std::abs(dudy);

            width_v =
                std::abs(dvdx) + std::abs(dvdy);

            return true;
        }

        if (!has_ray_cone ||
            !has_uv_parameterization ||
            !std::isfinite(cone_width) ||
            cone_width < 0.0) {
            return false;
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
            determinant <= 1e-12 * g00 * g11) {
            return false;
        }

        width_u =
            cone_width * std::sqrt(g11 / determinant);

        width_v =
            cone_width * std::sqrt(g00 / determinant);

        if (!std::isfinite(width_u) ||
            !std::isfinite(width_v)) {
            width_u = 0.0;
            width_v = 0.0;
            return false;
        }

        return true;
    }
};

class Texture {
public:
    virtual ~Texture() = default;

    virtual Vec3 value(
        double u,
        double v,
        const Vec3& p,
        const TextureSampleContext& context
    ) const = 0;
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
        const Vec3&,
        const TextureSampleContext&
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
        const Vec3&,
        const TextureSampleContext& context
    ) const override {
        double width_u = 0.0;
        double width_v = 0.0;

        if (!context.get_uv_footprint(width_u, width_v)) {
            return sample_filtered(u, v, 0.0, 0.0);
        }

        return sample_filtered(
            u,
            v,
            width_u,
            width_v
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
        const Vec3&,
        const TextureSampleContext&
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
          color_b_(color_b) {
        if (!std::isfinite(frequency_) || frequency_ <= 0.0) {
            throw std::invalid_argument(
                "Checker texture frequency must be finite and positive."
            );
        }
    }

    Vec3 value(
        double,
        double,
        const Vec3& p,
        const TextureSampleContext& context
    ) const override {
        double width_x = 0.0;
        double width_y = 0.0;
        double width_z = 0.0;

        if (context.has_surface_differentials &&
            std::isfinite(context.dpdx.x) &&
            std::isfinite(context.dpdx.y) &&
            std::isfinite(context.dpdx.z) &&
            std::isfinite(context.dpdy.x) &&
            std::isfinite(context.dpdy.y) &&
            std::isfinite(context.dpdy.z)) {

            width_x =
                std::abs(context.dpdx.x) +
                std::abs(context.dpdy.x);

            width_y =
                std::abs(context.dpdx.y) +
                std::abs(context.dpdy.y);

            width_z =
                std::abs(context.dpdx.z) +
                std::abs(context.dpdy.z);
        } else if (
            context.has_ray_cone &&
            std::isfinite(context.cone_width) &&
            context.cone_width > 0.0
        ) {
            const double diameter =
                2.0 * context.cone_width;

            width_x = diameter;
            width_y = diameter;
            width_z = diameter;
        }

        if (!std::isfinite(width_x) ||
            !std::isfinite(width_y) ||
            !std::isfinite(width_z)) {
            width_x = 0.0;
            width_y = 0.0;
            width_z = 0.0;
        }

        const double coordinate_x =
            frequency_ * p.x / PI;

        const double coordinate_y =
            frequency_ * p.y / PI;

        const double coordinate_z =
            frequency_ * p.z / PI;

        const double filtered_x =
            average_square_wave(
                coordinate_x,
                frequency_ * width_x / PI
            );

        const double filtered_y =
            average_square_wave(
                coordinate_y,
                frequency_ * width_y / PI
            );

        const double filtered_z =
            average_square_wave(
                coordinate_z,
                frequency_ * width_z / PI
            );

        const double checker_average =
            filtered_x * filtered_y * filtered_z;

        const double weight_a =
            0.5 * (1.0 + checker_average);

        return
            weight_a * color_a_ +
            (1.0 - weight_a) * color_b_;
    }

private:
    static constexpr double PI =
        3.14159265358979323846;

    double frequency_;
    Vec3 color_a_;
    Vec3 color_b_;

    static double square_wave_primitive(double x) {
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
};

class UVNormalTexture final : public Texture {
public:
    explicit UVNormalTexture(double frequency)
        : frequency_(frequency) {}

    Vec3 value(
        double u,
        double v,
        const Vec3&,
        const TextureSampleContext& context
    ) const override {
        double width_u = 0.0;
        double width_v = 0.0;

        if (!context.get_uv_footprint(width_u, width_v)) {
            return evaluate_normal(u, v, 0.0, 0.0);
        }

        return evaluate_normal(
            u,
            v,
            width_u,
            width_v
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