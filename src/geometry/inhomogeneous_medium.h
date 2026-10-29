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

class InhomogeneousMedium final : public Hittable {
public:
    InhomogeneousMedium(
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
                "InhomogeneousMedium requires a boundary."
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
        const double ray_length = ray.direction.length();

        if (!std::isfinite(ray_length) || ray_length <= 0.0) {
            return false;
        }

        const std::vector<Interval> intervals =
            medium_intervals(ray, ray_t);

        for (const Interval& interval : intervals) {
            const double segment_length =
                (interval.max - interval.min) * ray_length;

            double travelled = 0.0;

            while (true) {
                const double xi = std::clamp(
                    Random::random_double(),
                    0.0,
                    std::nextafter(1.0, 0.0)
                );

                // Sample candidate events from the majorant
                // homogeneous Poisson process.
                travelled += -std::log1p(-xi) / density_;

                if (travelled >= segment_length) {
                    break;
                }

                const double t =
                    interval.min + travelled / ray_length;

                const Vec3 point = ray.at(t);
                const double local_density =
                    density_at(point);

                // Delta tracking: accept a candidate according
                // to the local-to-majorant density ratio.
                if (Random::random_double()
                    < local_density / density_) {
                    record.t = t;
                    record.point = point;
                    record.normal = Vec3(1.0, 0.0, 0.0);
                    record.front_face = true;
                    record.u = 0.0;
                    record.v = 0.0;
                    record.is_volume = true;
                    record.material = phase_function_;

                    return true;
                }
            }
        }

        return false;
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

    double transmittance(
        const Ray& ray,
        const Interval& ray_t
    ) const override {
        const double ray_length = ray.direction.length();

        if (!std::isfinite(ray_length) || ray_length <= 0.0) {
            return 1.0;
        }

        const std::vector<Interval> intervals =
            medium_intervals(ray, ray_t);

        double result = 1.0;

        for (const Interval& interval : intervals) {
            const double segment_length =
                (interval.max - interval.min) * ray_length;

            double travelled = 0.0;

            while (true) {
                const double xi = std::clamp(
                    Random::random_double(),
                    0.0,
                    std::nextafter(1.0, 0.0)
                );

                travelled += -std::log1p(-xi) / density_;

                if (travelled >= segment_length) {
                    break;
                }

                const double t =
                    interval.min + travelled / ray_length;

                const double local_density =
                    density_at(ray.at(t));

                // Ratio tracking estimates transmittance through
                // the same spatial density field used by hit().
                result *= 1.0 - local_density / density_;

                if (result <= 1e-12) {
                    return 0.0;
                }
            }
        }

        return result;
    }

    AABB bounding_box() const override {
        return boundary_->bounding_box();
    }

private:
    std::shared_ptr<const Hittable> boundary_;
    double density_;
    std::shared_ptr<const Material> phase_function_;

    // The local density stays in [0.1 * density_, density_],
    // so density_ remains a valid majorant.
    double density_at(const Vec3& point) const {
        const double pattern = (
            std::sin(3.7 * point.x)
            + std::sin(4.3 * point.y)
            + std::sin(3.1 * point.z)
        ) / 3.0;

        const double density_factor =
            0.55 + 0.45 * pattern;

        return density_ * density_factor;
    }

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
                std::max(intersections[i].t, ray_t.min);

            const double t_exit =
                std::min(intersections[i + 1].t, ray_t.max);

            if (t_enter < t_exit) {
                intervals.emplace_back(t_enter, t_exit);
            }
        }

        return intervals;
    }
};
