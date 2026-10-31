#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

#include "materials/material.h"
#include "materials/texture.h"
#include "materials/material_decorator.h"

class NormalMappedMaterial final : public MaterialDecorator {
public:
    NormalMappedMaterial(
        std::shared_ptr<const Material> base_material,
        std::shared_ptr<const Texture> normal_map,
        double normal_strength = 1.0
    )
        : MaterialDecorator(std::move(base_material)),
          normal_map_(std::move(normal_map)),
          normal_strength_(
              std::clamp(
                  std::isfinite(normal_strength)
                      ? normal_strength
                      : 0.25,
                  0.0,
                  1.0
              )
          ) {

        if (!normal_map_) {
            throw std::invalid_argument(
                "NormalMappedMaterial requires a normal map."
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

        const Vec3 rgb = normal_map_->value_with_cone(
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

        // Decode tangent-space RGB from [0, 1] to [-1, 1].
        Vec3 tangent_normal(
            2.0 * rgb.x - 1.0,
            2.0 * rgb.y - 1.0,
            2.0 * rgb.z - 1.0
        );

        // Blend toward a flat normal according to normal strength.
        tangent_normal = normalize_or(
            Vec3(0.0, 0.0, 1.0) * (1.0 - normal_strength_) +
                tangent_normal * normal_strength_,
            Vec3(0.0, 0.0, 1.0)
        );

        Vec3 world_normal =
            tangent_normal.x * tangent +
            tangent_normal.y * bitangent +
            tangent_normal.z * base_normal;

        world_normal = normalize_or(
            world_normal,
            geometric_normal
        );

        // Keep the shading normal in the geometric hemisphere.
        const double geometric_dot =
            dot(world_normal, geometric_normal);

        if (geometric_dot < 0.0) {
            world_normal =
                world_normal -
                geometric_dot * geometric_normal;

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
        const Vec3 wo =
            normalize_or(
                outgoing_direction,
                hit.normal
            );

        const Vec3 wi =
            normalize_or(
                incoming_direction,
                hit.normal
            );

        HitRecord mapped_hit = hit;
        mapped_hit.shading_normal = mapped_normal(
            Ray(hit.point, -wo),
            hit
        );
        mapped_hit.has_shading_normal = true;
        mapped_hit.has_resolved_shading_normal = true;

        return base_material_->evaluate_bsdf(
            mapped_hit,
            wo,
            wi
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
        mapped_hit.shading_normal = mapped_normal(
            incoming,
            hit
        );
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

        // Transmission and straight-through events are not
        // reflection BSDF samples.
        if (info.is_transmission || info.is_straight_through) {
            return true;
        }

        return true;
    }

private:
    std::shared_ptr<const Texture> normal_map_;
    double normal_strength_ = 1.0;

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
