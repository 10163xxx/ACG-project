#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

#include "materials/material.h"
#include "materials/texture.h"
#include "materials/material_decorator.h"

class BumpMappedMaterial final : public MaterialDecorator {
public:
    BumpMappedMaterial(
        std::shared_ptr<const Material> base_material,
        std::shared_ptr<const Texture> height_map,
        double strength = 1.0
    )
        : MaterialDecorator(std::move(base_material)),
          height_map_(std::move(height_map)),
          strength_(strength) {

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
        if (hit.has_resolved_shading_normal) {
            return hit.get_shading_normal();
        }

        const Vec3 geometric_normal =
            normalize_or(
                hit.normal,
                Vec3(0.0, 1.0, 0.0)
            );

        Vec3 base_normal =
            normalize_or(
                base_material_->shading_normal(incoming, hit),
                geometric_normal
            );

        if (dot(base_normal, geometric_normal) < 0.0) {
            base_normal = -1.0 * base_normal;
        }

        Vec3 tangent =
            hit.tangent -
            dot(hit.tangent, base_normal) *
                base_normal;

        tangent = normalize_or(
            tangent,
            fallback_tangent(base_normal)
        );

        Vec3 bitangent =
            hit.bitangent -
            dot(hit.bitangent, base_normal) *
                base_normal -
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
            base_normal
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
    
    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        HitRecord mapped_hit = hit;
        mapped_hit.shading_normal = mapped_normal(
            Ray(hit.point, -outgoing_direction),
            hit
        );
        mapped_hit.has_shading_normal = true;
        mapped_hit.has_resolved_shading_normal = true;

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
        HitRecord mapped_hit = hit;
        mapped_hit.shading_normal = mapped_normal(
            Ray(hit.point, -outgoing_direction),
            hit
        );
        mapped_hit.has_shading_normal = true;
        mapped_hit.has_resolved_shading_normal = true;

        return base_material_->scattering_pdf(
            mapped_hit,
            outgoing_direction,
            incoming_direction
        );
    }

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered,
        ScatterSampleInfo& info
    ) const override {
        HitRecord mapped_hit = hit;
        mapped_hit.shading_normal = mapped_normal(incoming, hit);
        mapped_hit.has_shading_normal = true;
        mapped_hit.has_resolved_shading_normal = true;

        if (!base_material_->scatter(
                incoming,
                mapped_hit,
                attenuation,
                scattered,
                info
            )) {
            return false;
        }

        if (info.is_transmission || info.is_straight_through) {
            return true;
        }

        return true;
    }

private:
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
