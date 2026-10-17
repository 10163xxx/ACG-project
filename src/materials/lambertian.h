#pragma once

#include <memory>
#include <utility>

#include "materials/material.h"
#include "materials/texture.h"
#include "core/random.h"

class Lambertian : public Material {
public:
    explicit Lambertian(const Vec3& albedo)
        : texture_(std::make_shared<SolidColor>(albedo)) {}

    explicit Lambertian(std::shared_ptr<const Texture> texture)
        : texture_(std::move(texture)) {}

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered
    ) const override {
        Vec3 scatter_direction =
            hit.normal + Random::random_unit_vector();

        if (scatter_direction.near_zero()) {
            scatter_direction = hit.normal;
        }

        scattered = Ray(hit.point, scatter_direction);

        attenuation = texture_->value(hit.u, hit.v, hit.point);

        return true;
    }

    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        const double cos_outgoing =
            dot(hit.normal, outgoing_direction);

        const double cos_incoming =
            dot(hit.normal, incoming_direction);

        if (cos_outgoing <= 0.0 || cos_incoming <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        constexpr double PI = 3.14159265358979323846;

        const Vec3 albedo =
            texture_->value(hit.u, hit.v, hit.point);

        return albedo / PI;
    }

private:
    std::shared_ptr<const Texture> texture_;
};
