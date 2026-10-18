#pragma once

#include "core/ray.h"
#include "core/interval.h"
#include "geometry/aabb.h"
#include "geometry/hit_record.h"

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
    virtual bool hit_surface(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const {
        if (!hit(ray, ray_t, record)) {
            return false;
        }

        return !record.is_volume;
    }

    virtual double transmittance(
        const Ray& ray,
        const Interval& ray_t
    ) const {
        (void)ray;
        (void)ray_t;

        // Opaque surfaces are handled by hit_surface().
        // Objects without a participating medium do not attenuate light.
        return 1.0;
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