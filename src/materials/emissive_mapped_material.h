#pragma once

#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

#include "materials/material.h"
#include "materials/texture.h"

class EmissiveMappedMaterial final : public Material {
public:
    EmissiveMappedMaterial(
        std::shared_ptr<const Material> base_material,
        const Vec3& emission,
        std::shared_ptr<const Texture> emission_texture = nullptr
    )
        : base_material_(std::move(base_material)),
          emission_(emission),
          emission_texture_(std::move(emission_texture)) {
        if (!base_material_) {
            throw std::invalid_argument(
                "EmissiveMappedMaterial requires a base material."
            );
        }

        if (!std::isfinite(emission_.x) ||
            !std::isfinite(emission_.y) ||
            !std::isfinite(emission_.z) ||
            emission_.x < 0.0 ||
            emission_.y < 0.0 ||
            emission_.z < 0.0) {
            throw std::invalid_argument(
                "Emission must be finite and non-negative."
            );
        }
    }

    Vec3 emitted(const HitRecord& hit) const override {
        if (!hit.front_face) {
            return base_material_->emitted(hit);
        }

        Vec3 radiance = base_material_->emitted(hit);

        if (emission_texture_) {
            const Vec3 texture_color =
                emission_texture_->value_with_cone(
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

            radiance = radiance + emission_ * texture_color;
        } else {
            radiance = radiance + emission_;
        }

        return radiance;
    }

    bool is_emissive() const override {
        return base_material_->is_emissive() ||
               emission_.x > 0.0 ||
               emission_.y > 0.0 ||
               emission_.z > 0.0;
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
        );
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
        return base_material_->supports_direct_lighting();
    }

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered
    ) const override {
        return base_material_->scatter(
            incoming,
            hit,
            attenuation,
            scattered
        );
    }

private:
    std::shared_ptr<const Material> base_material_;
    Vec3 emission_;
    std::shared_ptr<const Texture> emission_texture_;
};
