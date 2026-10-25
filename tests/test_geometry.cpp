#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

#include "geometry/aabb.h"
#include "geometry/sphere.h"
#include "geometry/hittable_list.h"
#include "geometry/triangle.h"

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
    
    void test_sphere_uv_coordinates() {
        Sphere sphere(Vec3(0.0, 0.0, 0.0), 1.0);

        const Ray ray_z(
            Vec3(0.0, 0.0, 3.0),
            Vec3(0.0, 0.0, -1.0)
        );

        HitRecord hit_z;
        assert(sphere.hit(ray_z, Interval(0.001, 100.0), hit_z));

        assert(approximately_equal(hit_z.point.x, 0.0));
        assert(approximately_equal(hit_z.point.y, 0.0));
        assert(approximately_equal(hit_z.point.z, 1.0));
        assert(approximately_equal(hit_z.u, 0.25));
        assert(approximately_equal(hit_z.v, 0.5));

        const Ray ray_x(
            Vec3(3.0, 0.0, 0.0),
            Vec3(-1.0, 0.0, 0.0)
        );

        HitRecord hit_x;
        assert(sphere.hit(ray_x, Interval(0.001, 100.0), hit_x));

        assert(approximately_equal(hit_x.u, 0.5));
        assert(approximately_equal(hit_x.v, 0.5));
    }

    void test_triangle_uv_parameterization_without_differentials() {
        const Triangle triangle(
            Vec3(0.0, 0.0, 0.0),
            Vec3(1.0, 0.0, 0.0),
            Vec3(0.0, 1.0, 0.0)
        );

        // This ray has no screen-space ray differentials.
        const Ray ray(
            Vec3(0.25, 0.25, 1.0),
            Vec3(0.0, 0.0, -1.0)
        );

        HitRecord record;

        assert(triangle.hit(
            ray,
            Interval(0.001, 10.0),
            record
        ));

        assert(!ray.has_differentials);
        assert(record.has_uv_parameterization);

        assert(approximately_equal(
            record.dpdu.x, 1.0
        ));
        assert(approximately_equal(
            record.dpdu.y, 0.0
        ));
        assert(approximately_equal(
            record.dpdu.z, 0.0
        ));

        assert(approximately_equal(
            record.dpdv.x, 0.0
        ));
        assert(approximately_equal(
            record.dpdv.y, 1.0
        ));
        assert(approximately_equal(
            record.dpdv.z, 0.0
        ));

        assert(!record.has_uv_differentials);
    }
}

int main() {
    test_sphere_bounding_box();
    test_aabb_hit();
    test_hittable_list_bounding_box();
    test_sphere_uv_coordinates();
    test_triangle_uv_parameterization_without_differentials();
    std::cout << "Geometry tests passed.\n";
    return 0;
}
