#pragma once

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

        // A valid UV parameterization is required to convert
        // the UV height gradient into a world-space gradient.
        if (!hit.has_uv_parameterization) {
            return base_normal;
        }

        // Project the UV parameterization directions onto the
        // current shading-normal tangent plane.
        const Vec3 dpdu =
            hit.dpdu -
            dot(hit.dpdu, base_normal) * base_normal;

        const Vec3 dpdv =
            hit.dpdv -
            dot(hit.dpdv, base_normal) * base_normal;

        const double g00 = dot(dpdu, dpdu);
        const double g01 = dot(dpdu, dpdv);
        const double g11 = dot(dpdv, dpdv);

        const double determinant =
            g00 * g11 - g01 * g01;

        if (!std::isfinite(g00) ||
            !std::isfinite(g01) ||
            !std::isfinite(g11) ||
            !std::isfinite(determinant) ||
            g00 <= 1e-20 ||
            g11 <= 1e-20 ||
            determinant <= 1e-12 * g00 * g11) {
            return base_normal;
        }

        // Estimate the height gradient in UV space.
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

        if (!std::isfinite(dhdu) ||
            !std::isfinite(dhdv)) {
            return base_normal;
        }

        // Solve the Gram system:
        //
        // [dot(dpdu, dpdu)  dot(dpdu, dpdv)] [a] = [dhdu]
        // [dot(dpdu, dpdv)  dot(dpdv, dpdv)] [b]   [dhdv]
        //
        // The world-space gradient is a * dpdu + b * dpdv.
        const double a =
            (dhdu * g11 - dhdv * g01) /
            determinant;

        const double b =
            (dhdv * g00 - dhdu * g01) /
            determinant;

        const Vec3 world_gradient =
            a * dpdu + b * dpdv;

        if (!std::isfinite(world_gradient.x) ||
            !std::isfinite(world_gradient.y) ||
            !std::isfinite(world_gradient.z)) {
            return base_normal;
        }

        Vec3 world_normal =
            base_normal - strength_ * world_gradient;

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
};
