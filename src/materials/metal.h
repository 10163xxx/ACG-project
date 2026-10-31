#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "materials/material.h"
#include "materials/shading_normal.h"
#include "materials/principled.h"
#include "core/random.h"

class Metal : public Material {
public:
    Metal(const Vec3& albedo, double fuzz = 0.0)
        : albedo_(albedo),
          fuzz_(validated_fuzz(fuzz)),
          rough_bsdf_(
              albedo,
              1.0,
              std::max(fuzz_, 0.045)
          ) {}

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered,
        ScatterSampleInfo& info
    ) const override {
        if (fuzz_ > 0.0) {
            return rough_bsdf_.scatter(
                incoming,
                hit,
                attenuation,
                scattered,
                info
            );
        }

        info.is_straight_through = false;
        info.is_transmission = false;

        info.ray_cone_spread_scale = 1.0;
        info.ray_cone_spread_add = 0.0;

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

        if (reflected.near_zero()) {
            return false;
        }

        const Vec3 incoming_direction =
            normalize(reflected);

        if (!shading_normal::valid_reflection_directions(
                geometric_normal,
                shading_normal,
                outgoing,
                incoming_direction)) {
            return false;
        }

        scattered = Ray(hit.point, reflected);
        attenuation = albedo_;

        return true;
    }

    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        if (fuzz_ <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        return rough_bsdf_.evaluate_bsdf(
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
        if (fuzz_ <= 0.0) {
            return 0.0;
        }

        return rough_bsdf_.scattering_pdf(
            hit,
            outgoing_direction,
            incoming_direction
        );
    }

    bool supports_direct_lighting() const override {
        return fuzz_ > 0.0;
    }

private:
    Vec3 albedo_;
    double fuzz_;
    PrincipledBSDF rough_bsdf_;

    static double validated_fuzz(double fuzz) {
        if (!std::isfinite(fuzz)) {
            throw std::invalid_argument(
                "Metal fuzz must be finite."
            );
        }

        return std::clamp(fuzz, 0.0, 1.0);
    }
};