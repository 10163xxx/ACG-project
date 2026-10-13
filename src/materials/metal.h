#pragma once

#include <algorithm>

#include "materials/material.h"
#include "core/random.h"

class Metal : public Material {
public:
    Metal(const Vec3& albedo, double fuzz = 0.0)
        : albedo_(albedo),
          fuzz_(std::clamp(fuzz, 0.0, 1.0)) {}

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered
    ) const override {
        const Vec3 unit_direction =
            normalize(incoming.direction);

        const Vec3 reflected =
            reflect(unit_direction, hit.normal);

        const Vec3 direction =
            reflected + fuzz_ * Random::random_in_unit_sphere();

        scattered = Ray(hit.point, direction);
        attenuation = albedo_;

        return dot(scattered.direction, hit.normal) > 0.0;
    }

private:
    Vec3 albedo_;
    double fuzz_;
};
