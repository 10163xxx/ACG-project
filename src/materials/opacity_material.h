#pragma once

#include <algorithm>
#include <memory>
#include <stdexcept>

#include "materials/material.h"
#include "core/random.h"

class OpacityMaterial final : public Material {
public:
    OpacityMaterial(
        std::shared_ptr<const Material> base_material,
        double opacity
    )
        : base_material_(std::move(base_material)),
          opacity_(std::clamp(opacity, 0.0, 1.0)) {
        if (!base_material_) {
            throw std::invalid_argument(
                "OpacityMaterial requires a base material."
            );
        }
    }

    Vec3 emitted(const HitRecord& hit) const override {
        return base_material_->emitted(hit) * opacity_;
    }

    bool is_emissive() const override {
        return opacity_ > 0.0 && base_material_->is_emissive();
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
        ) * opacity_;
    }

    double scattering_pdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        return base_material_->scattering_pdf(
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
        // A partially transparent surface cannot be treated as a fully
        // opaque blocker by the current direct-lighting shadow-ray code.
        // Disable NEE for this wrapper; path sampling handles both branches.
        return opacity_ >= 1.0 &&
               base_material_->supports_direct_lighting();
    }

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered
    ) const override {
        if (opacity_ <= 0.0) {
            attenuation = Vec3(1.0, 1.0, 1.0);
            scattered = Ray(hit.point, incoming.direction);
            return true;
        }

        if (opacity_ < 1.0 &&
            Random::random_double() >= opacity_) {
            // Alpha/transparency branch: continue in the original direction.
            attenuation = Vec3(1.0, 1.0, 1.0);
            scattered = Ray(hit.point, incoming.direction);
            return true;
        }

        return base_material_->scatter(
            incoming,
            hit,
            attenuation,
            scattered
        );
    }

private:
    std::shared_ptr<const Material> base_material_;
    double opacity_;
};
