#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "core/ray.h"
#include "geometry/hittable_list.h"
#include "integrator/path_tracer.h"

int main() {
    const HittableList empty_world;
    const PathTracer tracer(20, 5);

    const Ray upward_ray(
        Vec3(0.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0)
    );

    const Vec3 result =
        tracer.trace(upward_ray, empty_world);

    assert(std::abs(result.x - 0.5) < 1e-12);
    assert(std::abs(result.y - 0.7) < 1e-12);
    assert(std::abs(result.z - 1.0) < 1e-12);

    bool rejected_zero_max_depth = false;

    try {
        const PathTracer invalid_tracer(0, 5);
        (void)invalid_tracer;
    } catch (const std::invalid_argument&) {
        rejected_zero_max_depth = true;
    }

    assert(rejected_zero_max_depth);

    bool rejected_zero_rr_depth = false;

    try {
        const PathTracer invalid_tracer(20, 0);
        (void)invalid_tracer;
    } catch (const std::invalid_argument&) {
        rejected_zero_rr_depth = true;
    }

    assert(rejected_zero_rr_depth);

    std::cout << "All path tracer tests passed.\n";
    return 0;
}
