#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "materials/material.h"
#include "core/random.h"

class Dielectric : public Material {
public:
    explicit Dielectric(
        double index_of_refraction,
        const Vec3& tint = Vec3(1.0, 1.0, 1.0)
    )
        : ir_(index_of_refraction),
          tint_(
              std::clamp(tint.x, 0.0, 1.0),
              std::clamp(tint.y, 0.0, 1.0),
              std::clamp(tint.z, 0.0, 1.0)
          ) {
        if (!std::isfinite(ir_) || ir_ <= 0.0) {
            throw std::invalid_argument(
                "Index of refraction must be positive and finite."
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
        attenuation = tint_;

        const double refraction_ratio =
            hit.front_face ? (1.0 / ir_) : ir_;

        const Vec3 unit_direction =
            normalize(incoming.direction);

        const double cos_theta =
            std::min(
                dot(-unit_direction, hit.normal),
                1.0
            );

        const double sin_theta =
            std::sqrt(std::max(
                0.0,
                1.0 - cos_theta * cos_theta
            ));

        const bool cannot_refract =
            refraction_ratio * sin_theta > 1.0;

        Vec3 direction;

        if (
            cannot_refract ||
            reflectance(cos_theta, refraction_ratio)
                > Random::random_double()
        ) {
            direction = reflect(unit_direction, hit.normal);
        } else {
            direction = refract(
                unit_direction,
                hit.normal,
                refraction_ratio
            );
        }

        scattered = Ray(hit.point, direction);
        return true;
    }

    bool blocks_shadow(const HitRecord& hit) const override {
        (void)hit;
        return false;
    }

    double shadow_transmittance(
        const HitRecord& hit,
        const Vec3& ray_direction
    ) const override {
        const double refraction_ratio =
            hit.front_face ? (1.0 / ir_) : ir_;

        const Vec3 direction = normalize(ray_direction);

        const double cos_theta = std::clamp(
            std::abs(dot(direction, hit.normal)),
            0.0,
            1.0
        );

        const double fresnel =
            reflectance(cos_theta, refraction_ratio);

        const double tint =
            (tint_.x + tint_.y + tint_.z) / 3.0;

        return (1.0 - fresnel) * tint;
    }

private:
    double ir_;
    Vec3 tint_;

    static double reflectance(
        double cosine,
        double refraction_ratio
    ) {
        double r0 =
            (1.0 - refraction_ratio) /
            (1.0 + refraction_ratio);

        r0 *= r0;

        return r0 + (1.0 - r0)
            * std::pow(1.0 - cosine, 5.0);
    }
};
