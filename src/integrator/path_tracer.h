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

    // Existing API: path tracing without explicit NEE lights.
    Vec3 trace(
        const Ray& initial_ray,
        const Hittable& world
    ) const {
        return trace_impl(initial_ray, world, nullptr);
    }

    // NEE API: world is used for intersections and visibility;
    // scene_objects provides the objects available for sampling.
    Vec3 trace(
        const Ray& initial_ray,
        const Hittable& world,
        const HittableList& scene_objects
    ) const {
        return trace_impl(
            initial_ray, world, &scene_objects
        );
    }

private:
    int max_depth_;
    int russian_roulette_start_depth_;

    Vec3 trace_impl(
        const Ray& initial_ray,
        const Hittable& world,
        const HittableList* scene_objects
    ) const {
        Ray ray = initial_ray;

        Vec3 throughput(1.0, 1.0, 1.0);
        Vec3 radiance(0.0, 0.0, 0.0);

        const Interval ray_interval(
            0.001,
            std::numeric_limits<double>::infinity()
        );

        // True when the previous scattering event was a
        // non-delta event whose direct light is estimated by NEE.
        bool suppress_next_emission = false;

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

            const Vec3 emitted = hit.material->emitted(hit);

            // Prevent double counting: NEE handles the direct
            // contribution after Lambertian scattering.
            if (!suppress_next_emission) {
                radiance =
                    radiance + throughput * emitted;
            }

            // Estimate direct illumination at this surface.
            if (scene_objects != nullptr &&
                hit.material->supports_direct_lighting()) {
                const Vec3 direct = estimate_direct_lighting(
                    hit,
                    *hit.material,
                    ray,
                    world,
                    *scene_objects
                );

                radiance = radiance + throughput * direct;
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

            suppress_next_emission =
                scene_objects != nullptr &&
                hit.material->supports_direct_lighting();

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

    static Vec3 estimate_direct_lighting(
        const HitRecord& hit,
        const Material& material,
        const Ray& incoming_ray,
        const Hittable& world,
        const HittableList& scene_objects
    ) {
        constexpr double EPSILON = 0.001;
        constexpr double MIN_DISTANCE = 2.0 * EPSILON;

        const auto& objects = scene_objects.objects();

        // First collect objects that can be sampled and whose
        // sampled surface belongs to an emissive material.
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

            if (!probe.material ||
                !probe.material->is_emissive() ||
                probe_pdf_area <= 0.0) {
                continue;
            }

            light_indices.push_back(i);
        }

        // No sampleable emissive objects: no direct-light contribution.
        if (light_indices.empty()) {
            return Vec3(0.0, 0.0, 0.0);
        }

        // Choose uniformly among emissive objects only.
        const double selection_pdf =
            1.0 / static_cast<double>(light_indices.size());

        std::size_t light_choice = static_cast<std::size_t>(
            Random::random_double() *
            static_cast<double>(light_indices.size())
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

        if (!light_sample.material ||
            !light_sample.material->is_emissive() ||
            pdf_area <= 0.0) {  
            return Vec3(0.0, 0.0, 0.0);
        }

        // Offset the shadow ray to reduce self-intersections.
        const Vec3 shadow_origin =
            hit.point + EPSILON * hit.normal;

        const Vec3 to_light =
            light_sample.point - shadow_origin;

        const double distance_squared =
            to_light.length_squared();

        if (distance_squared <=
            MIN_DISTANCE * MIN_DISTANCE) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const double distance = std::sqrt(distance_squared);
        const Vec3 wi = to_light / distance;

        // Keep the geometric normal check to prevent light
        // arriving from below the actual surface.
        const double cos_geometric =
            dot(hit.normal, wi);

        if (cos_geometric <= 0.0) {
            return Vec3(0.0, 0.0, 0.0);
        }

        // Use the material's shading normal for the
        // direct-light cosine term.
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

        // Shadow ray: reject samples blocked before reaching
        // the sampled point on the light.
        HitRecord blocker;
        const Interval shadow_interval(
            EPSILON,
            distance - EPSILON
        );

        if (shadow_interval.max <= shadow_interval.min) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Ray shadow_ray(shadow_origin, wi);

        if (world.hit(shadow_ray, shadow_interval, blocker)) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Vec3 bsdf =
            material.evaluate_bsdf(hit, wo, wi);

        // Area-measure estimator, including object-selection PDF:
        //
        // Le * f * cos_surface * cos_light
        // -----------------------------------------------
        // distance^2 * pdf_area * selection_pdf
        const double weight =
            cos_surface * cos_light /
            (distance_squared * pdf_area * selection_pdf);

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
