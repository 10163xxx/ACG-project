#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <memory>

#include "materials/material.h"
#include "core/random.h"
#include "materials/texture.h"
#include "materials/shading_normal.h"
#include "materials/texture_sample_context.h"

class PrincipledBSDF final : public Material {
public:
    PrincipledBSDF(
        const Vec3& base_color,
        double metallic,
        double roughness,
        const Vec3& dielectric_f0 = Vec3(0.04, 0.04, 0.04),
        std::shared_ptr<const Texture> metallic_texture = nullptr,
        std::shared_ptr<const Texture> roughness_texture = nullptr
    )
        : base_color_(base_color),
          metallic_(std::clamp(metallic, 0.0, 1.0)),
          roughness_(std::clamp(roughness, 0.045, 1.0)),
          dielectric_f0_(
              std::clamp(dielectric_f0.x, 0.0, 1.0),
              std::clamp(dielectric_f0.y, 0.0, 1.0),
              std::clamp(dielectric_f0.z, 0.0, 1.0)),
          metallic_texture_(std::move(metallic_texture)),
          roughness_texture_(std::move(roughness_texture))
        {
        if (!std::isfinite(base_color.x) ||
            !std::isfinite(base_color.y) ||
            !std::isfinite(base_color.z)) {
            throw std::invalid_argument(
                "Principled base color must be finite."
            );
        }

        if (!std::isfinite(metallic) ||
            !std::isfinite(roughness)) {
            throw std::invalid_argument(
                "Principled metallic and roughness must be finite."
            );
        }

        if (!std::isfinite(dielectric_f0.x) ||
            !std::isfinite(dielectric_f0.y) ||
            !std::isfinite(dielectric_f0.z)) {
            throw std::invalid_argument(
                "Principled dielectric F0 must be finite."
            );
        }

        if (base_color.x < 0.0 ||
            base_color.x > 1.0 ||
            base_color.y < 0.0 ||
            base_color.y > 1.0 ||
            base_color.z < 0.0 ||
            base_color.z > 1.0) {
            throw std::invalid_argument(
                "Principled base color must be in [0, 1]."
            );
        }

        if (dielectric_f0.x < 0.0 ||
            dielectric_f0.y < 0.0 ||
            dielectric_f0.z < 0.0) {
            throw std::invalid_argument(
                "Principled dielectric F0 cannot be negative."
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

        const Vec3 geometric_normal =
            normalize(hit.normal);

        const Vec3 n =
            normalize(hit.get_shading_normal());

        const Vec3 wo =
            normalize(-incoming.direction);

        if (dot(geometric_normal, wo) <= 0.0 ||
            dot(n, wo) <= 0.0) {
            return false;
        }

        const double metallic = metallic_at(hit);
        const double roughness = roughness_at(hit);

        const double specular_probability =
            specular_sampling_probability(
                n,
                wo,
                metallic
            );

        Vec3 wi;

        if (Random::random_double() < specular_probability) {
            const Vec3 half_vector =
                sample_ggx_half_vector(
                    n,
                    wo,
                    roughness
                );

            wi = normalize(reflect(-wo, half_vector));

            // GGX reflection broadens the ray cone according
            // to the sampled material roughness.
            info.ray_cone_spread_add =
                0.5 * roughness;
        } else {
            wi = sample_cosine_hemisphere(n);

            // Diffuse sampling produces a broader angular
            // distribution than glossy reflection.
            info.ray_cone_spread_add = 0.5;
        }

        if (!shading_normal::valid_reflection_directions(
                geometric_normal,
                n,
                wo,
                wi)) {
            return false;
        }

        const double pdf =
            scattering_pdf(hit, wo, wi);

        if (!std::isfinite(pdf) || pdf <= 0.0) {
            return false;
        }

        const Vec3 f = evaluate_bsdf(hit, wo, wi);

        const double cos_theta =
            std::max(0.0, dot(n, wi));

        attenuation = f * (cos_theta / pdf);
        scattered = Ray(hit.point, wi);

        return true;
    }

    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        const Vec3 geometric_normal =
            normalize(hit.normal);

        const Vec3 n =
            normalize(hit.get_shading_normal());

        const Vec3 wo =
            normalize(outgoing_direction);

        const Vec3 wi =
            normalize(incoming_direction);

        if (!shading_normal::valid_reflection_directions(
                geometric_normal,
                n,
                wo,
                wi)) {
            return Vec3(0.0, 0.0, 0.0);
        }

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

        const double metallic = metallic_at(hit);
        const double roughness = roughness_at(hit);

        const double alpha = roughness * roughness;
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
            (1.0 - metallic) * dielectric_f0_
            + metallic * base_color_;

        const Vec3 fresnel =
            schlick_fresnel(f0, cos_oh);

        const Vec3 specular =
            fresnel * (
                distribution * geometry /
                (4.0 * cos_o * cos_i)
            );

        const Vec3 diffuse_fresnel_out =
            schlick_fresnel(dielectric_f0_, cos_o);

        const Vec3 diffuse_fresnel_in =
            schlick_fresnel(dielectric_f0_, cos_i);

        const Vec3 one(1.0, 1.0, 1.0);

        const Vec3 diffuse =
            (1.0 - metallic)
            * base_color_
            * (one - diffuse_fresnel_out)
            * (one - diffuse_fresnel_in)
            / PI;

        const Vec3 multiple_scattering =
            evaluate_multiple_scattering(
                cos_o,
                cos_i,
                f0,
                roughness
            );

        const Vec3 base_bsdf =
            diffuse + specular + multiple_scattering;

        const double correction =
            shading_normal::radiance_correction(
                geometric_normal,
                n,
                wo,
                wi
            );

        return base_bsdf * correction;
    }

    double scattering_pdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        const Vec3 geometric_normal =
            normalize(hit.normal);

        const Vec3 n =
            normalize(hit.get_shading_normal());

        const Vec3 wo =
            normalize(outgoing_direction);

        const Vec3 wi =
            normalize(incoming_direction);

        if (!shading_normal::valid_reflection_directions(
                geometric_normal,
                n,
                wo,
                wi)) {
            return 0.0;
        }

        const double cos_o = dot(n, wo);
        const double cos_i = dot(n, wi);

        if (cos_o <= 0.0 || cos_i <= 0.0) {
            return 0.0;
        }

        const double metallic = metallic_at(hit);
        const double roughness = roughness_at(hit);

        const double specular_probability =
            specular_sampling_probability(
                n,
                wo,
                metallic
            );

        const double diffuse_pdf = cos_i / PI;

        const Vec3 half_sum = wo + wi;

        if (half_sum.near_zero()) {
            return (1.0 - specular_probability) * diffuse_pdf;
        }

        const Vec3 h = normalize(half_sum);

        const double cos_h =
            std::max(0.0, dot(n, h));

        const double cos_oh =
            std::abs(dot(wo, h));

        if (cos_h <= 0.0 || cos_oh <= 1e-12) {
            return (1.0 - specular_probability) * diffuse_pdf;
        }

        const double alpha = roughness * roughness;
        const double alpha_squared = alpha * alpha;

        const double denominator =
            cos_h * cos_h * (alpha_squared - 1.0) + 1.0;

        const double distribution =
            alpha_squared /
            (PI * denominator * denominator);

        const double specular_pdf =
            distribution
            * smith_g1(cos_o, alpha_squared)
            / (4.0 * cos_o);

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
    Vec3 dielectric_f0_;
    std::shared_ptr<const Texture> metallic_texture_;
    std::shared_ptr<const Texture> roughness_texture_;

    static double luminance(const Vec3& color) {
        return
            0.2126 * color.x
            + 0.7152 * color.y
            + 0.0722 * color.z;
    }

    double ggx_directional_albedo(
        double cos_theta,
        double roughness
    ) const {
        const double alpha =
            roughness * roughness;

        const double alpha_uv =
            alpha * alpha;

        const double mu =
            std::clamp(cos_theta, 0.0, 1.0);

        const double roughness_term =
            -0.20277
            + alpha_uv * (
                2.772
                + alpha_uv * (
                    -2.6175
                    + 0.73343 * alpha_uv
                )
            );

        const double angle_term =
            3.09507
            + mu * (
                -9.11369
                + mu * (
                    15.8884
                    + mu * (
                        -13.70343
                        + 4.51786 * mu
                    )
                )
            );

        const double albedo =
            1.0
            - 1.4594
            * alpha_uv
            * mu
            * roughness_term
            * angle_term;

        return std::clamp(albedo, 0.0, 1.0);
    }

    double ggx_average_albedo(
        double roughness
    ) const {
        const double alpha =
            roughness * roughness;

        const double alpha_uv =
            alpha * alpha;

        const double albedo =
            1.0
            + alpha_uv * (
                -0.113
                + alpha_uv * (
                    -1.8695
                    + alpha_uv * (
                        2.2268
                        - 0.83397 * alpha_uv
                    )
                )
            );

        return std::clamp(albedo, 0.0, 1.0);
    }

    Vec3 evaluate_multiple_scattering(
        double cos_o,
        double cos_i,
        const Vec3& f0,
        double roughness
    ) const {
        const double e_o =
            ggx_directional_albedo(
                cos_o,
                roughness
            );

        const double e_i =
            ggx_directional_albedo(
                cos_i,
                roughness
            );

        const double e_avg =
            ggx_average_albedo(roughness);

        const double geometric_denominator =
            PI * (1.0 - e_avg);

        if (geometric_denominator <= 1e-12) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const double average_albedo_factor =
            (1.0 - e_o)
            * (1.0 - e_i)
            / geometric_denominator;

        const Vec3 f_avg =
            (f0 * 20.0 + Vec3(1.0, 1.0, 1.0))
            / 21.0;

        const Vec3 denominator =
            Vec3(1.0, 1.0, 1.0)
            - f_avg * (1.0 - e_avg);

        const Vec3 numerator =
            f_avg * f_avg * e_avg;

        const Vec3 fresnel_factor(
            numerator.x / denominator.x,
            numerator.y / denominator.y,
            numerator.z / denominator.z
        );

        return average_albedo_factor * fresnel_factor;
    }

    double specular_sampling_probability(
        const Vec3& normal,
        const Vec3& outgoing_direction,
        double metallic
    ) const {
        const double cos_o = std::clamp(
            dot(normal, outgoing_direction),
            0.0,
            1.0
        );

        const Vec3 f0 =
            (1.0 - metallic) * dielectric_f0_
            + metallic * base_color_;

        const Vec3 fresnel =
            schlick_fresnel(f0, cos_o);

        const Vec3 diffuse_fresnel =
            schlick_fresnel(dielectric_f0_, cos_o);

        const Vec3 one(1.0, 1.0, 1.0);

        const double specular_energy =
            std::max(0.0, luminance(fresnel));

        const double diffuse_energy =
            std::max(
                0.0,
                (1.0 - metallic)
                * luminance(
                    base_color_
                    * (one - diffuse_fresnel)
                )
            );

        constexpr double ENERGY_EPSILON = 1e-12;

        if (diffuse_energy <= ENERGY_EPSILON) {
            return specular_energy > ENERGY_EPSILON
                ? 1.0
                : 0.5;
        }

        if (specular_energy <= ENERGY_EPSILON) {
            return 0.0;
        }

        const double total_energy =
            specular_energy + diffuse_energy;

        if (!std::isfinite(total_energy) ||
            total_energy <= ENERGY_EPSILON) {
            return 0.5;
        }

        return std::clamp(
            specular_energy / total_energy,
            0.0,
            1.0
        );
    }

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
        const Vec3& normal,
        const Vec3& outgoing_direction,
        double roughness
    ) const {
        const Vec3 helper =
            std::abs(normal.x) > 0.9
                ? Vec3(0.0, 1.0, 0.0)
                : Vec3(1.0, 0.0, 0.0);

        const Vec3 tangent =
            normalize(cross(helper, normal));

        const Vec3 bitangent =
            cross(normal, tangent);

        const Vec3 wo = normalize(outgoing_direction);

        const Vec3 view(
            dot(wo, tangent),
            dot(wo, bitangent),
            dot(wo, normal)
        );

        const double alpha = roughness * roughness;

        const Vec3 stretched_view =
            normalize(Vec3(
                alpha * view.x,
                alpha * view.y,
                view.z
            ));

        const double lensq =
            stretched_view.x * stretched_view.x
            + stretched_view.y * stretched_view.y;

        const Vec3 t1 =
            lensq > 1e-12
                ? Vec3(
                    -stretched_view.y,
                    stretched_view.x,
                    0.0
                ) / std::sqrt(lensq)
                : Vec3(1.0, 0.0, 0.0);

        const Vec3 t2 = cross(stretched_view, t1);

        const double r1 = Random::random_double();
        const double r2 = Random::random_double();

        const double radius = std::sqrt(r1);
        const double phi = 2.0 * PI * r2;

        const double p1 = radius * std::cos(phi);
        double p2 = radius * std::sin(phi);

        const double blend =
            0.5 * (1.0 + stretched_view.z);

        p2 =
            (1.0 - blend)
                * std::sqrt(std::max(0.0, 1.0 - p1 * p1))
            + blend * p2;

        const double p3 =
            std::sqrt(std::max(
                0.0,
                1.0 - p1 * p1 - p2 * p2
            ));

        const Vec3 sampled_normal =
            p1 * t1
            + p2 * t2
            + p3 * stretched_view;

        const Vec3 local_half_vector = normalize(Vec3(
            alpha * sampled_normal.x,
            alpha * sampled_normal.y,
            std::max(0.0, sampled_normal.z)
        ));

        return normalize(
            local_half_vector.x * tangent
            + local_half_vector.y * bitangent
            + local_half_vector.z * normal
        );
    }

    double metallic_at(const HitRecord& hit) const {
        if (!metallic_texture_) {
            return metallic_;
        }

        const Vec3 value = metallic_texture_->value(
            hit.u,
            hit.v,
            hit.point,
            make_texture_sample_context(hit)
        );

        return std::clamp(value.x, 0.0, 1.0);
    }

    double roughness_at(const HitRecord& hit) const {
        if (!roughness_texture_) {
            return roughness_;
        }

        const Vec3 value = roughness_texture_->value(
            hit.u,
            hit.v,
            hit.point,
            make_texture_sample_context(hit)
        );

        return std::clamp(value.x, 0.045, 1.0);
    }
};
