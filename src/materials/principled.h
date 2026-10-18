#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "materials/material.h"
#include "core/random.h"

class PrincipledBSDF final : public Material {
public:
    PrincipledBSDF(
        const Vec3& base_color,
        double metallic,
        double roughness
    )
        : base_color_(base_color),
          metallic_(std::clamp(metallic, 0.0, 1.0)),
          roughness_(std::clamp(roughness, 0.045, 1.0)) {
        if (base_color.x < 0.0 ||
            base_color.y < 0.0 ||
            base_color.z < 0.0) {
            throw std::invalid_argument(
                "Principled base color cannot be negative."
            );
        }
    }

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered
    ) const override {
        const Vec3 n = normalize(hit.normal);
        const Vec3 wo = normalize(-incoming.direction);

        if (dot(n, wo) <= 0.0) {
            return false;
        }

        const double specular_probability =
            0.25 + 0.75 * metallic_;

        Vec3 wi;

        if (Random::random_double()
            < specular_probability) {
            const Vec3 half_vector =
                sample_ggx_half_vector(n);

            wi = normalize(reflect(
                -wo,
                half_vector
            ));
        } else {
            wi = sample_cosine_hemisphere(n);
        }

        if (dot(n, wi) <= 0.0) {
            return false;
        }

        const double pdf =
            scattering_pdf(hit, wo, wi);

        if (pdf <= 1e-12) {
            return false;
        }

        const Vec3 f =
            evaluate_bsdf(hit, wo, wi);

        const double cos_theta =
            std::max(0.0, dot(n, wi));

        attenuation =
            f * (cos_theta / pdf);

        return true;
    }

    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        const Vec3 n = normalize(hit.normal);
        const Vec3 wo = normalize(outgoing_direction);
        const Vec3 wi = normalize(incoming_direction);

        const double cos_o = dot(n, wo);
        const double cos_i = dot(n, wi);

        if (cos_o <= 0.0 || cos_i <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Vec3 half_sum = wo + wi;

        if (half_sum.near_zero()) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Vec3 h = normalize(half_sum);

        const double cos_h =
            std::max(0.0, dot(n, h));

        const double cos_oh =
            std::max(0.0, dot(wo, h));

        if (cos_h <= 0.0 || cos_oh <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const double alpha = roughness_ * roughness_;
        const double alpha_squared = alpha * alpha;

        const double denominator =
            cos_h * cos_h * (alpha_squared - 1.0) + 1.0;

        const double distribution =
            alpha_squared /
            (PI * denominator * denominator);

        const double geometry =
            smith_g1(cos_o, alpha_squared)
            * smith_g1(cos_i, alpha_squared);

        const Vec3 f0 =
            (1.0 - metallic_) * Vec3(0.04, 0.04, 0.04)
            + metallic_ * base_color_;

        const Vec3 fresnel =
            schlick_fresnel(f0, cos_oh);

        const Vec3 specular =
            fresnel * (
                distribution * geometry /
                (4.0 * cos_o * cos_i)
            );

        const Vec3 diffuse_fresnel =
            schlick_fresnel(
                Vec3(0.04, 0.04, 0.04),
                cos_oh
            );

        const Vec3 diffuse =
            (1.0 - metallic_)
            * base_color_
            * (Vec3(1.0, 1.0, 1.0) - diffuse_fresnel)
            / PI;

        return diffuse + specular;
    }

    double scattering_pdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        const Vec3 n = normalize(hit.normal);
        const Vec3 wo = normalize(outgoing_direction);
        const Vec3 wi = normalize(incoming_direction);

        const double cos_o = dot(n, wo);
        const double cos_i = dot(n, wi);

        if (cos_o <= 0.0 || cos_i <= 0.0) {
            return 0.0;
        }

        const double specular_probability =
            0.25 + 0.75 * metallic_;

        const double diffuse_pdf =
            cos_i / PI;

        const Vec3 half_sum = wo + wi;

        if (half_sum.near_zero()) {
            return (1.0 - specular_probability)
                * diffuse_pdf;
        }

        const Vec3 h = normalize(half_sum);
        const double cos_h =
            std::max(0.0, dot(n, h));

        const double cos_oh =
            std::abs(dot(wo, h));

        if (cos_h <= 0.0 || cos_oh <= 1e-12) {
            return (1.0 - specular_probability)
                * diffuse_pdf;
        }

        const double alpha = roughness_ * roughness_;
        const double alpha_squared = alpha * alpha;

        const double denominator =
            cos_h * cos_h * (alpha_squared - 1.0) + 1.0;

        const double distribution =
            alpha_squared /
            (PI * denominator * denominator);

        // Transform the GGX half-vector PDF to a direction PDF.
        const double specular_pdf =
            distribution * cos_h / (4.0 * cos_oh);

        return
            (1.0 - specular_probability) * diffuse_pdf
            + specular_probability * specular_pdf;
    }

    bool supports_direct_lighting() const override {
        return true;
    }

private:
    static constexpr double PI =
        3.14159265358979323846;

    Vec3 base_color_;
    double metallic_;
    double roughness_;

    static double smith_g1(
        double cos_theta,
        double alpha_squared
    ) {
        if (cos_theta <= 0.0) {
            return 0.0;
        }

        const double cos_squared =
            cos_theta * cos_theta;

        return 2.0 * cos_theta /
            (cos_theta + std::sqrt(
                alpha_squared
                + (1.0 - alpha_squared) * cos_squared
            ));
    }

    static Vec3 schlick_fresnel(
        const Vec3& f0,
        double cos_theta
    ) {
        const double m =
            std::pow(
                std::clamp(1.0 - cos_theta, 0.0, 1.0),
                5.0
            );

        return f0 + (Vec3(1.0, 1.0, 1.0) - f0) * m;
    }

    static Vec3 sample_cosine_hemisphere(
        const Vec3& normal
    ) {
        const double r1 = Random::random_double();
        const double r2 = Random::random_double();

        const double phi = 2.0 * PI * r1;
        const double radius = std::sqrt(r2);

        const double x = radius * std::cos(phi);
        const double y = radius * std::sin(phi);
        const double z = std::sqrt(1.0 - r2);

        const Vec3 helper =
            std::abs(normal.x) > 0.9
                ? Vec3(0.0, 1.0, 0.0)
                : Vec3(1.0, 0.0, 0.0);

        const Vec3 tangent =
            normalize(cross(helper, normal));

        const Vec3 bitangent =
            cross(normal, tangent);

        return normalize(
            x * tangent
            + y * bitangent
            + z * normal
        );
    }

    Vec3 sample_ggx_half_vector(
        const Vec3& normal
    ) const {
        const double r1 = Random::random_double();
        const double r2 = Random::random_double();

        const double alpha = roughness_ * roughness_;
        const double alpha_squared = alpha * alpha;

        const double phi = 2.0 * PI * r1;

        const double cos_theta =
            std::sqrt(
                (1.0 - r2) /
                (1.0 + (alpha_squared - 1.0) * r2)
            );

        const double sin_theta =
            std::sqrt(
                std::max(0.0, 1.0 - cos_theta * cos_theta)
            );

        const Vec3 local(
            sin_theta * std::cos(phi),
            sin_theta * std::sin(phi),
            cos_theta
        );

        const Vec3 helper =
            std::abs(normal.x) > 0.9
                ? Vec3(0.0, 1.0, 0.0)
                : Vec3(1.0, 0.0, 0.0);

        const Vec3 tangent =
            normalize(cross(helper, normal));

        const Vec3 bitangent =
            cross(normal, tangent);

        return normalize(
            local.x * tangent
            + local.y * bitangent
            + local.z * normal
        );
    }
};
