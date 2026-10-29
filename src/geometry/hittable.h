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

        for (const HitRecord& intersection : intersections) {
            if (intersection.is_volume ||
                !intersection.material) {
                continue;
            }

            result = result
                * intersection.material->shadow_transmittance(
                    intersection,
                    ray.direction
                );

            if (result.near_zero()) {
                return Vec3(0.0, 0.0, 0.0);
            }

            const Material* material =
                intersection.material.get();

            if (intersection.front_face) {
                active_media.push_back({
                    material,
                    intersection.t
                });
                continue;
            }

            for (std::size_t i = active_media.size(); i > 0; --i) {
                const std::size_t index = i - 1;

                if (active_media[index].material != material) {
                    continue;
                }

                const double distance = std::max(
                    0.0,
                    intersection.t - active_media[index].entry_t
                ) * direction_length;

                const Vec3 absorption =
                    material->shadow_absorption_coefficient();

                const Vec3 attenuation(
                    std::exp(-absorption.x * distance),
                    std::exp(-absorption.y * distance),
                    std::exp(-absorption.z * distance)
                );

                result = result * attenuation;

                active_media.erase(
                    active_media.begin() + index
                );

                if (result.near_zero()) {
                    return Vec3(0.0, 0.0, 0.0);
                }

                break;
            }
        }

        return result;
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