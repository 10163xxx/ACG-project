#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include "materials/material.h"

class MaterialDecorator : public Material {
public:
    explicit MaterialDecorator(
        std::shared_ptr<const Material> base_material
    )
        : base_material_(std::move(base_material)) {
        if (!base_material_) {
            throw std::invalid_argument(
                "MaterialDecorator requires a base material."
            );
        }
    }

    Vec3 emitted(const HitRecord& hit) const override {
        return base_material_->emitted(hit);
    }

    bool is_emissive() const override {
        return base_material_->is_emissive();
    }

    bool blocks_shadow(const HitRecord& hit) const override {
        return base_material_->blocks_shadow(hit);
    }

    Vec3 shadow_absorption_coefficient() const override {
        return base_material_->shadow_absorption_coefficient();
    }

    double index_of_refraction() const override {
        return base_material_->index_of_refraction();
    }

    bool is_medium_boundary() const override {
        return base_material_->is_medium_boundary();
    }

    Vec3 shadow_transmittance(
        const HitRecord& hit,
        const Vec3& ray_direction
    ) const override {
        return base_material_->shadow_transmittance(
            hit,
            ray_direction
        );
    }

    Vec3 shadow_boundary_transmittance(
        const HitRecord& hit,
        const Vec3& ray_direction,
        double incident_ior,
        double transmitted_ior
    ) const override {
        return base_material_->shadow_boundary_transmittance(
            hit,
            ray_direction,
            incident_ior,
            transmitted_ior
        );
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
        return base_material_->shading_normal(
            incoming,
            hit
        );
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
        return base_material_->scatter(
            incoming,
            hit,
            attenuation,
            scattered,
            info
        );
    }

protected:
    std::shared_ptr<const Material> base_material_;
};