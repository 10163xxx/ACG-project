#pragma once

#include <cmath>

#include "materials/material.h"
#include "core/random.h"

class Isotropic final : public Material {
public:
    explicit Isotropic(const Vec3& albedo)
        : albedo_(albedo) {}

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered,
        ScatterSampleInfo& info
    ) const override {
        info.is_straight_through = false;
        info.is_transmission = false;

        info.ray_cone_spread_scale = 1.0;
        info.ray_cone_spread_add = 1.0;

        (void)incoming;

        constexpr double PI = 3.14159265358979323846;

        const double z =
            Random::random_double(-1.0, 1.0);

        const double phi =
            2.0 * PI * Random::random_double();

        const double radius =
            std::sqrt(std::max(0.0, 1.0 - z * z));

        const Vec3 direction(
            radius * std::cos(phi),
            radius * std::sin(phi),
            z
        );

        scattered = Ray(hit.point, direction);
        attenuation = albedo_;

        return true;
    }

    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        (void)hit;
        (void)outgoing_direction;
        (void)incoming_direction;

        constexpr double PI = 3.14159265358979323846;

        return albedo_ / (4.0 * PI);
    }

    double scattering_pdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        (void)hit;
        (void)outgoing_direction;
        (void)incoming_direction;

        constexpr double PI = 3.14159265358979323846;

        return 1.0 / (4.0 * PI);
    }

private:
    Vec3 albedo_;
};
