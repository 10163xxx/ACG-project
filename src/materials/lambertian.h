#pragma once

#include "materials/material.h"
#include "core/random.h"

class Lambertian : public Material {
public:
    explicit Lambertian(const Vec3& albedo)
        : albedo_(albedo) {}

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
        attenuation = albedo_;

        return true;
    }

private:
    Vec3 albedo_;
};
