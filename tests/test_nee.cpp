#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

#include "core/random.h"
#include "core/ray.h"
#include "geometry/hittable_list.h"
#include "geometry/sphere.h"
#include "integrator/path_tracer.h"
#include "materials/diffuse_light.h"
#include "materials/lambertian.h"

int main() {
    // 1. Verify emissive material identification.
    const auto diffuse_material =
        std::make_shared<Lambertian>(
            Vec3(0.8, 0.8, 0.8)
        );

    const auto light_material =
        std::make_shared<DiffuseLight>(
            Vec3(4.0, 4.0, 4.0)
        );

    assert(!diffuse_material->is_emissive());
    assert(light_material->is_emissive());

    // A single bounce isolates the direct-light estimate.
    const PathTracer tracer(1, 5);

    const Ray camera_ray(
        Vec3(0.0, 0.0, 3.0),
        Vec3(0.0, 0.0, -1.0)
    );

    // 2. A diffuse surface without an emissive object
    // should produce no direct-light contribution.
    HittableList no_light_scene;

    no_light_scene.add(
        std::make_shared<Sphere>(
            Vec3(0.0, 0.0, 0.0),
            1.0,
            diffuse_material
        )
    );

    Random::seed(12345);

    const Vec3 no_light_result = tracer.trace(
        camera_ray,
        no_light_scene,
        no_light_scene
    );

    assert(std::abs(no_light_result.x) < 1e-12);
    assert(std::abs(no_light_result.y) < 1e-12);
    assert(std::abs(no_light_result.z) < 1e-12);

    // 3. With an emissive sphere present, at least one of
    // several deterministic seeds should produce a visible
    // positive direct-light sample.
    HittableList lit_scene;

    lit_scene.add(
        std::make_shared<Sphere>(
            Vec3(0.0, 0.0, 0.0),
            1.0,
            diffuse_material
        )
    );

    lit_scene.add(
        std::make_shared<Sphere>(
            Vec3(2.0, 2.0, 1.5),
            0.5,
            light_material
        )
    );

    bool found_positive_direct_light = false;

    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        Random::seed(seed);

        const Vec3 result = tracer.trace(
            camera_ray,
            lit_scene,
            lit_scene
        );

        if (result.x > 1e-9 ||
            result.y > 1e-9 ||
            result.z > 1e-9) {
            found_positive_direct_light = true;
            break;
        }
    }

    assert(found_positive_direct_light);

    std::cout << "All NEE tests passed.\n";
    return 0;
}
