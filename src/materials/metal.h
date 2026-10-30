#pragma once

#include <algorithm>

#include "materials/material.h"
#include "materials/shading_normal.h"
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
        Ray& scattered,
        ScatterSampleInfo& info
    ) const override {
        info.is_straight_through = false;

        const Vec3 geometric_normal =
            normalize(hit.normal);

        const Vec3 shading_normal =
            normalize(hit.get_shading_normal());

        const Vec3 unit_direction =
            normalize(incoming.direction);

        const Vec3 outgoing =
            -unit_direction;

        if (!shading_normal::valid_reflection_outgoing_direction(
                geometric_normal,
                shading_normal,
                outgoing)) {
            return false;
        }

        const Vec3 reflected =
            reflect(unit_direction, shading_normal);

        const Vec3 direction =
            reflected
            + fuzz_ * Random::random_in_unit_sphere();

        if (direction.near_zero()) {
            return false;
        }

        const Vec3 incoming_direction =
            normalize(direction);

        if (!shading_normal::valid_reflection_directions(
                geometric_normal,
                shading_normal,
                outgoing,
                incoming_direction)) {
            return false;
        }

        scattered = Ray(hit.point, direction);
        attenuation = albedo_;

        return true;
    }

private:
    Vec3 albedo_;
    double fuzz_;
};
