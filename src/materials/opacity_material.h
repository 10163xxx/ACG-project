#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

#include "materials/material.h"
#include "materials/texture.h"
#include "core/random.h"

class OpacityMaterial final : public Material {
public:
    OpacityMaterial(
        std::shared_ptr<const Material> base_material,
        double opacity,
        std::shared_ptr<const Texture> opacity_texture = nullptr
    )
        : base_material_(std::move(base_material)),
          opacity_(std::clamp(opacity, 0.0, 1.0)),
          opacity_texture_(std::move(opacity_texture)) {
        if (!base_material_) {
            throw std::invalid_argument(
                "OpacityMaterial requires a base material."
            );
        }

        if (!std::isfinite(opacity)) {
            throw std::invalid_argument(
                "Opacity must be finite."
            );
        }
    }

    Vec3 emitted(const HitRecord& hit) const override {
        return base_material_->emitted(hit) * opacity_at(hit);
    }

    bool is_emissive() const override {
        return opacity_ > 0.0 &&
               base_material_->is_emissive();
    }

    Vec3 shadow_absorption_coefficient() const override {
        return base_material_->shadow_absorption_coefficient();
    }

    double index_of_refraction() const override {
        return base_material_->index_of_refraction();
    }

    bool blocks_shadow(const HitRecord& hit) const override {
        return opacity_at(hit) >= 1.0
            && base_material_->blocks_shadow(hit);
    }

    Vec3 shadow_transmittance(
        const HitRecord& hit,
        const Vec3& ray_direction
    ) const override {
        const double opacity = opacity_at(hit);

        const Vec3 hole_transmittance(
            1.0 - opacity,
            1.0 - opacity,
            1.0 - opacity
        );

        if (opacity <= 0.0) {
            return Vec3(1.0, 1.0, 1.0);
        }

        Vec3 base_transmittance(0.0, 0.0, 0.0);

        if (!base_material_->blocks_shadow(hit)) {
            base_transmittance =
                base_material_->shadow_transmittance(
                    hit,
                    ray_direction
                );
        }

        return hole_transmittance
            + opacity * base_transmittance;
    }

    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        return base_material_->evaluate_bsdf(
            hit,
            outgoing_direction,
            incoming_direction
        ) * opacity_at(hit);
    }

    double scattering_pdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        return opacity_at(hit)
            * base_material_->scattering_pdf(
                hit,
                outgoing_direction,
                incoming_direction
            );
    }

    Vec3 shading_normal(
        const Ray& incoming,
        const HitRecord& hit
    ) const override {
        return base_material_->shading_normal(incoming, hit);
    }

    bool supports_direct_lighting() const override {
        return base_material_->supports_direct_lighting();
    }

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered,
        ScatterSampleInfo& info
    ) const override {
        const double opacity = opacity_at(hit);

        if (opacity <= 0.0 ||
            Random::random_double() >= opacity) {
            info.is_straight_through = true;

            attenuation = Vec3(1.0, 1.0, 1.0);
            scattered = Ray(hit.point, incoming.direction);

            return true;
        }

        info.is_straight_through = false;

        return base_material_->scatter(
            incoming,
            hit,
            attenuation,
            scattered,
            info
        );
    }

private:
    std::shared_ptr<const Material> base_material_;
    double opacity_;
    std::shared_ptr<const Texture> opacity_texture_;

    double opacity_at(const HitRecord& hit) const {
        if (!opacity_texture_) {
            return opacity_;
        }

        const Vec3 color = opacity_texture_->value_with_cone(
            hit.u,
            hit.v,
            hit.point,
            hit.dudx,
            hit.dvdx,
            hit.dudy,
            hit.dvdy,
            hit.has_uv_differentials,
            hit.ray_cone_width,
            hit.has_ray_cone,
            hit.dpdu,
            hit.dpdv,
            hit.has_uv_parameterization
        );

        // MTL map_d is interpreted as a grayscale opacity map:
        // white is opaque and black is transparent.
        const double map_opacity =
            0.2126 * color.x +
            0.7152 * color.y +
            0.0722 * color.z;

        if (!std::isfinite(map_opacity)) {
            return 0.0;
        }

        return opacity_ * std::clamp(
            map_opacity,
            0.0,
            1.0
        );
    }
};
