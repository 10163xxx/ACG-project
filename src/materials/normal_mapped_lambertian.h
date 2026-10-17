#pragma once

#include <cmath>
#include <memory>
#include <utility>

#include "core/random.h"
#include "materials/material.h"
#include "materials/texture.h"

class NormalMappedLambertian final : public Material {
public:
    NormalMappedLambertian(
        const Vec3& albedo,
        std::shared_ptr<const Texture> normal_map)
        : albedo_(albedo),
          normal_map_(std::move(normal_map)) {}

    Vec3 mapped_normal(
        const Ray& incoming,
        const HitRecord& hit) const {
        Vec3 n = hit.front_face
            ? hit.normal
            : Vec3(-hit.normal.x, -hit.normal.y, -hit.normal.z);

        n = normalize_or(n, Vec3(0.0, 1.0, 0.0));

        Vec3 t(n.z, 0.0, -n.x);
        if (t.length_squared() < 1e-12) {
            t = Vec3(1.0, 0.0, 0.0);
        }
        t = normalize_or(t, Vec3(1.0, 0.0, 0.0));

        const Vec3 b(
            n.y * t.z - n.z * t.y,
            n.z * t.x - n.x * t.z,
            n.x * t.y - n.y * t.x);

        const Vec3 rgb = normal_map_->value(hit.u, hit.v, hit.point);
        Vec3 tangent_normal(
            2.0 * rgb.x - 1.0,
            2.0 * rgb.y - 1.0,
            2.0 * rgb.z - 1.0);

        tangent_normal = normalize_or(
            tangent_normal, Vec3(0.0, 0.0, 1.0));

        Vec3 world_normal =
            tangent_normal.x * t +
            tangent_normal.y * b +
            tangent_normal.z * n;

        world_normal = normalize_or(world_normal, n);

        const double facing =
            incoming.direction.x * world_normal.x +
            incoming.direction.y * world_normal.y +
            incoming.direction.z * world_normal.z;

        if (facing > 0.0) {
            world_normal = Vec3(
                -world_normal.x,
                -world_normal.y,
                -world_normal.z);
        }

        return world_normal;
    }

    bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered) const override {
        const Vec3 geometric_normal = hit.normal;
        const Vec3 shading_normal = mapped_normal(incoming, hit);

        Vec3 scatter_direction =
            shading_normal + Random::random_unit_vector();

        if (scatter_direction.length_squared() < 1e-12) {
            scatter_direction = shading_normal;
        }

        const double side = dot(scatter_direction, geometric_normal);

        if (side <= 1e-8) {
            scatter_direction =
                scatter_direction
                - side * geometric_normal
                + 1e-6 * geometric_normal;
        }

        scattered = Ray(hit.point, scatter_direction);
        attenuation = albedo_;
        return true;
    }
    
    Vec3 evaluate_bsdf(
        const HitRecord& hit,
        const Vec3& outgoing_direction,
        const Vec3& incoming_direction
    ) const override {
        // Reconstruct the incoming ray direction from the
        // outgoing direction used by the BSDF evaluation.
        const Ray incoming_ray(
            hit.point,
            -outgoing_direction
        );

        const Vec3 shading_normal =
            mapped_normal(incoming_ray, hit);

        // Both directions must lie above the shading normal.
        const double cos_outgoing =
            dot(shading_normal, outgoing_direction);

        const double cos_incoming =
            dot(shading_normal, incoming_direction);

        // Reject directions that lie below the geometric
        // surface, even if the normal map points toward them.
        const double geometric_outgoing =
            dot(hit.normal, outgoing_direction);

        const double geometric_incoming =
            dot(hit.normal, incoming_direction);

        if (cos_outgoing <= 0.0 ||
            cos_incoming <= 0.0 ||
            geometric_outgoing <= 0.0 ||
            geometric_incoming <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        constexpr double PI = 3.14159265358979323846;

        return albedo_ / PI;
    }

    bool supports_direct_lighting() const override {
        return true;
    }

private:
    static Vec3 normalize_or(const Vec3& v, const Vec3& fallback) {
        const double len2 = v.length_squared();
        if (len2 < 1e-12) {
            return fallback;
        }
        return v / std::sqrt(len2);
    }

    Vec3 albedo_;
    std::shared_ptr<const Texture> normal_map_;
};
