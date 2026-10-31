#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#include "materials/texture.h"

class PerlinNoiseTexture final : public Texture {
public:
    PerlinNoiseTexture(
        const Vec3& color_a,
        const Vec3& color_b,
        double scale = 1.0,
        int octaves = 5,
        std::uint32_t seed = 0
    )
        : color_a_(color_a),
          color_b_(color_b),
          scale_(scale),
          octaves_(octaves),
          seed_(seed) {
        if (!std::isfinite(scale_) || scale_ <= 0.0) {
            throw std::invalid_argument(
                "Perlin texture scale must be finite and positive."
            );
        }

        if (octaves_ < 1 || octaves_ > 12) {
            throw std::invalid_argument(
                "Perlin texture octaves must be in [1, 12]."
            );
        }

        if (!finite_color(color_a_) || !finite_color(color_b_)) {
            throw std::invalid_argument(
                "Perlin texture colors must be finite."
            );
        }
    }

    Vec3 value(
        double,
        double,
        const Vec3& p,
        const TextureSampleContext& context
    ) const override {
        double footprint = 0.0;

        if (context.has_surface_differentials &&
            std::isfinite(context.dpdx.x) &&
            std::isfinite(context.dpdx.y) &&
            std::isfinite(context.dpdx.z) &&
            std::isfinite(context.dpdy.x) &&
            std::isfinite(context.dpdy.y) &&
            std::isfinite(context.dpdy.z)) {
            footprint = std::max(
                context.dpdx.length(),
                context.dpdy.length()
            );
        } else if (
            context.has_ray_cone &&
            std::isfinite(context.cone_width) &&
            context.cone_width >= 0.0
        ) {
            footprint = context.cone_width;
        }

        int filtered_octaves = 0;
        double octave_frequency = scale_;

        for (int octave = 0; octave < octaves_; ++octave) {
            // Always retain the base octave.
            // Only remove higher-frequency octaves when they exceed
            // the estimated sampling footprint.
            if (filtered_octaves > 0 &&
                footprint > 0.0 &&
                footprint * octave_frequency > 1.0) {
                break;
            }

            ++filtered_octaves;
            octave_frequency *= 2.0;
        }

        return evaluate(p, filtered_octaves);
    }

private:
    Vec3 color_a_;
    Vec3 color_b_;
    double scale_;
    int octaves_;
    std::uint32_t seed_;

    static bool finite_color(const Vec3& color) {
        return std::isfinite(color.x) &&
               std::isfinite(color.y) &&
               std::isfinite(color.z);
    }

    static double fade(double t) {
        // Quintic interpolation has continuous first and second
        // derivatives at lattice boundaries.
        return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    }

    static double lerp(double a, double b, double t) {
        return a + t * (b - a);
    }

    static std::uint32_t mix_bits(std::uint32_t value) {
        value ^= value >> 16;
        value *= 0x7feb352dU;
        value ^= value >> 15;
        value *= 0x846ca68bU;
        value ^= value >> 16;
        return value;
    }

    std::uint32_t lattice_hash(
        std::int64_t x,
        std::int64_t y,
        std::int64_t z
    ) const {
        std::uint32_t hash = seed_;

        hash ^= mix_bits(
            static_cast<std::uint32_t>(x) + 0x9e3779b9U
        );

        hash = mix_bits(hash);

        hash ^= mix_bits(
            static_cast<std::uint32_t>(y) + 0x85ebca6bU
        );

        hash = mix_bits(hash);

        hash ^= mix_bits(
            static_cast<std::uint32_t>(z) + 0xc2b2ae35U
        );

        return mix_bits(hash);
    }

    static Vec3 gradient(std::uint32_t hash) {
        constexpr double INV_SQRT_2 =
            0.70710678118654752440;

        const double a = (hash & 1U) ? 1.0 : -1.0;
        const double b = (hash & 2U) ? 1.0 : -1.0;

        switch ((hash >> 2U) % 3U) {
        case 0:
            return Vec3(a, b, 0.0) * INV_SQRT_2;
        case 1:
            return Vec3(a, 0.0, b) * INV_SQRT_2;
        default:
            return Vec3(0.0, a, b) * INV_SQRT_2;
        }
    }

    double noise(const Vec3& p) const {
        const double floor_x = std::floor(p.x);
        const double floor_y = std::floor(p.y);
        const double floor_z = std::floor(p.z);

        const auto x = static_cast<std::int64_t>(floor_x);
        const auto y = static_cast<std::int64_t>(floor_y);
        const auto z = static_cast<std::int64_t>(floor_z);

        const double fx = p.x - floor_x;
        const double fy = p.y - floor_y;
        const double fz = p.z - floor_z;

        const double u = fade(fx);
        const double v = fade(fy);
        const double w = fade(fz);

        double corners[2][2][2]{};

        for (int dz = 0; dz <= 1; ++dz) {
            for (int dy = 0; dy <= 1; ++dy) {
                for (int dx = 0; dx <= 1; ++dx) {
                    const Vec3 g = gradient(
                        lattice_hash(
                            x + dx,
                            y + dy,
                            z + dz
                        )
                    );

                    corners[dz][dy][dx] = dot(
                        g,
                        Vec3(
                            fx - dx,
                            fy - dy,
                            fz - dz
                        )
                    );
                }
            }
        }

        const double z0y0 = lerp(
            lerp(corners[0][0][0], corners[0][0][1], u),
            lerp(corners[0][1][0], corners[0][1][1], u),
            v
        );

        const double z0y1 = lerp(
            lerp(corners[1][0][0], corners[1][0][1], u),
            lerp(corners[1][1][0], corners[1][1][1], u),
            v
        );

        // Scale and clamp the signed gradient noise into [0, 1].
        return std::clamp(
            0.5 + 0.5 * lerp(z0y0, z0y1, w),
            0.0,
            1.0
        );
    }

    Vec3 evaluate(const Vec3& p, int octave_count) const {
        if (!finite_color(p)) {
            return color_a_;
        }

        double frequency = scale_;
        double amplitude = 1.0;
        double weighted_noise = 0.0;
        double total_weight = 0.0;

        for (int octave = 0; octave < octave_count; ++octave) {
            const Vec3 sample_point = p * frequency;

            weighted_noise +=
                amplitude * noise(sample_point);

            total_weight += amplitude;

            frequency *= 2.0;
            amplitude *= 0.5;
        }

        const double t = total_weight > 0.0
            ? weighted_noise / total_weight
            : 0.5;

        return (1.0 - t) * color_a_ + t * color_b_;
    }
};
