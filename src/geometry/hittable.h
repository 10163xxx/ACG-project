#pragma once

#include "core/ray.h"
#include "core/interval.h"
#include "geometry/aabb.h"
#include "geometry/hit_record.h"
#include "geometry/medium_state.h"
#include "materials/material.h"

#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

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

    bool initialize_medium_state(
        const Vec3& origin,
        MediumState& medium_state
    ) const {
        const std::vector<Vec3> probe_directions = {
            normalize(Vec3(
                1.0,
                0.3713906763541037,
                0.6947465906068658
            )),
            normalize(Vec3(
                -0.5278640450004206,
                1.0,
                0.3183098861837907
            )),
            normalize(Vec3(
                0.2718281828459045,
                -0.6180339887498948,
                1.0
            )),
            normalize(Vec3(
                -1.0,
                -0.4142135623730950,
                0.7320508075688772
            )),
            normalize(Vec3(
                0.5772156649015329,
                1.0,
                -0.7071067811865475
            ))
        };

        for (const Vec3& direction : probe_directions) {
            const Ray probe_ray(origin, direction);

            std::vector<HitRecord> intersections;

            all_intersections(
                probe_ray,
                Interval(
                    -std::numeric_limits<double>::infinity(),
                    0.0
                ),
                intersections
            );

            if (medium_state.initialize_from_intersections(
                    std::move(intersections))) {
                return true;
            }
        }

        medium_state.clear();
        return false;
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

        std::sort(
            intersections.begin(),
            intersections.end(),
            [](const HitRecord& a, const HitRecord& b) {
                return a.t < b.t;
            }
        );

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

    // Transmittance contributed by participating media contained
    // in this geometry hierarchy.
    // Surface interfaces and dielectric absorption are handled
    // separately by transmittance().
    virtual Vec3 volume_transmittance(
        const Ray& ray,
        const Interval& ray_t
    ) const {
        (void)ray;
        (void)ray_t;

        return Vec3(1.0, 1.0, 1.0);
    }

    virtual Vec3 transmittance(
        const Ray& ray,
        const Interval& ray_t
    ) const {
        std::vector<HitRecord> intersections;
        all_intersections(ray, ray_t, intersections);

        std::sort(
            intersections.begin(),
            intersections.end(),
            [](const HitRecord& a, const HitRecord& b) {
                return a.t < b.t;
            }
        );

        // Reconstruct the medium state at the shadow-ray origin.
        MediumState active_media;

        if (!initialize_medium_state(
                ray.origin,
                active_media)) {
            return Vec3(0.0, 0.0, 0.0);
        }

        Vec3 result = volume_transmittance(ray, ray_t);

        const double direction_length = ray.direction.length();
        double previous_t = ray_t.min;

        const auto apply_absorption = [&](
            const Vec3& absorption,
            double distance
        ) {
            if (distance <= 0.0) {
                return;
            }

            result = result * Vec3(
                std::exp(-absorption.x * distance),
                std::exp(-absorption.y * distance),
                std::exp(-absorption.z * distance)
            );
        };

        for (const HitRecord& intersection : intersections) {
            if (intersection.is_volume) {
                continue;
            }

            if (!intersection.material) {
                continue;
            }

            const double segment_distance =
                std::max(0.0, intersection.t - previous_t)
                * direction_length;

            if (!active_media.empty()) {
                apply_absorption(
                    active_media.current_absorption(),
                    segment_distance
                );

                if (result.near_zero()) {
                    return Vec3(0.0, 0.0, 0.0);
                }
            }

            previous_t = intersection.t;

            const Material* material =
                intersection.material.get();

            if (material->is_medium_boundary()) {
                MediumState::Transition transition;

                if (!active_media.prepare_transition(
                        intersection,
                        transition)) {
                    return Vec3(0.0, 0.0, 0.0);
                }

                result = result
                    * material->shadow_transmittance(
                        intersection,
                        ray.direction,
                        transition.incident_ior,
                        transition.transmitted_ior
                    );

                if (result.near_zero()) {
                    return Vec3(0.0, 0.0, 0.0);
                }

                if (!active_media.commit_transition(
                        intersection,
                        transition)) {
                    return Vec3(0.0, 0.0, 0.0);
                }

                continue;
            }

            if (material->blocks_shadow(intersection)) {
                return Vec3(0.0, 0.0, 0.0);
            }

            result = result
                * material->shadow_transmittance(
                    intersection,
                    ray.direction,
                    1.0,
                    1.0
                );

            if (result.near_zero()) {
                return Vec3(0.0, 0.0, 0.0);
            }
        }

        if (!active_media.empty()) {
            const double remaining_distance =
                std::max(0.0, ray_t.max - previous_t)
                * direction_length;

            apply_absorption(
                active_media.current_absorption(),
                remaining_distance
            );
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

    // Sample only the emissive part of this surface.
    // The returned PDF is measured per unit area.
    virtual bool sample_light_surface(
        double u,
        double v,
        HitRecord& record,
        double& pdf_area
    ) const {
        if (!sample_surface(u, v, record, pdf_area)) {
            pdf_area = 0.0;
            return false;
        }

        if (!record.material ||
            !record.material->is_emissive() ||
            pdf_area <= 0.0) {
            pdf_area = 0.0;
            return false;
        }

        return true;
    }

    virtual bool has_emissive_surface() const {
        HitRecord probe;
        double pdf_area = 0.0;

        return sample_light_surface(
            0.5,
            0.5,
            probe,
            pdf_area
        );
    }

    // PDF of sampling this particular emissive surface point.
    // This default implementation is suitable for primitives
    // with a uniform area sampling distribution.
    virtual double light_surface_pdf(
        const HitRecord& record
    ) const {
        if (!record.material ||
            !record.material->is_emissive()) {
            return 0.0;
        }

        HitRecord probe;
        double pdf_area = 0.0;

        if (!sample_light_surface(
                0.5,
                0.5,
                probe,
                pdf_area)) {
            return 0.0;
        }

        return pdf_area;
    }
};