#pragma once

#include <memory>
#include <utility>
#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "materials/material.h"
#include "materials/texture.h"
#include "materials/shading_normal.h"
#include "materials/texture_sample_context.h"
#include "core/random.h"

class Lambertian : public Material {
public:
    explicit Lambertian(const Vec3& albedo)
        : texture_(std::make_shared<SolidColor>(albedo)) {}

    explicit Lambertian(std::shared_ptr<const Texture> texture)
        : texture_(std::move(texture)) {
        if (!texture_) {
            throw std::invalid_argument(
                "Lambertian texture cannot be null."
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
        info.ray_cone_spread_add = 0.5;

        const Vec3 geometric_normal =
            normalize(hit.normal);

        const Vec3 normal =
            normalize(hit.get_shading_normal());

        const Vec3 outgoing =
            normalize(-incoming.direction);

        if (!shading_normal::valid_reflection_outgoing_direction(
                geometric_normal,
                normal,
                outgoing)) {
            return false;
        }

        // Construct an orthonormal basis around the shading normal.
        const Vec3 helper =
            std::abs(normal.x) > 0.9
                ? Vec3(0.0, 1.0, 0.0)
                : Vec3(1.0, 0.0, 0.0);

        const Vec3 tangent =
            normalize(cross(helper, normal));

        const Vec3 bitangent =
            cross(normal, tangent);

        // Cosine-weighted hemisphere sampling.
        const double r1 = Random::random_double();
        const double r2 = Random::random_double();

        constexpr double PI = 3.14159265358979323846;

        const double phi = 2.0 * PI * r1;
        const double radius = std::sqrt(r2);
        const double local_x = radius * std::cos(phi);
        const double local_y = radius * std::sin(phi);
        const double local_z = std::sqrt(1.0 - r2);

        const Vec3 scatter_direction =
            local_x * tangent
            + local_y * bitangent
            + local_z * normal;

        const Vec3 incoming_direction =
            normalize(scatter_direction);

        if (!shading_normal::valid_reflection_directions(
                geometric_normal,
                normal,
                outgoing,
                incoming_direction)) {
            return false;
        }

        const double pdf =
            scattering_pdf(
                hit,
                outgoing,
                incoming_direction
            );

        if (!std::isfinite(pdf) || pdf <= 1e-12) {
            return false;
        }

        const Vec3 bsdf =
            evaluate_bsdf(
                hit,
                outgoing,
                incoming_direction
            );

        const double cosine =
            std::max(0.0, dot(normal, incoming_direction));

        attenuation = bsdf * (cosine / pdf);
        scattered = Ray(hit.point, incoming_direction);

        return true;
    }

    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        const Vec3 geometric_normal =
            normalize(hit.normal);

        const Vec3 normal =
            normalize(hit.get_shading_normal());

        const Vec3 outgoing =
            normalize(outgoing_direction);

        const Vec3 incoming =
            normalize(incoming_direction);

        if (!shading_normal::valid_reflection_directions(
                geometric_normal,
                normal,
                outgoing,
                incoming)) {
            return Vec3(0.0, 0.0, 0.0);
        }

        constexpr double PI = 3.14159265358979323846;

        const Vec3 albedo = texture_->value(
            hit.u,
            hit.v,
            hit.point,
            make_texture_sample_context(hit)
        );

        const double correction =
            shading_normal::radiance_correction(
                geometric_normal,
                normal,
                outgoing,
                incoming
            );

        return (albedo / PI) * correction;
    }

    double scattering_pdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        const Vec3 geometric_normal =
            normalize(hit.normal);

        const Vec3 normal =
            normalize(hit.get_shading_normal());

        const Vec3 outgoing =
            normalize(outgoing_direction);

        const Vec3 incoming =
            normalize(incoming_direction);

        if (!shading_normal::valid_reflection_directions(
                geometric_normal,
                normal,
                outgoing,
                incoming)) {
            return 0.0;
        }

        constexpr double PI = 3.14159265358979323846;

        return dot(normal, incoming) / PI;
    }

    bool supports_direct_lighting() const override {
        return true;
    }

private:
    std::shared_ptr<const Texture> texture_;
};
