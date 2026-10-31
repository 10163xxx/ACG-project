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
        const Vec3& tint = Vec3(1.0, 1.0, 1.0),
        const Vec3& absorption = Vec3(0.0, 0.0, 0.0)
    )
        : ir_(index_of_refraction),
          tint_(
              std::clamp(tint.x, 0.0, 1.0),
              std::clamp(tint.y, 0.0, 1.0),
              std::clamp(tint.z, 0.0, 1.0)
          ),
          absorption_(absorption) {
        if (!std::isfinite(ir_) || ir_ <= 0.0) {
            throw std::invalid_argument(
                "Index of refraction must be positive and finite."
            );
        }

        if (
            !std::isfinite(absorption_.x) ||
            !std::isfinite(absorption_.y) ||
            !std::isfinite(absorption_.z) ||
            absorption_.x < 0.0 ||
            absorption_.y < 0.0 ||
            absorption_.z < 0.0
        ) {
            throw std::invalid_argument(
                "Absorption coefficients must be finite and non-negative."
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

        double refraction_ratio;

        if (std::isfinite(info.incident_ior) &&
            std::isfinite(info.transmitted_ior) &&
            info.incident_ior > 0.0 &&
            info.transmitted_ior > 0.0) {
            refraction_ratio =
                info.incident_ior / info.transmitted_ior;
        } else {
            // Fallback for callers that do not provide
            // the surrounding medium state.
            refraction_ratio =
                hit.front_face ? (1.0 / ir_) : ir_;
        }

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

        info.is_transmission = false;

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

            info.is_transmission = true;
        }

        scattered = Ray(hit.point, direction);
        return true;
    }

    bool blocks_shadow(const HitRecord& hit) const override {
        (void)hit;
        return false;
    }

    Vec3 shadow_absorption_coefficient() const override {
        return absorption_;
    }

    double index_of_refraction() const override {
        return ir_;
    }

    bool is_medium_boundary() const override {
        return true;
    }

    Vec3 shadow_transmittance(
        const HitRecord& hit,
        const Vec3& ray_direction,
        double incident_ior,
        double transmitted_ior
    ) const override {
        if (!std::isfinite(incident_ior) ||
            !std::isfinite(transmitted_ior) ||
            incident_ior <= 0.0 ||
            transmitted_ior <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const double refraction_ratio =
            incident_ior / transmitted_ior;

        const Vec3 direction = normalize(ray_direction);

        const double cos_theta = std::clamp(
            std::abs(dot(direction, hit.normal)),
            0.0,
            1.0
        );

        const double fresnel =
            fresnel_reflectance(
                cos_theta,
                refraction_ratio
            );

        return tint_ * (1.0 - fresnel);
    }

private:
    double ir_;
    Vec3 tint_;
    Vec3 absorption_;

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

    static double fresnel_reflectance(
        double cos_theta_i,
        double eta_i_over_eta_t
    ) {
        const double sin_theta_i = std::sqrt(
            std::max(
                0.0,
                1.0 - cos_theta_i * cos_theta_i
            )
        );

        const double sin_theta_t =
            eta_i_over_eta_t * sin_theta_i;

        // Total internal reflection.
        if (sin_theta_t >= 1.0) {
            return 1.0;
        }

        const double cos_theta_t = std::sqrt(
            std::max(
                0.0,
                1.0 - sin_theta_t * sin_theta_t
            )
        );

        const double rs_numerator =
            eta_i_over_eta_t * cos_theta_i
            - cos_theta_t;

        const double rs_denominator =
            eta_i_over_eta_t * cos_theta_i
            + cos_theta_t;

        const double rp_numerator =
            cos_theta_i
            - eta_i_over_eta_t * cos_theta_t;

        const double rp_denominator =
            cos_theta_i
            + eta_i_over_eta_t * cos_theta_t;

        const double rs =
            rs_numerator / rs_denominator;

        const double rp =
            rp_numerator / rp_denominator;

        return std::clamp(
            0.5 * (rs * rs + rp * rp),
            0.0,
            1.0
        );
    }
};
