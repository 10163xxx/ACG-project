#pragma once

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "core/ray.h"
#include "core/interval.h"
#include "core/vec3.h"
#include "core/random.h"
#include "geometry/hittable.h"
#include "materials/material.h"

class PathTracer {
public:
    explicit PathTracer(
        int max_depth = 20,
        int russian_roulette_start_depth = 5
    )
        : max_depth_(max_depth),
          russian_roulette_start_depth_(
              russian_roulette_start_depth
          ) {
        if (max_depth_ <= 0) {
            throw std::invalid_argument(
                "PathTracer max_depth must be positive."
            );
        }

        if (russian_roulette_start_depth_ <= 0) {
            throw std::invalid_argument(
                "Russian Roulette start depth must be positive."
            );
        }
    }

    Vec3 trace(
        const Ray& initial_ray,
        const Hittable& world
    ) const {
        Ray ray = initial_ray;

        Vec3 throughput(1.0, 1.0, 1.0);
        Vec3 radiance(0.0, 0.0, 0.0);

        const Interval ray_interval(
            0.001,
            std::numeric_limits<double>::infinity()
        );

        for (int depth = 0; depth < max_depth_; ++depth) {
            HitRecord hit;

            if (!world.hit(ray, ray_interval, hit)) {
                radiance =
                    radiance + throughput * background(ray);
                break;
            }

            if (!hit.material) {
                break;
            }

            Vec3 attenuation;
            Ray scattered;

            if (!hit.material->scatter(
                    ray,
                    hit,
                    attenuation,
                    scattered)) {
                break;
            }

            throughput = throughput * attenuation;
            ray = scattered;

            if (throughput.near_zero()) {
                break;
            }

            const int bounce_count = depth + 1;

            if (bounce_count >= russian_roulette_start_depth_
                && bounce_count < max_depth_) {

                const double survival_probability =
                    russian_roulette_probability(throughput);

                if (Random::random_double()
                    >= survival_probability) {
                    break;
                }

                throughput =
                    throughput / survival_probability;
            }
        }

        return radiance;
    }

private:
    int max_depth_;
    int russian_roulette_start_depth_;

    static double russian_roulette_probability(
        const Vec3& throughput
    ) {
        const double max_component = std::max({
            throughput.x,
            throughput.y,
            throughput.z
        });

        constexpr double min_survival_probability = 0.05;
        constexpr double max_survival_probability = 0.95;

        return std::clamp(
            max_component,
            min_survival_probability,
            max_survival_probability
        );
    }

    static Vec3 background(const Ray& ray) {
        const Vec3 unit_direction =
            normalize(ray.direction);

        const double t =
            0.5 * (unit_direction.y + 1.0);

        const Vec3 white(1.0, 1.0, 1.0);
        const Vec3 sky(0.5, 0.7, 1.0);

        return (1.0 - t) * white + t * sky;
    }
};
