#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include "materials/material.h"
#include "materials/texture.h"

class TexturedMaterial final : public Material {
public:
    TexturedMaterial(
        std::shared_ptr<const Material> base_material,
        std::shared_ptr<const Texture> albedo_texture
    )
        : base_material_(std::move(base_material)),
          albedo_texture_(std::move(albedo_texture)) {
        if (!base_material_) {
            throw std::invalid_argument(
                "TexturedMaterial requires a base material."
            );
        }

        if (!albedo_texture_) {
            throw std::invalid_argument(
                "TexturedMaterial requires an albedo texture."
            );
        }
    }

    Vec3 emitted(const HitRecord& hit) const override {
        return base_material_->emitted(hit);
    }

    bool is_emissive() const override {
        return base_material_->is_emissive();
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
        Ray& scattered
    ) const override {
        if (!base_material_->scatter(
                incoming,
                hit,
                attenuation,
                scattered
            )) {
            return false;
        }

        attenuation = attenuation * texture_color(hit);
        return true;
    }

private:
    std::shared_ptr<const Material> base_material_;
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
