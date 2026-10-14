#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "acceleration/bvh.h"
#include "geometry/hittable_list.h"
#include "geometry/sphere.h"

namespace {
void compare_hit_results(
    const Hittable& linear_world,
    const Hittable& bvh,
    const Ray& ray
) {
    HitRecord linear_record;
    HitRecord bvh_record;
    const Interval ray_t(0.001, 1000.0);

    const bool linear_hit = linear_world.hit(ray, ray_t, linear_record);
    const bool bvh_hit = bvh.hit(ray, ray_t, bvh_record);
    assert(linear_hit == bvh_hit);

    if (linear_hit) {
        assert(std::abs(linear_record.t - bvh_record.t) < 1e-9);
        assert((linear_record.point - bvh_record.point).length() < 1e-8);
    }
}
} // namespace

int main() {
    bool rejected_empty = false;
    try {
        BVHNode invalid(std::vector<std::shared_ptr<const Hittable>>{});
    } catch (const std::invalid_argument&) {
        rejected_empty = true;
    }
    assert(rejected_empty);

    bool rejected_null = false;
    try {
        BVHNode invalid({std::shared_ptr<const Hittable>{}});
    } catch (const std::invalid_argument&) {
        rejected_null = true;
    }
    assert(rejected_null);

    std::vector<std::shared_ptr<const Hittable>> objects;
    for (int x = -4; x <= 4; ++x) {
        const double radius = (x % 2 == 0) ? 0.45 : 0.35;
        objects.push_back(std::make_shared<Sphere>(
            Vec3(static_cast<double>(x), 0.0, -6.0), radius
        ));
    }
    // Add a larger, deeper sphere to exercise overlapping bounding boxes.
    objects.push_back(std::make_shared<Sphere>(
        Vec3(0.0, 0.0, -9.0), 1.25
    ));

    HittableList linear_world;
    for (const auto& object : objects) {
        linear_world.add(object);
    }
    BVHNode bvh(objects);

    const std::vector<Ray> rays = {
        Ray(Vec3(0.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0)),
        Ray(Vec3(-4.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0)),
        Ray(Vec3(4.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0)),
        Ray(Vec3(2.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0)),
        Ray(Vec3(20.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0)),
        // Direction is parallel to two AABB axes; this also exercises slab handling.
        Ray(Vec3(0.0, 10.0, 0.0), Vec3(0.0, 0.0, -1.0)),
        Ray(Vec3(0.0, 0.0, 0.0), Vec3(1.0, 0.0, 0.0))
    };

    for (const Ray& ray : rays) {
        compare_hit_results(linear_world, bvh, ray);
    }

    std::cout << "SAH BVH tests passed.\n";
    return 0;
}
