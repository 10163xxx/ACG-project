#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

#include "geometry/aabb.h"
#include "geometry/sphere.h"
#include "geometry/hittable_list.h"

namespace {
    bool approximately_equal(double a, double b) {
        constexpr double epsilon = 1e-9;
        return std::abs(a - b) < epsilon;
    }

    void test_sphere_bounding_box() {
        Sphere sphere(Vec3(1.0, 2.0, 3.0), 2.0);
        const AABB box = sphere.bounding_box();

        assert(approximately_equal(box.minimum.x, -1.0));
        assert(approximately_equal(box.minimum.y,  0.0));
        assert(approximately_equal(box.minimum.z,  1.0));

        assert(approximately_equal(box.maximum.x, 3.0));
        assert(approximately_equal(box.maximum.y, 4.0));
        assert(approximately_equal(box.maximum.z, 5.0));
    }

    void test_aabb_hit() {
        const AABB box(
            Vec3(-1.0, -1.0, -1.0),
            Vec3( 1.0,  1.0,  1.0)
        );

        const Ray hit_ray(
            Vec3(0.0, 0.0, 3.0),
            Vec3(0.0, 0.0, -1.0)
        );

        const Ray miss_ray(
            Vec3(2.0, 0.0, 3.0),
            Vec3(0.0, 0.0, -1.0)
        );

        assert(box.hit(hit_ray, Interval(0.0, 100.0)));
        assert(!box.hit(miss_ray, Interval(0.0, 100.0)));
    }

    void test_hittable_list_bounding_box() {
        HittableList world;

        world.add(std::make_shared<Sphere>(
            Vec3(-2.0, 0.0, 0.0), 1.0
        ));

        world.add(std::make_shared<Sphere>(
            Vec3(2.0, 0.0, 0.0), 1.0
        ));

        const AABB box = world.bounding_box();

        assert(approximately_equal(box.minimum.x, -3.0));
        assert(approximately_equal(box.maximum.x,  3.0));
        assert(approximately_equal(box.minimum.y, -1.0));
        assert(approximately_equal(box.maximum.y,  1.0));
        assert(approximately_equal(box.minimum.z, -1.0));
        assert(approximately_equal(box.maximum.z,  1.0));
    }
}

int main() {
    test_sphere_bounding_box();
    test_aabb_hit();
    test_hittable_list_bounding_box();

    std::cout << "Geometry tests passed.\n";
    return 0;
}
