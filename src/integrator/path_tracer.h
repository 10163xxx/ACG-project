#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
#include <memory>

#include "core/ray.h"
#include "core/interval.h"
#include "core/vec3.h"
#include "core/random.h"
#include "geometry/hittable.h"
#include "geometry/hittable_list.h"
#include "geometry/medium_state.h"
#include "materials/point_light.h"
#include "materials/material.h"
#include "materials/environment_map.h"
#include "integrator/mis.h"
#include "integrator/direct_lighting.h"

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
        const Hittable& world,
        const HittableList& scene_objects,
        const std::vector<PointLight>* point_lights = nullptr,
        const EnvironmentMap* environment = nullptr,
        const std::vector<std::size_t>* precomputed_light_indices = nullptr
    ) const {
        Ray ray = initial_ray;

        Vec3 throughput(1.0, 1.0, 1.0);
        Vec3 radiance(0.0, 0.0, 0.0);
        MediumState active_media;

        const Vec3 probe_direction = normalize(
            Vec3(1.0, 0.3713906763541037, 0.6947465906068658)
        );

        const Ray probe_ray(
            initial_ray.origin,
            probe_direction
        );

        std::vector<HitRecord> previous_intersections;

        world.all_intersections(
            probe_ray,
            Interval(
                -std::numeric_limits<double>::infinity(),
                0.0
            ),
            previous_intersections
        );

        if (!active_media.initialize_from_intersections(
                std::move(previous_intersections))) {
            throw std::runtime_error(
                "Failed to initialize the camera ray medium state."
            );
        }

        const Interval ray_interval(
            0.001,
            std::numeric_limits<double>::infinity()
        );

        bool previous_scatter_used_nee = false;
        double previous_bsdf_pdf = 0.0;
        int previous_area_light_samples =
            DirectLighting::AREA_LIGHT_SAMPLES;
        Vec3 previous_scatter_point;

        const std::vector<std::size_t> owned_light_indices =
            precomputed_light_indices == nullptr
                ? DirectLighting::collect_light_indices(scene_objects)
                : std::vector<std::size_t>{};

        const std::vector<std::size_t>& light_indices =
            precomputed_light_indices != nullptr
                ? *precomputed_light_indices
                : owned_light_indices;

        for (int depth = 0; depth < max_depth_; ++depth) {
            HitRecord hit;

            if (!world.hit(ray, ray_interval, hit)) {
                const Vec3 environment_radiance =
                    environment != nullptr
                        ? environment->evaluate(ray.direction)
                        : background(ray);

                double environment_weight = 1.0;

                if (environment != nullptr
                    && previous_scatter_used_nee) {
                    const double environment_pdf =
                        environment->pdf(ray.direction);

                    environment_weight =
                        mis::power_heuristic(
                            previous_bsdf_pdf,
                            environment_pdf
                        );
                }

                radiance = radiance
                    + throughput
                    * environment_radiance
                    * environment_weight;

                break;
            }

            // Only the innermost active medium attenuates this segment.
            if (!active_media.empty()) {
                const double segment_distance =
                    hit.t * ray.direction.length();

                if (std::isfinite(segment_distance) &&
                    segment_distance >= 0.0) {
                    const Vec3 absorption =
                        active_media.current_absorption();

                    throughput = throughput * Vec3(
                        std::exp(-absorption.x * segment_distance),
                        std::exp(-absorption.y * segment_distance),
                        std::exp(-absorption.z * segment_distance)
                    );
                }

                if (throughput.near_zero()) {
                    break;
                }
            }

            // Advance the ray-cone radius to the hit point.
            // Ray directions are not guaranteed to be normalized,
            // so convert the hit parameter to world-space distance.
            const double ray_length =
                ray.direction.length();

            const double hit_distance =
                hit.t * ray_length;

            if (std::isfinite(hit_distance) &&
                hit_distance >= 0.0 &&
                std::isfinite(ray.cone_width) &&
                std::isfinite(ray.cone_spread) &&
                ray.cone_width >= 0.0 &&
                ray.cone_spread >= 0.0) {
                hit.ray_cone_width =
                    ray.cone_width
                    + hit_distance * ray.cone_spread;

                hit.has_ray_cone =
                    std::isfinite(hit.ray_cone_width);
            }

            if (!hit.material) {
                break;
            }

            const Vec3 emitted = hit.material->emitted(hit);

            double emission_weight = 1.0;

            if (previous_scatter_used_nee
                && !emitted.near_zero()) {
                const double light_pdf =
                    DirectLighting::light_solid_angle_pdf_for_hit(
                        ray,
                        hit,
                        previous_scatter_point,
                        world,
                        scene_objects,
                        light_indices
                    );

                emission_weight =
                    mis::power_heuristic(
                        previous_bsdf_pdf,
                        DirectLighting::BSDF_SAMPLES,
                        light_pdf,
                        previous_area_light_samples
                    );
            }

            radiance = radiance
                + throughput * emitted * emission_weight;

            const Vec3 direct_lighting =
                hit.is_volume
                    ? DirectLighting::estimate_volume_lighting(
                        hit,
                        *hit.material,
                        ray,
                        world,
                        &scene_objects,
                        light_indices,
                        point_lights,
                        environment
                    )
                    : DirectLighting::estimate_surface_lighting(
                        hit,
                        *hit.material,
                        ray,
                        world,
                        &scene_objects,
                        light_indices,
                        point_lights,
                        environment
                    );

            radiance = radiance + throughput * direct_lighting;

            Vec3 attenuation;
            Ray scattered;
            ScatterSampleInfo scatter_info;

            MediumState::Transition medium_transition;

            if (hit.material->is_medium_boundary()) {
                if (!active_media.prepare_transition(
                        hit,
                        medium_transition)) {
                    throw std::runtime_error(
                        "Dielectric medium boundary transition is invalid."
                    );
                }

                scatter_info.incident_ior =
                    medium_transition.incident_ior;

                scatter_info.transmitted_ior =
                    medium_transition.transmitted_ior;
            }

            if (!hit.material->scatter(
                    ray,
                    hit,
                    attenuation,
                    scattered,
                    scatter_info)) {
                break;
            }

            // Update the medium state only after actual transmission.
            // Reflection and total internal reflection leave it unchanged.
            if (scatter_info.is_transmission &&
                hit.material->is_medium_boundary()) {
                if (!active_media.commit_transition(
                        hit,
                        medium_transition)) {
                    throw std::runtime_error(
                        "Failed to commit dielectric medium transition."
                    );
                }
            }

            // Carry the cone footprint from the hit point
            // to the next ray's origin.
            if (hit.has_ray_cone) {
                scattered.cone_width =
                    hit.ray_cone_width;

                scattered.cone_spread =
                    ray.cone_spread;
            }

            previous_scatter_used_nee =
                !scatter_info.is_straight_through
                && (
                    hit.is_volume
                        ? true
                        : hit.material->supports_direct_lighting()
                );

            previous_scatter_point = hit.point;

            // Surface direct lighting uses multiple area-light samples.
            // Volume direct lighting currently uses one area-light sample.
            previous_area_light_samples =
                hit.is_volume
                    ? DirectLighting::VOLUME_LIGHT_SAMPLES
                    : DirectLighting::AREA_LIGHT_SAMPLES;

            if (previous_scatter_used_nee) {
                const Vec3 wo =
                    normalize(-ray.direction);

                const Vec3 wi =
                    normalize(scattered.direction);

                previous_bsdf_pdf =
                    hit.material->scattering_pdf(
                        hit,
                        wo,
                        wi
                    );
            } else {
                previous_bsdf_pdf = 0.0;
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
