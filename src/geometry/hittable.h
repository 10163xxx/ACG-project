#pragma once

#include "core/ray.h"
#include "core/interval.h"
#include "geometry/aabb.h"
#include "geometry/hit_record.h"
#include "materials/material.h"

#include <vector>
#include <algorithm>
#include <cmath>

class Hittable {
public:
    virtual ~Hittable() = default;

    virtual bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const = 0;

    virtual void all_intersections(
        const Ray& ray,
        const Interval& ray_t,
        std::vector<HitRecord>& intersections
    ) const {
        constexpr double STEP_EPSILON = 1e-7;

        double t_min = ray_t.min;
        HitRecord record;

        while (t_min < ray_t.max) {
            if (!hit(
                    ray,
                    Interval(t_min, ray_t.max),
                    record)) {
                break;
            }

            intersections.push_back(record);

            t_min = record.t + STEP_EPSILON;
        }
    }

    // Find an opaque surface only.
    // Participating-medium scattering events are not occluders.
    // Find the nearest fully opaque surface.
    // Transparent surfaces do not block the ray completely.
    virtual bool hit_surface(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const {
        std::vector<HitRecord> intersections;
        all_intersections(ray, ray_t, intersections);

        for (const HitRecord& intersection : intersections) {
            if (intersection.is_volume) {
                continue;
            }

            if (!intersection.material ||
                intersection.material->blocks_shadow(
                    intersection
                )) {
                record = intersection;
                return true;
            }
        }

        return false;
    }

    virtual Vec3 transmittance(
        const Ray& ray,
        const Interval& ray_t
    ) const {
        std::vector<HitRecord> intersections;
        all_intersections(ray, ray_t, intersections);

        Vec3 result(1.0, 1.0, 1.0);

        struct ActiveMedium {
            const Material* material;
            double entry_t;
        };

        std::vector<ActiveMedium> active_media;

        const double direction_length = ray.direction.length();
        const bool closed_volume = is_closed_volume();

        const auto apply_absorption = [&result](
            const Material* material,
            double distance
        ) {
            if (material == nullptr || distance <= 0.0) {
                return;
            }

            const Vec3 absorption =
                material->shadow_absorption_coefficient();

            const Vec3 attenuation(
                std::exp(-absorption.x * distance),
                std::exp(-absorption.y * distance),
                std::exp(-absorption.z * distance)
            );

            result = result * attenuation;
        };

        for (const HitRecord& intersection : intersections) {
            if (intersection.is_volume ||
                !intersection.material) {
                continue;
            }

            const Material* material =
                intersection.material.get();

            // Evaluate transmission at every surface independently.
            result = result
                * material->shadow_transmittance(
                    intersection,
                    ray.direction
                );

            if (result.near_zero()) {
                return Vec3(0.0, 0.0, 0.0);
            }

            // Open surfaces have no inferred interior absorption.
            if (!closed_volume) {
                continue;
            }

            if (intersection.front_face) {
                active_media.push_back({
                    material,
                    intersection.t
                });

                continue;
            }

            if (active_media.empty()) {
                // The ray interval may begin inside this closed
                // volume, before its exit boundary is encountered.
                const double distance = std::max(
                    0.0,
                    intersection.t - ray_t.min
                ) * direction_length;

                apply_absorption(material, distance);

                if (result.near_zero()) {
                    return Vec3(0.0, 0.0, 0.0);
                }

                continue;
            }

            // Pair boundaries by geometric traversal order rather
            // than material pointer. Different OBJ faces may use
            // different materials on the entry and exit boundaries.
            const ActiveMedium medium = active_media.back();
            active_media.pop_back();

            const double distance = std::max(
                0.0,
                intersection.t - medium.entry_t
            ) * direction_length;

            // Use the entry material's absorption coefficient for
            // the interior segment.
            apply_absorption(medium.material, distance);

            if (result.near_zero()) {
                return Vec3(0.0, 0.0, 0.0);
            }
        }

        // The ray interval can end before it exits the volume.
        for (const ActiveMedium& medium : active_media) {
            const double distance = std::max(
                0.0,
                ray_t.max - medium.entry_t
            ) * direction_length;

            apply_absorption(medium.material, distance);

            if (result.near_zero()) {
                return Vec3(0.0, 0.0, 0.0);
            }
        }

        return result;
    }

    // Whether this geometry is a verified closed volume.
    // Open surfaces must not infer an entry point when a ray
    // encounters a back face without a matching front face.
    virtual bool is_closed_volume() const {
        return false;
    }

    virtual AABB bounding_box() const = 0;

    virtual bool sample_surface(
        double u,
        double v,
        HitRecord& record,
        double& pdf_area
    ) const {
        (void)u;
        (void)v;
        (void)record;

        pdf_area = 0.0;
        return false;
    }
};