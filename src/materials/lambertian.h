#pragma once

#include <memory>
#include <utility>

#include "materials/material.h"
#include "materials/texture.h"
#include "core/random.h"

class Lambertian : public Material {
public:
    explicit Lambertian(const Vec3& albedo)
        : texture_(std::make_shared<SolidColor>(albedo)) {}

    explicit Lambertian(std::shared_ptr<const Texture> texture)
        : texture_(std::move(texture)) {}

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered
    ) const override {
        (void)incoming;

        const Vec3 normal = normalize(hit.normal);

        // Construct an orthonormal basis around the normal.
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

        scattered = Ray(hit.point, normalize(scatter_direction));

        attenuation = texture_->value_with_cone(
            hit.u,
            hit.v,
            hit.point,
            hit.dudx,
            hit.dvdx,
            hit.dudy,
            hit.dvdy,
            hit.has_uv_differentials,
            hit.ray_cone_width,
            hit.has_ray_cone,
            hit.dpdu,
            hit.dpdv,
            hit.has_uv_parameterization
        );

        return true;
    }

    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        const double cos_outgoing =
            dot(hit.normal, outgoing_direction);

        const double cos_incoming =
            dot(hit.normal, incoming_direction);

        if (cos_outgoing <= 0.0 || cos_incoming <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        constexpr double PI = 3.14159265358979323846;

        const Vec3 albedo = texture_->value_with_cone(
            hit.u,
            hit.v,
            hit.point,
            hit.dudx,
            hit.dvdx,
            hit.dudy,
            hit.dvdy,
            hit.has_uv_differentials,
            hit.ray_cone_width,
            hit.has_ray_cone,
            hit.dpdu,
            hit.dpdv,
            hit.has_uv_parameterization
        );

        return albedo / PI;
    }

    double scattering_pdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        const Vec3 normal = normalize(hit.normal);
        const Vec3 outgoing = normalize(outgoing_direction);
        const Vec3 incoming = normalize(incoming_direction);

        const double cos_outgoing = dot(normal, outgoing);
        const double cos_incoming = dot(normal, incoming);

        if (cos_outgoing <= 0.0 || cos_incoming <= 0.0) {
            return 0.0;
        }

        constexpr double PI = 3.14159265358979323846;

        return cos_incoming / PI;
    }

    bool supports_direct_lighting() const override {
        return true;
    }

private:
    std::shared_ptr<const Texture> texture_;
};
