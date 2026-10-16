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

private:
    std::shared_ptr<const Texture> texture_;
};
