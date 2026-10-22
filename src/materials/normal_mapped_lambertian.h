#pragma once

#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>
#include <algorithm>

#include "materials/material.h"
#include "materials/texture.h"

class NormalMappedMaterial final : public Material {
public:
    NormalMappedMaterial(
        std::shared_ptr<const Material> base_material,
        std::shared_ptr<const Texture> normal_map,
        double normal_strength = 0.4
    )
        : base_material_(std::move(base_material)),
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
        if (!base_material_) {
            throw std::invalid_argument(
                "NormalMappedMaterial requires a base material."
            );
        }

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

        Vec3 rgb;

        if (hit.has_uv_differentials) {
            rgb = normal_map_->value(
                hit.u,
                hit.v,
                hit.point,
                hit.dudx,
                hit.dvdx,
                hit.dudy,
                hit.dvdy
            );
        } else {
            rgb = normal_map_->value(
                hit.u,
                hit.v,
                hit.point
            );
        }

        // Decode tangent-space RGB from [0, 1] to [-1, 1].
        Vec3 tangent_normal(
            2.0 * rgb.x - 1.0,
            2.0 * rgb.y - 1.0,
            2.0 * rgb.z - 1.0
        );

        // Blend the normal map toward the flat tangent-space normal.
        // This reduces excessive shading changes near hard mesh edges.
        tangent_normal = normalize_or(
            Vec3(0.0, 0.0, 1.0) * (1.0 - normal_strength_) +
                tangent_normal * normal_strength_,
            Vec3(0.0, 0.0, 1.0)
        );

        Vec3 world_normal =
            tangent_normal.x * tangent +
            tangent_normal.y * bitangent +
            tangent_normal.z * geometric_normal;

        world_normal = normalize_or(
            world_normal,
            geometric_normal
        );

        // Keep the mapped normal in the geometric hemisphere.
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
        const Vec3 geometric_normal = hit.normal;

        // Preserve the geometric surface as a hard hemisphere constraint.
        if (dot(geometric_normal, outgoing_direction) <= 0.0 ||
            dot(geometric_normal, incoming_direction) <= 0.0) {
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
        Ray& scattered
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
                scattered
            )) {
            return false;
        }

        const Vec3 direction =
            normalize_or(
                scattered.direction,
                geometric_normal
            );

        // Reject samples below the geometric surface.
        // Do not project or rotate them: doing so would change
        // the sampling distribution without updating its PDF.
        if (dot(direction, geometric_normal) <= 1e-8) {
            return false;
        }

        scattered = Ray(hit.point, direction);
        return true;
    }

private:
    std::shared_ptr<const Material> base_material_;
    std::shared_ptr<const Texture> normal_map_;
    double normal_strength_ = 0.4;

    static Vec3 normalize_or(
        const Vec3& value,
        const Vec3& fallback
    ) {
        const double length_squared = value.length_squared();

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
