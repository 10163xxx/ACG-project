#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "acceleration/bvh.h"
#include "geometry/hittable_list.h"
#include "geometry/sphere.h"

int main() {
    std::vector<std::shared_ptr<const Hittable>> objects;

    objects.push_back(
        std::make_shared<Sphere>(Vec3(-2.0, 0.0, -5.0), 1.0)
    );
    objects.push_back(
        std::make_shared<Sphere>(Vec3(2.0, 0.0, -5.0), 1.0)
    );
    objects.push_back(
        std::make_shared<Sphere>(Vec3(0.0, 0.0, -8.0), 1.5)
    );

    HittableList linear_world;
    for (const auto& object : objects) {
        linear_world.add(object);
    }

    BVHNode bvh(objects);

    const std::vector<Ray> rays = {
        Ray(Vec3(0.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0)),
        Ray(Vec3(-2.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0)),
        Ray(Vec3(2.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0)),
        Ray(Vec3(10.0, 0.0, 0.0), Vec3(0.0, 0.0, -1.0))
    };

    for (const Ray& ray : rays) {
        HitRecord linear_record;
        HitRecord bvh_record;

        const Interval ray_t(0.001, 1000.0);

        const bool linear_hit =
            linear_world.hit(ray, ray_t, linear_record);

        const bool bvh_hit =
            bvh.hit(ray, ray_t, bvh_record);

        assert(linear_hit == bvh_hit);

        if (linear_hit) {
            assert(std::abs(
                linear_record.t - bvh_record.t
            ) < 1e-9);
        }
    }

    std::cout << "BVH tests passed.\n";
    return 0;
}
