#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <memory>
#include <vector>

#include "core/ray.h"
#include "core/interval.h"
#include "core/vec3.h"
#include "core/random.h"
#include "geometry/hittable.h"
#include "geometry/hittable_list.h"
#include "materials/point_light.h"
#include "materials/material.h"
#include "materials/environment_map.h"
#include "integrator/mis.h"

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
        return trace_impl(
            initial_ray,
            world,
            nullptr,
            nullptr
        );
    }

    Vec3 trace(
        const Ray& initial_ray,
        const Hittable& world,
        const HittableList& scene_objects
    ) const {
        return trace_impl(
            initial_ray,
            world,
            &scene_objects,
            nullptr
        );
    }

    // NEE API with both area lights and point lights.
    Vec3 trace(
        const Ray& initial_ray,
        const Hittable& world,
        const HittableList& scene_objects,
        const std::vector<PointLight>& point_lights
    ) const {
        return trace_impl(
            initial_ray,
            world,
            &scene_objects,
            &point_lights
        );
    }

    Vec3 trace(
        const Ray& initial_ray,
        const Hittable& world,
        const HittableList& scene_objects,
        const std::vector<PointLight>& point_lights,
        const EnvironmentMap& environment
    ) const {
        return trace_impl(
            initial_ray,
            world,
            &scene_objects,
            &point_lights,
            &environment
        );
    }

private:
    int max_depth_;
    int russian_roulette_start_depth_;

    Vec3 trace_impl(
        const Ray& initial_ray,
        const Hittable& world,
        const HittableList* scene_objects,
        const std::vector<PointLight>* point_lights,
        const EnvironmentMap* environment = nullptr
    ) const {
        Ray ray = initial_ray;

        Vec3 throughput(1.0, 1.0, 1.0);
        Vec3 radiance(0.0, 0.0, 0.0);

        const Interval ray_interval(
            0.001,
            std::numeric_limits<double>::infinity()
        );

        bool previous_scatter_used_nee = false;
        double previous_bsdf_pdf = 0.0;
        Vec3 previous_scatter_point;

        const std::vector<std::size_t> light_indices =
            scene_objects != nullptr
                ? collect_light_indices(*scene_objects)
                : std::vector<std::size_t>{};

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

            if (!hit.material) {
                break;
            }

            const Vec3 emitted = hit.material->emitted(hit);

            double emission_weight = 1.0;

            if (scene_objects != nullptr
                && previous_scatter_used_nee
                && !emitted.near_zero()) {
                const double light_pdf =
                    light_solid_angle_pdf_for_hit(
                        ray,
                        hit,
                        previous_scatter_point,
                        world,
                        *scene_objects,
                        light_indices
                    );

                emission_weight = mis::power_heuristic(
                    previous_bsdf_pdf,
                    light_pdf
                );
            }

            radiance = radiance
                + throughput * emitted * emission_weight;

            if (!hit.is_volume && hit.material->supports_direct_lighting()) {
                if (scene_objects != nullptr) {
                    const Vec3 area_direct =
                        estimate_direct_lighting(
                            hit,
                            *hit.material,
                            ray,
                            world,
                            *scene_objects,
                            light_indices
                        );

                    radiance =
                        radiance + throughput * area_direct;
                }

                if (point_lights != nullptr) {
                    const Vec3 point_direct =
                        estimate_point_lighting(
                            hit,
                            *hit.material,
                            ray,
                            world,
                            *point_lights
                        );

                    radiance =
                        radiance + throughput * point_direct;
                }

                if (environment != nullptr) {
                    const Vec3 environment_direct =
                        estimate_environment_lighting(
                            hit,
                            *hit.material,
                            ray,
                            world,
                            *environment
                        );

                    radiance =
                        radiance
                        + throughput * environment_direct;
                }
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

            previous_scatter_used_nee =
                    !hit.is_volume
                    && scene_objects != nullptr
                    && hit.material->supports_direct_lighting();

            previous_scatter_point = hit.point;

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

    static std::vector<std::size_t> collect_light_indices(
        const HittableList& scene_objects
    ) {
        const auto& objects = scene_objects.objects();

        std::vector<std::size_t> light_indices;
        light_indices.reserve(objects.size());

        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (!objects[i]) {
                continue;
            }

            HitRecord probe;
            double probe_pdf_area = 0.0;

            if (!objects[i]->sample_surface(
                    0.5,
                    0.5,
                    probe,
                    probe_pdf_area)) {
                continue;
            }

            if (!probe.material
                || !probe.material->is_emissive()
                || probe_pdf_area <= 0.0) {
                continue;
            }

            light_indices.push_back(i);
        }

        return light_indices;
    }

    static double light_solid_angle_pdf_for_hit(
        const Ray& ray,
        const HitRecord& light_hit,
        const Vec3& previous_point,
        const Hittable& world,
        const HittableList& scene_objects,
        const std::vector<std::size_t>& light_indices
    ) {
        (void)world;

        const auto& objects = scene_objects.objects();

        if (light_indices.empty()) {
            return 0.0;
        }

        const double selection_pdf =
            1.0 / static_cast<double>(light_indices.size());

        const double distance_squared =
            (light_hit.point - previous_point).length_squared();

        if (distance_squared <= 0.0) {
            return 0.0;
        }

        const Vec3 direction = normalize(ray.direction);

        double total_pdf = 0.0;

        const Interval hit_interval(
            0.001,
            std::numeric_limits<double>::infinity()
        );

        for (const std::size_t index : light_indices) {
            HitRecord candidate;

            if (!objects[index]->hit(
                    ray,
                    hit_interval,
                    candidate)) {
                continue;
            }

            if (!candidate.material
                || !candidate.material->is_emissive()) {
                continue;
            }

            const double hit_tolerance =
                1e-6 * std::max(1.0, std::abs(light_hit.t));

            if (std::abs(candidate.t - light_hit.t)
                > hit_tolerance) {
                continue;
            }

            HitRecord probe;
            double pdf_area = 0.0;

            if (!objects[index]->sample_surface(
                    0.5,
                    0.5,
                    probe,
                    pdf_area)) {
                continue;
            }

            const double cos_light =
                dot(candidate.normal, -direction);

            if (cos_light <= 0.0 || pdf_area <= 0.0) {
                continue;
            }

            const double pdf =
                mis::area_to_solid_angle_pdf(
                    pdf_area * selection_pdf,
                    distance_squared,
                    cos_light
                );

            total_pdf += pdf;
        }

        return total_pdf;
    }

    static Vec3 estimate_environment_lighting(
        const HitRecord& hit,
        const Material& material,
        const Ray& incoming_ray,
        const Hittable& world,
        const EnvironmentMap& environment
    ) {
        constexpr double EPSILON = 0.001;
        constexpr double MIN_TRANSMITTANCE = 1e-12;

        double light_pdf = 0.0;

        const Vec3 wi =
            environment.sample_direction(light_pdf);

        if (light_pdf <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Vec3 shading_normal =
            material.shading_normal(incoming_ray, hit);

        const double cos_surface =
            dot(shading_normal, wi);

        if (cos_surface <= 0.0
            || dot(hit.normal, wi) <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Vec3 shadow_origin =
            hit.point + EPSILON * hit.normal;

        const Ray shadow_ray(shadow_origin, wi);

        const Interval shadow_interval(
            EPSILON,
            std::numeric_limits<double>::infinity()
        );

        HitRecord blocker;

        // Only opaque surfaces block the environment light.
        if (world.hit_surface(
                shadow_ray,
                shadow_interval,
                blocker)) {
            return Vec3(0.0, 0.0, 0.0);
        }

        // Volumes attenuate the shadow ray instead of acting
        // as opaque blockers.
        const double shadow_transmittance =
            world.transmittance(
                shadow_ray,
                shadow_interval
            );

        if (shadow_transmittance <= MIN_TRANSMITTANCE) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Vec3 wo =
            normalize(-incoming_ray.direction);

        const Vec3 bsdf =
            material.evaluate_bsdf(hit, wo, wi);

        const double bsdf_pdf =
            material.scattering_pdf(hit, wo, wi);

        const double mis_weight =
            mis::power_heuristic(
                light_pdf,
                bsdf_pdf
            );

        const Vec3 environment_radiance =
            environment.evaluate(wi);

        return environment_radiance
            * bsdf
            * (
                cos_surface
                * mis_weight
                * shadow_transmittance
                / light_pdf
            );
    }

    static Vec3 estimate_point_lighting(
        const HitRecord& hit,
        const Material& material,
        const Ray& incoming_ray,
        const Hittable& world,
        const std::vector<PointLight>& point_lights
    ) {
        constexpr double EPSILON = 0.001;
        constexpr double MIN_TRANSMITTANCE = 1e-12;

        Vec3 total_radiance(0.0, 0.0, 0.0);

        const Vec3 wo =
            normalize(-incoming_ray.direction);

        const Vec3 shading_normal =
            material.shading_normal(incoming_ray, hit);

        for (const PointLight& light : point_lights) {
            const Vec3 to_light =
                light.position - hit.point;

            const double distance_squared =
                to_light.length_squared();

            if (distance_squared <= EPSILON * EPSILON) {
                continue;
            }

            const double distance =
                std::sqrt(distance_squared);

            const Vec3 wi =
                to_light / distance;

            // Reject light arriving from below the geometric surface.
            const double cos_geometric =
                dot(hit.normal, wi);

            if (cos_geometric <= 0.0) {
                continue;
            }

            const double cos_surface =
                dot(shading_normal, wi);

            if (cos_surface <= 0.0) {
                continue;
            }

            // Offset the shadow ray to avoid self-intersection.
            const Vec3 shadow_origin =
                hit.point + EPSILON * hit.normal;

            const Vec3 shadow_to_light =
                light.position - shadow_origin;

            const double shadow_distance =
                shadow_to_light.length();

            if (shadow_distance <= EPSILON) {
                continue;
            }

            const Ray shadow_ray(
                shadow_origin,
                shadow_to_light / shadow_distance
            );

            const Interval shadow_interval(
                EPSILON,
                shadow_distance - EPSILON
            );

            if (shadow_interval.max <= shadow_interval.min) {
                continue;
            }

            HitRecord blocker;

            // Opaque geometry blocks the point light.
            if (world.hit_surface(
                    shadow_ray,
                    shadow_interval,
                    blocker)) {
                continue;
            }

            // Participating media attenuate the light.
            const double shadow_transmittance =
                world.transmittance(
                    shadow_ray,
                    shadow_interval
                );

            if (shadow_transmittance <= MIN_TRANSMITTANCE) {
                continue;
            }

            const Vec3 bsdf =
                material.evaluate_bsdf(
                    hit,
                    wo,
                    wi
                );

            // Point lights are delta lights. No area-light MIS.
            const double attenuation =
                1.0 / distance_squared;

            total_radiance =
                total_radiance
                + light.intensity
                * bsdf
                * (
                    cos_surface
                    * attenuation
                    * shadow_transmittance
                );
        }

        return total_radiance;
    }
    
    static Vec3 estimate_direct_lighting(
        const HitRecord& hit,
        const Material& material,
        const Ray& incoming_ray,
        const Hittable& world,
        const HittableList& scene_objects,
        const std::vector<std::size_t>& light_indices
    ) {
        constexpr int LIGHT_SAMPLES = 4;

        Vec3 total_radiance(0.0, 0.0, 0.0);

        for (int i = 0; i < LIGHT_SAMPLES; ++i) {
            total_radiance =
                total_radiance
                + estimate_direct_lighting_single(
                    hit,
                    material,
                    incoming_ray,
                    world,
                    scene_objects,
                    light_indices
                );
        }

        return total_radiance
            / static_cast<double>(LIGHT_SAMPLES);
    }

    static Vec3 estimate_direct_lighting_single(
        const HitRecord& hit,
        const Material& material,
        const Ray& incoming_ray,
        const Hittable& world,
        const HittableList& scene_objects,
        const std::vector<std::size_t>& light_indices
    ) {
        constexpr double EPSILON = 0.001;
        constexpr double MIN_DISTANCE = 2.0 * EPSILON;
        constexpr double MIN_TRANSMITTANCE = 1e-12;

        const auto& objects = scene_objects.objects();

        // No sampleable emissive objects: no direct-light contribution.
        if (light_indices.empty()) {
            return Vec3(0.0, 0.0, 0.0);
        }

        // Choose uniformly among emissive objects only.
        const double selection_pdf =
            1.0 / static_cast<double>(light_indices.size());

        std::size_t light_choice = static_cast<std::size_t>(
            Random::random_double()
            * static_cast<double>(light_indices.size())
        );

        if (light_choice >= light_indices.size()) {
            light_choice = light_indices.size() - 1;
        }

        const std::size_t selected_index =
            light_indices[light_choice];

        HitRecord light_sample;
        double pdf_area = 0.0;

        if (!objects[selected_index]->sample_surface(
                Random::random_double(),
                Random::random_double(),
                light_sample,
                pdf_area)) {
            return Vec3(0.0, 0.0, 0.0);
        }

        if (!light_sample.material
            || !light_sample.material->is_emissive()
            || pdf_area <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        // Calculate lighting geometry from the actual shading point.
        const Vec3 to_light =
            light_sample.point - hit.point;

        const double distance_squared =
            to_light.length_squared();

        if (distance_squared <= MIN_DISTANCE * MIN_DISTANCE) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const double distance =
            std::sqrt(distance_squared);

        const Vec3 wi =
            to_light / distance;

        // Reject light arriving from below the geometric surface.
        const double cos_geometric =
            dot(hit.normal, wi);

        if (cos_geometric <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Vec3 wo =
            normalize(-incoming_ray.direction);

        const Vec3 shading_normal =
            material.shading_normal(incoming_ray, hit);

        const double cos_surface =
            dot(shading_normal, wi);

        if (cos_surface <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        // The light emits only from its outward-facing side.
        const double cos_light =
            dot(light_sample.normal, -wi);

        if (cos_light <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        light_sample.front_face = true;

        const Vec3 light_emission =
            light_sample.material->emitted(light_sample);

        if (light_emission.near_zero()) {
            return Vec3(0.0, 0.0, 0.0);
        }

        // Offset only the visibility ray, not the MIS reference point.
        const Vec3 shadow_origin =
            hit.point + EPSILON * hit.normal;

        const Vec3 shadow_to_light =
            light_sample.point - shadow_origin;

        const double shadow_distance =
            shadow_to_light.length();

        if (shadow_distance <= EPSILON) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Vec3 shadow_direction =
            shadow_to_light / shadow_distance;

        const Interval shadow_interval(
            EPSILON,
            shadow_distance - EPSILON
        );

        if (shadow_interval.max <= shadow_interval.min) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Ray shadow_ray(
            shadow_origin,
            shadow_direction
        );

        HitRecord blocker;

        // Opaque surfaces block the sampled area light.
        if (world.hit_surface(
                shadow_ray,
                shadow_interval,
                blocker)) {
            return Vec3(0.0, 0.0, 0.0);
        }

        // Media attenuate the sampled light along its shadow ray.
        const double shadow_transmittance =
            world.transmittance(
                shadow_ray,
                shadow_interval
            );

        if (shadow_transmittance <= MIN_TRANSMITTANCE) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Vec3 bsdf =
            material.evaluate_bsdf(hit, wo, wi);

        const double light_pdf =
            mis::area_to_solid_angle_pdf(
                pdf_area * selection_pdf,
                distance_squared,
                cos_light
            );

        if (light_pdf <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const double bsdf_pdf =
            material.scattering_pdf(
                hit,
                wo,
                wi
            );

        const double mis_weight =
            mis::power_heuristic(
                light_pdf,
                bsdf_pdf
            );

        const double weight =
            cos_surface
            * mis_weight
            * shadow_transmittance
            / light_pdf;

        return light_emission * bsdf * weight;
    }

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
