#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

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
        const double infinity =
            std::numeric_limits<double>::infinity();

        HitRecord entry;
        HitRecord exit;

        // Find the first boundary crossing.
        if (!boundary_->hit(
                ray,
                Interval(-infinity, infinity),
                entry)) {
            return false;
        }

        // Find the second crossing, starting just after entry.
        if (!boundary_->hit(
                ray,
                Interval(entry.t + 1e-6, infinity),
                exit)) {
            return false;
        }

        double t_enter = entry.t;
        double t_exit = exit.t;

        if (t_enter < ray_t.min) {
            t_enter = ray_t.min;
        }

        if (t_exit > ray_t.max) {
            t_exit = ray_t.max;
        }

        if (t_enter >= t_exit) {
            return false;
        }

        if (t_enter < 0.0) {
            t_enter = 0.0;
        }

        const double ray_length = ray.direction.length();

        if (ray_length <= 0.0) {
            return false;
        }

        const double distance_inside =
            (t_exit - t_enter) * ray_length;

        // Exponential free-flight sampling:
        // distance = -ln(1 - xi) / sigma_t.
        const double xi = std::clamp(
            Random::random_double(),
            0.0,
            std::nextafter(1.0, 0.0)
        );

        const double sampled_distance =
            -std::log1p(-xi) / density_;

        if (sampled_distance >= distance_inside) {
            return false;
        }

        const double t =
            t_enter + sampled_distance / ray_length;

        if (!ray_t.contains(t)) {
            return false;
        }

        record.t = t;
        record.point = ray.at(t);

        // A volume event has no geometric surface normal.
        // Keep the placeholder only for compatibility with existing interfaces.
        record.normal = Vec3(1.0, 0.0, 0.0);
        record.front_face = true;
        record.u = 0.0;
        record.v = 0.0;
        record.is_volume = true;
        record.material = phase_function_;

        return true;
    }

    bool hit_surface(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override {
        (void)ray;
        (void)ray_t;
        (void)record;

        // A participating medium is not an opaque surface.
        return false;
    }

    double transmittance(
        const Ray& ray,
        const Interval& ray_t
    ) const override {
        const double infinity =
            std::numeric_limits<double>::infinity();

        HitRecord entry;
        HitRecord exit;

        if (!boundary_->hit(
                ray,
                Interval(-infinity, infinity),
                entry)) {
            return 1.0;
        }

        if (!boundary_->hit(
                ray,
                Interval(
                    entry.t + 1e-6,
                    infinity
                ),
                exit)) {
            return 1.0;
        }

        const double t_enter =
            std::max(entry.t, ray_t.min);

        const double t_exit =
            std::min(exit.t, ray_t.max);

        if (t_enter >= t_exit) {
            return 1.0;
        }

        const double distance_inside =
            (t_exit - t_enter)
            * ray.direction.length();

        return std::exp(
            -density_ * distance_inside
        );
    }

    AABB bounding_box() const override {
        return boundary_->bounding_box();
    }

private:
    std::shared_ptr<const Hittable> boundary_;
    double density_;
    std::shared_ptr<const Material> phase_function_;
};
