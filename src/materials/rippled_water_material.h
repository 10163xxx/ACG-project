#pragma once

#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "materials/material_decorator.h"

// Wraps a base material and perturbs its shading normal with the
// analytic gradient of a sum of traveling sine waves. Used for pond
// water: the ripple field bends reflection and refraction the way
// wind-ruffled surfaces do, without geometric displacement.
class RippledWaterMaterial final : public MaterialDecorator {
public:
    struct Wave {
        // Horizontal travel direction; normalized in the constructor.
        Vec3 direction;

        double wavelength;
        double amplitude;
        double phase;
    };

    RippledWaterMaterial(
        std::shared_ptr<const Material> base_material,
        std::vector<Wave> waves,
        double strength = 1.0
    )
        : MaterialDecorator(std::move(base_material)),
          waves_(std::move(waves)),
          strength_(strength) {
        if (waves_.empty()) {
            throw std::invalid_argument(
                "RippledWaterMaterial requires at least one wave."
            );
        }

        for (Wave& wave : waves_) {
            if (!std::isfinite(wave.wavelength) ||
                wave.wavelength <= 0.0 ||
                !std::isfinite(wave.amplitude) ||
                wave.amplitude < 0.0 ||
                !std::isfinite(wave.phase)) {
                throw std::invalid_argument(
                    "Ripple waves need a positive wavelength, a "
                    "non-negative amplitude and a finite phase."
                );
            }

            const double length_squared =
                wave.direction.length_squared();

            if (!std::isfinite(length_squared) ||
                length_squared < 1e-20) {
                throw std::invalid_argument(
                    "Ripple wave direction must be a finite "
                    "horizontal vector."
                );
            }

            wave.direction =
                wave.direction / std::sqrt(length_squared);
        }

        if (!std::isfinite(strength_)) {
            throw std::invalid_argument(
                "Ripple strength must be finite."
            );
        }
    }

    Vec3 shading_normal(
        const Ray&,
        const HitRecord& hit
    ) const override {
        return mapped_normal(hit);
    }

    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        HitRecord mapped_hit = hit;
        mapped_hit.shading_normal = mapped_normal(hit);
        mapped_hit.has_shading_normal = true;
        mapped_hit.has_resolved_shading_normal = true;

        return base_material_->evaluate_bsdf(
            mapped_hit,
            outgoing_direction,
            incoming_direction
        );
    }

    double scattering_pdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        HitRecord mapped_hit = hit;
        mapped_hit.shading_normal = mapped_normal(hit);
        mapped_hit.has_shading_normal = true;
        mapped_hit.has_resolved_shading_normal = true;

        return base_material_->scattering_pdf(
            mapped_hit,
            outgoing_direction,
            incoming_direction
        );
    }

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered,
        ScatterSampleInfo& info
    ) const override {
        HitRecord mapped_hit = hit;
        mapped_hit.shading_normal = mapped_normal(hit);
        mapped_hit.has_shading_normal = true;
        mapped_hit.has_resolved_shading_normal = true;

        return base_material_->scatter(
            incoming,
            mapped_hit,
            attenuation,
            scattered,
            info
        );
    }

private:
    std::vector<Wave> waves_;
    double strength_;

    // Surface gradient of the ripple height field at a world-space
    // point. Each wave contributes its slope along its travel
    // direction; the base normal then tilts against the gradient.
    Vec3 ripple_gradient(const Vec3& point) const {
        Vec3 gradient;

        for (const Wave& wave : waves_) {
            const double wavenumber =
                2.0 * 3.14159265358979323846 / wave.wavelength;

            const double phase =
                wavenumber *
                    (point.x * wave.direction.x
                     + point.z * wave.direction.z)
                + wave.phase;

            const double slope =
                wavenumber * wave.amplitude * std::cos(phase);

            gradient.x += slope * wave.direction.x;
            gradient.z += slope * wave.direction.z;
        }

        return gradient;
    }

    Vec3 mapped_normal(const HitRecord& hit) const {
        const Vec3 geometric_normal = normalize_or(
            hit.normal,
            Vec3(0.0, 1.0, 0.0)
        );

        Vec3 world_normal = normalize_or(
            geometric_normal - strength_ * ripple_gradient(hit.point),
            geometric_normal
        );

        // Keep the perturbed normal in the geometric hemisphere.
        const double hemisphere_dot =
            dot(world_normal, geometric_normal);

        if (hemisphere_dot < 0.0) {
            world_normal =
                world_normal -
                hemisphere_dot * geometric_normal;

            world_normal = normalize_or(
                world_normal,
                geometric_normal
            );
        }

        return world_normal;
    }

    static Vec3 normalize_or(
        const Vec3& value,
        const Vec3& fallback
    ) {
        const double length_squared = value.length_squared();

        if (!std::isfinite(length_squared) ||
            length_squared < 1e-20) {
            return fallback;
        }

        return value / std::sqrt(length_squared);
    }
};
