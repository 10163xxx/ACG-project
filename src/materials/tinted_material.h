#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include "materials/material.h"
#include "materials/material_decorator.h"

// Wraps a base material and multiplies its response by a constant
// color. Used to give every plant instance a slightly different
// tone so repeated models do not read as clones.
class TintedMaterial final : public MaterialDecorator {
public:
    TintedMaterial(
        std::shared_ptr<const Material> base_material,
        const Vec3& tint
    )
        : MaterialDecorator(std::move(base_material)),
          tint_(
              std::clamp(tint.x, 0.0, 2.0),
              std::clamp(tint.y, 0.0, 2.0),
              std::clamp(tint.z, 0.0, 2.0)
          ) {
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
        ) * tint_;
    }

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered,
        ScatterSampleInfo& info
    ) const override {
        if (!base_material_->scatter(
                incoming,
                hit,
                attenuation,
                scattered,
                info
            )) {
            return false;
        }

        attenuation = attenuation * tint_;
        return true;
    }

private:
    Vec3 tint_;
};
