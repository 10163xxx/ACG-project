#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "geometry/hittable.h"
#include "materials/isotropic.h"
#include "core/random.h"

class ConstantMedium final : public Hittable {
public:
    ConstantMedium(
        std::shared_ptr<const Hittable> boundary,
        double density,
        const Vec3& albedo
    )
        : boundary_(std::move(boundary)),
          density_(density),
          phase_function_(
              std::make_shared<Isotropic>(albedo)
          ) {
        if (!boundary_) {
            throw std::invalid_argument(
                "ConstantMedium requires a boundary."
            );
        }

        if (!boundary_->is_closed_volume()) {
            throw std::invalid_argument(
                "ConstantMedium requires a closed volume boundary."
            );
        }

        if (!std::isfinite(density_) || density_ <= 0.0) {
            throw std::invalid_argument(
                "Medium density must be finite and positive."
            );
        }

        if (!std::isfinite(albedo.x)
            || !std::isfinite(albedo.y)
            || !std::isfinite(albedo.z)
            || albedo.x < 0.0 || albedo.x > 1.0
            || albedo.y < 0.0 || albedo.y > 1.0
            || albedo.z < 0.0 || albedo.z > 1.0) {
            throw std::invalid_argument(
                "Medium albedo must be in [0, 1]."
            );
        }
    }

    bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override {
        const std::vector<Interval> intervals =
            medium_intervals(ray, ray_t);

        if (intervals.empty()) {
            return false;
        }

        const double ray_length =
            ray.direction.length();

        double total_distance = 0.0;

        for (const Interval& interval : intervals) {
            total_distance +=
                (interval.max - interval.min) * ray_length;
        }

        if (total_distance <= 0.0) {
            return false;
        }

        const double xi = std::clamp(
            Random::random_double(),
            0.0,
            std::nextafter(1.0, 0.0)
        );

        const double sampled_distance =
            -std::log1p(-xi) / density_;

        if (sampled_distance >= total_distance) {
            return false;
        }

        double remaining_distance = sampled_distance;

        for (const Interval& interval : intervals) {
            const double interval_distance =
                (interval.max - interval.min) * ray_length;

            if (remaining_distance < interval_distance) {
                const double t =
                    interval.min
                    + remaining_distance / ray_length;

                record.t = t;
                record.point = ray.at(t);
                record.normal = Vec3(1.0, 0.0, 0.0);
                record.front_face = true;
                record.u = 0.0;
                record.v = 0.0;
                record.is_volume = true;
                record.material = phase_function_;

                return true;
            }

            remaining_distance -= interval_distance;
        }

        return false;
    }

    void all_intersections(
        const Ray& ray,
        const Interval& ray_t,
        std::vector<HitRecord>& intersections
    ) const override {
        (void)ray;
        (void)ray_t;
        (void)intersections;
    }

    bool hit_surface(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override {
        (void)ray;
        (void)ray_t;
        (void)record;

        return false;
    }

    Vec3 volume_transmittance(
        const Ray& ray,
        const Interval& ray_t
    ) const override {
        const std::vector<Interval> intervals =
            medium_intervals(ray, ray_t);

        const double ray_length =
            ray.direction.length();

        double total_distance = 0.0;

        for (const Interval& interval : intervals) {
            total_distance +=
                (interval.max - interval.min) * ray_length;
        }

        const double result =
            std::exp(-density_ * total_distance);

        return Vec3(result, result, result);
    }

    AABB bounding_box() const override {
        return boundary_->bounding_box();
    }

private:
    std::shared_ptr<const Hittable> boundary_;
    double density_;
    std::shared_ptr<const Material> phase_function_;

    std::vector<Interval> medium_intervals(
        const Ray& ray,
        const Interval& ray_t
    ) const {
        const double infinity =
            std::numeric_limits<double>::infinity();

        std::vector<HitRecord> intersections;

        boundary_->all_intersections(
            ray,
            Interval(-infinity, infinity),
            intersections
        );

        std::sort(
            intersections.begin(),
            intersections.end(),
            [](const HitRecord& a, const HitRecord& b) {
                return a.t < b.t;
            }
        );

        std::vector<Interval> intervals;
        intervals.reserve(intersections.size() / 2);

        for (std::size_t i = 0;
             i + 1 < intersections.size();
             i += 2) {
            const double t_enter =
                std::max(
                    intersections[i].t,
                    ray_t.min
                );

            const double t_exit =
                std::min(
                    intersections[i + 1].t,
                    ray_t.max
                );

            if (t_enter < t_exit) {
                intervals.emplace_back(
                    t_enter,
                    t_exit
                );
            }
        }

        return intervals;
    }
};
