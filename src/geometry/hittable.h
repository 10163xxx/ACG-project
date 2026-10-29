#pragma once

#include "core/ray.h"
#include "core/interval.h"
#include "geometry/aabb.h"
#include "geometry/hit_record.h"
#include "materials/material.h"

#include <vector>

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

    virtual double transmittance(
        const Ray& ray,
        const Interval& ray_t
    ) const {
        std::vector<HitRecord> intersections;
        all_intersections(ray, ray_t, intersections);

        double result = 1.0;

        for (const HitRecord& intersection : intersections) {
            if (intersection.is_volume ||
                !intersection.material) {
                continue;
            }

            result *=
                intersection.material->shadow_transmittance(
                    intersection,
                    ray.direction
                );

            if (result <= 1e-12) {
                return 0.0;
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