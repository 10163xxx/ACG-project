#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include "materials/material.h"
#include "materials/texture.h"
#include "materials/material_decorator.h"

class TexturedMaterial final : public MaterialDecorator {
public:
    TexturedMaterial(
        std::shared_ptr<const Material> base_material,
        std::shared_ptr<const Texture> albedo_texture
    )
        : MaterialDecorator(std::move(base_material)),
          albedo_texture_(std::move(albedo_texture)) {

        if (!albedo_texture_) {
            throw std::invalid_argument(
                "TexturedMaterial requires an albedo texture."
            );
        }
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
        ) * texture_color(hit);
    }

    double scattering_pdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        // The texture changes the BSDF value, not its directional PDF.
        return base_material_->scattering_pdf(
            hit,
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
        if (!base_material_->scatter(
                incoming,
                hit,
                attenuation,
                scattered,
                info
            )) {
            return false;
        }

        if (info.is_straight_through) {
            return true;
        }

        attenuation = attenuation * texture_color(hit);
        return true;
    }

private:
    std::shared_ptr<const Texture> albedo_texture_;

    Vec3 texture_color(const HitRecord& hit) const {
        return albedo_texture_->value_with_cone(
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
    }
};
