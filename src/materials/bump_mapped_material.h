#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

#include "materials/material.h"
#include "materials/texture.h"

class BumpMappedMaterial final : public Material {
public:
    BumpMappedMaterial(
        std::shared_ptr<const Material> base_material,
        std::shared_ptr<const Texture> height_map,
        double strength = 1.0
    )
        : base_material_(std::move(base_material)),
          height_map_(std::move(height_map)),
          strength_(strength) {
        if (!base_material_) {
            throw std::invalid_argument(
                "BumpMappedMaterial requires a base material."
            );
        }

        if (!height_map_) {
            throw std::invalid_argument(
                "BumpMappedMaterial requires a height map."
            );
        }

        if (!std::isfinite(strength_)) {
            throw std::invalid_argument(
                "Bump strength must be finite."
            );
        }
    }

    Vec3 mapped_normal(
        const Ray& incoming,
        const HitRecord& hit
    ) const {
        (void)incoming;

        const Vec3 geometric_normal =
            normalize_or(
                hit.normal,
                Vec3(0.0, 1.0, 0.0)
            );

        Vec3 tangent =
            hit.tangent -
            dot(hit.tangent, geometric_normal) *
                geometric_normal;

        tangent = normalize_or(
            tangent,
            fallback_tangent(geometric_normal)
        );

        Vec3 bitangent =
            hit.bitangent -
            dot(hit.bitangent, geometric_normal) *
                geometric_normal -
            dot(hit.bitangent, tangent) * tangent;

        bitangent = normalize_or(
            bitangent,
            normalize_or(
                cross(geometric_normal, tangent),
                Vec3(0.0, 0.0, 1.0)
            )
        );

        // Central differences estimate the height gradient in UV space.
        constexpr double UV_STEP = 1e-3;

        const double dhdu =
            (
                height(hit.u + UV_STEP, hit.v, hit) -
                height(hit.u - UV_STEP, hit.v, hit)
            ) / (2.0 * UV_STEP);

        const double dhdv =
            (
                height(hit.u, hit.v + UV_STEP, hit) -
                height(hit.u, hit.v - UV_STEP, hit)
            ) / (2.0 * UV_STEP);

        Vec3 world_normal =
            geometric_normal
            - strength_ * dhdu * tangent
            - strength_ * dhdv * bitangent;

        world_normal = normalize_or(
            world_normal,
            geometric_normal
        );

        // Keep the perturbed normal in the geometric hemisphere.
        const double hemisphere_dot =
            dot(world_normal, geometric_normal);

        if (hemisphere_dot < 0.0) {
            world_normal =
                world_normal -
                hemisphere_dot * geometric_normal;

            world_normal = normalize_or(
                world_normal,
                geometric_normal
            );
        }

        return world_normal;
    }

    Vec3 shading_normal(
        const Ray& incoming,
        const HitRecord& hit
    ) const override {
        return mapped_normal(incoming, hit);
    }

    Vec3 emitted(const HitRecord& hit) const override {
        return base_material_->emitted(hit);
    }

    bool is_emissive() const override {
        return base_material_->is_emissive();
    }

    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        if (dot(hit.normal, outgoing_direction) <= 0.0 ||
            dot(hit.normal, incoming_direction) <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        HitRecord mapped_hit = hit;
        mapped_hit.normal = mapped_normal(
            Ray(hit.point, -outgoing_direction),
            hit
        );

        if (dot(mapped_hit.normal, outgoing_direction) <= 0.0 ||
            dot(mapped_hit.normal, incoming_direction) <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        return base_material_->evaluate_bsdf(
            mapped_hit,
            outgoing_direction,
            incoming_direction
        );
    }

    double scattering_pdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        if (dot(hit.normal, outgoing_direction) <= 0.0 ||
            dot(hit.normal, incoming_direction) <= 0.0) {
            return 0.0;
        }

        HitRecord mapped_hit = hit;
        mapped_hit.normal = mapped_normal(
            Ray(hit.point, -outgoing_direction),
            hit
        );

        if (dot(mapped_hit.normal, outgoing_direction) <= 0.0 ||
            dot(mapped_hit.normal, incoming_direction) <= 0.0) {
            return 0.0;
        }

        return base_material_->scattering_pdf(
            mapped_hit,
            outgoing_direction,
            incoming_direction
        );
    }

    bool supports_direct_lighting() const override {
        return base_material_->supports_direct_lighting();
    }

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered,
        ScatterSampleInfo& info
    ) const override {
        const Vec3 geometric_normal =
            normalize_or(
                hit.normal,
                Vec3(0.0, 1.0, 0.0)
            );

        HitRecord mapped_hit = hit;
        mapped_hit.normal = mapped_normal(incoming, hit);

        if (!base_material_->scatter(
                incoming,
                mapped_hit,
                attenuation,
                scattered,
                info
            )) {
            return false;
        }

        if (info.is_straight_through) {
            return true;
        }

        const Vec3 direction =
            normalize_or(
                scattered.direction,
                geometric_normal
            );

        if (dot(direction, geometric_normal) <= 1e-8) {
            return false;
        }

        scattered = Ray(hit.point, direction);
        return true;
    }

private:
    std::shared_ptr<const Material> base_material_;
    std::shared_ptr<const Texture> height_map_;
    double strength_;

    double height(
        double u,
        double v,
        const HitRecord& hit
    ) const {
        const Vec3 color = height_map_->value_with_cone(
            u,
            v,
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

        // Convert RGB height maps to scalar luminance.
        return
            0.2126 * color.x +
            0.7152 * color.y +
            0.0722 * color.z;
    }

    static Vec3 normalize_or(
        const Vec3& value,
        const Vec3& fallback
    ) {
        const double length_squared =
            value.length_squared();

        if (!std::isfinite(length_squared) ||
            length_squared < 1e-20) {
            return fallback;
        }

        return value / std::sqrt(length_squared);
    }

    static Vec3 fallback_tangent(const Vec3& normal) {
        const Vec3 helper =
            std::abs(normal.y) < 0.8
                ? Vec3(0.0, 1.0, 0.0)
                : Vec3(1.0, 0.0, 0.0);

        return normalize_or(
            cross(helper, normal),
            Vec3(1.0, 0.0, 0.0)
        );
    }
};
