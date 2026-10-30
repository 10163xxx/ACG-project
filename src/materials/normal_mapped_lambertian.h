#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

#include "materials/material.h"
#include "materials/texture.h"

class NormalMappedMaterial final : public Material {
public:
    NormalMappedMaterial(
        std::shared_ptr<const Material> base_material,
        std::shared_ptr<const Texture> normal_map,
        double normal_strength = 1.0
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

    Vec3 mapped_normal(const HitRecord& hit) const {
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
            tangent_normal.z * geometric_normal;

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
        (void)incoming;
        return mapped_normal(hit);
    }

    Vec3 emitted(const HitRecord& hit) const override {
        return base_material_->emitted(hit);
    }

    bool is_emissive() const override {
        return base_material_->is_emissive();
    }

    Vec3 shadow_absorption_coefficient() const override {
        return base_material_->shadow_absorption_coefficient();
    }

    double index_of_refraction() const override {
        return base_material_->index_of_refraction();
    }

    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        const Vec3 geometric_normal =
            normalize_or(
                hit.normal,
                Vec3(0.0, 1.0, 0.0)
            );

        const Vec3 wo =
            normalize_or(
                outgoing_direction,
                geometric_normal
            );

        const Vec3 wi =
            normalize_or(
                incoming_direction,
                geometric_normal
            );

        // Reject directions crossing the geometric surface.
        if (dot(geometric_normal, wo) <= 0.0 ||
            dot(geometric_normal, wi) <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        HitRecord mapped_hit = hit;
        mapped_hit.shading_normal = mapped_normal(hit);
        mapped_hit.has_shading_normal = true;

        const Vec3 shading_normal =
            normalize_or(
                mapped_hit.get_shading_normal(),
                geometric_normal
            );

        // Both directions must also lie in the shading hemisphere.
        if (dot(shading_normal, wo) <= 0.0 ||
            dot(shading_normal, wi) <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Vec3 base_bsdf =
            base_material_->evaluate_bsdf(
                mapped_hit,
                wo,
                wi
            );

        const double correction =
            shading_normal_correction(
                geometric_normal,
                shading_normal,
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
            normalize_or(
                hit.normal,
                Vec3(0.0, 1.0, 0.0)
            );

        const Vec3 wo =
            normalize_or(
                outgoing_direction,
                geometric_normal
            );

        const Vec3 wi =
            normalize_or(
                incoming_direction,
                geometric_normal
            );

        // Reject directions crossing the geometric surface.
        if (dot(geometric_normal, wo) <= 0.0 ||
            dot(geometric_normal, wi) <= 0.0) {
            return 0.0;
        }

        // Use the same mapped normal as BSDF evaluation and sampling.
        HitRecord mapped_hit = hit;
        mapped_hit.shading_normal = mapped_normal(hit);
        mapped_hit.has_shading_normal = true;

        const Vec3 shading_normal =
            normalize_or(
                mapped_hit.get_shading_normal(),
                geometric_normal
            );

        if (dot(shading_normal, wo) <= 0.0 ||
            dot(shading_normal, wi) <= 0.0) {
            return 0.0;
        }

        return base_material_->scattering_pdf(
            mapped_hit,
            wo,
            wi
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

        const Vec3 wo =
            normalize_or(
                -incoming.direction,
                geometric_normal
            );

        if (dot(geometric_normal, wo) <= 0.0) {
            return false;
        }

        // Use the same mapped normal used by BSDF evaluation and PDF.
        HitRecord mapped_hit = hit;
        mapped_hit.shading_normal = mapped_normal(hit);
        mapped_hit.has_shading_normal = true;

        const Vec3 shading_normal =
            normalize_or(
                mapped_hit.get_shading_normal(),
                geometric_normal
            );

        if (dot(shading_normal, wo) <= 0.0) {
            return false;
        }

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

        const Vec3 wi =
            normalize_or(
                scattered.direction,
                geometric_normal
            );

        // Reject directions outside either valid hemisphere.
        if (dot(geometric_normal, wi) <= 1e-8 ||
            dot(shading_normal, wi) <= 0.0) {
            return false;
        }

        scattered = Ray(hit.point, wi);

        // Recompute throughput using this material's BSDF and PDF.
        const double pdf =
            scattering_pdf(hit, wo, wi);

        if (!std::isfinite(pdf) || pdf <= 1e-12) {
            return false;
        }

        const Vec3 bsdf =
            evaluate_bsdf(hit, wo, wi);

        const double cos_shading =
            std::max(0.0, dot(shading_normal, wi));

        attenuation = bsdf * (cos_shading / pdf);

        return true;
    }

private:
    std::shared_ptr<const Material> base_material_;
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

    static double shading_normal_correction(
        const Vec3& geometric_normal,
        const Vec3& shading_normal,
        const Vec3& wo,
        const Vec3& wi
    ) {
        const double wi_geometric =
            dot(wi, geometric_normal);

        const double wo_geometric =
            dot(wo, geometric_normal);

        const double wi_shading =
            dot(wi, shading_normal);

        const double wo_shading =
            dot(wo, shading_normal);

        constexpr double EPSILON = 1e-8;

        const double denominator =
            wi_shading * wo_geometric;

        if (!std::isfinite(denominator) ||
            std::abs(denominator) < EPSILON) {
            return 0.0;
        }

        const double correction =
            std::abs(
                (wi_geometric * wo_shading) /
                denominator
            );

        if (!std::isfinite(correction)) {
            return 0.0;
        }

        return correction;
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
