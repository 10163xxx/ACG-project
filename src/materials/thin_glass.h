#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "materials/material.h"
#include "core/random.h"

class ThinGlass final : public Material {
public:
    explicit ThinGlass(
        double index_of_refraction = 1.52,
        const Vec3& tint = Vec3(0.96, 0.985, 1.0)
    )
        : ior_(index_of_refraction),
          tint_(
              std::clamp(tint.x, 0.0, 1.0),
              std::clamp(tint.y, 0.0, 1.0),
              std::clamp(tint.z, 0.0, 1.0)
          ) {
        if (!std::isfinite(ior_) || ior_ <= 1.0) {
            throw std::invalid_argument(
                "ThinGlass IOR must be finite and greater than 1."
            );
        }

        if (!std::isfinite(tint.x) ||
            !std::isfinite(tint.y) ||
            !std::isfinite(tint.z)) {
            throw std::invalid_argument(
                "ThinGlass tint must be finite."
            );
        }
    }

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
        info.ray_cone_spread_add = 0.0;

        const Vec3 direction = normalize(incoming.direction);
        const Vec3 normal = normalize(hit.normal);

        const double cosine = std::clamp(
            std::abs(dot(direction, normal)),
            0.0,
            1.0
        );

        const double r0 =
            (ior_ - 1.0) / (ior_ + 1.0);

        const double r0_squared = r0 * r0;

        const double fresnel =
            r0_squared +
            (1.0 - r0_squared) *
            std::pow(1.0 - cosine, 5.0);

        if (Random::random_double() < fresnel) {
            attenuation = Vec3(1.0, 1.0, 1.0);

            scattered = Ray(
                hit.point,
                reflect(direction, normal)
            );
        } else {
            attenuation = tint_;
            info.is_transmission = true;

            // A thin architectural pane does not define
            // a closed refractive medium.
            scattered = Ray(hit.point, direction);
        }

        return true;
    }

    bool blocks_shadow(const HitRecord& hit) const override {
        (void)hit;
        return false;
    }

    Vec3 shadow_transmittance(
        const HitRecord& hit,
        const Vec3& ray_direction,
        double incident_ior,
        double transmitted_ior
    ) const override {
        (void)incident_ior;
        (void)transmitted_ior;

        const Vec3 direction = normalize(ray_direction);
        const Vec3 normal = normalize(hit.normal);

        const double cosine = std::clamp(
            std::abs(dot(direction, normal)),
            0.0,
            1.0
        );

        const double r0 =
            (ior_ - 1.0) / (ior_ + 1.0);

        const double r0_squared = r0 * r0;

        const double fresnel =
            r0_squared +
            (1.0 - r0_squared) *
            std::pow(1.0 - cosine, 5.0);

        return tint_ * (1.0 - fresnel);
    }

private:
    double ior_;
    Vec3 tint_;
};