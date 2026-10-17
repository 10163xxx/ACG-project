#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "geometry/mesh.h"

namespace {
    bool approximately_equal(double a, double b) {
        constexpr double epsilon = 1e-8;
        return std::abs(a - b) < epsilon;
    }

    TriangleMesh make_test_mesh() {
        const std::vector<Vec3> vertices = {
            Vec3(0.0, 0.0, 0.0),
            Vec3(1.0, 0.0, 0.0),
            Vec3(1.0, 1.0, 0.0),
            Vec3(0.0, 1.0, 0.0)
        };

        const std::vector<TriangleMesh::Face> faces = {
            {0, 1, 2},
            {0, 2, 3}
        };

        return TriangleMesh(vertices, faces);
    }

    void test_mesh_construction_and_area() {
        const TriangleMesh mesh = make_test_mesh();

        assert(mesh.triangle_count() == 2);
        assert(approximately_equal(mesh.area(), 1.0));
    }

    void test_mesh_bounding_box() {
        const TriangleMesh mesh = make_test_mesh();
        const AABB box = mesh.bounding_box();

        constexpr double padding = 1e-4;

        assert(approximately_equal(
            box.minimum.x, -padding
        ));
        assert(approximately_equal(
            box.minimum.y, -padding
        ));
        assert(approximately_equal(
            box.minimum.z, -padding
        ));

        assert(approximately_equal(
            box.maximum.x, 1.0 + padding
        ));
        assert(approximately_equal(
            box.maximum.y, 1.0 + padding
        ));
        assert(approximately_equal(
            box.maximum.z, padding
        ));
    }

    void test_mesh_ray_intersection() {
        const TriangleMesh mesh = make_test_mesh();

        const Ray hit_ray(
            Vec3(0.25, 0.75, 1.0),
            Vec3(0.0, 0.0, -1.0)
        );

        HitRecord record;

        assert(mesh.hit(
            hit_ray,
            Interval(0.001, 10.0),
            record
        ));

        assert(approximately_equal(record.t, 1.0));
        assert(approximately_equal(record.point.x, 0.25));
        assert(approximately_equal(record.point.y, 0.75));
        assert(approximately_equal(record.point.z, 0.0));

        const Ray miss_ray(
            Vec3(2.0, 2.0, 1.0),
            Vec3(0.0, 0.0, -1.0)
        );

        HitRecord miss_record;

        assert(!mesh.hit(
            miss_ray,
            Interval(0.001, 10.0),
            miss_record
        ));
    }

    void test_mesh_surface_sampling() {
        const TriangleMesh mesh = make_test_mesh();

        constexpr int sample_count = 1000;

        for (int i = 0; i < sample_count; ++i) {
            const double u =
                (static_cast<double>(i) + 0.5) /
                static_cast<double>(sample_count);

            const double v = 0.37;

            HitRecord sample;
            double pdf_area = 0.0;

            assert(mesh.sample_surface(
                u,
                v,
                sample,
                pdf_area
            ));

            assert(approximately_equal(pdf_area, 1.0));
            assert(approximately_equal(sample.point.z, 0.0));

            assert(sample.point.x >= -1e-8);
            assert(sample.point.x <= 1.0 + 1e-8);
            assert(sample.point.y >= -1e-8);
            assert(sample.point.y <= 1.0 + 1e-8);

            assert(approximately_equal(
                sample.normal.length(), 1.0
            ));
        }

        HitRecord sample;
        double pdf_area = 0.0;

        assert(!mesh.sample_surface(
            1.0, 0.5, sample, pdf_area
        ));

        assert(approximately_equal(pdf_area, 0.0));
    }

    void test_invalid_face_index() {
        const std::vector<Vec3> vertices = {
            Vec3(0.0, 0.0, 0.0),
            Vec3(1.0, 0.0, 0.0),
            Vec3(0.0, 1.0, 0.0)
        };

        const std::vector<TriangleMesh::Face> faces = {
            {0, 1, 3}
        };

        bool exception_thrown = false;

        try {
            TriangleMesh mesh(vertices, faces);
        } catch (const std::out_of_range&) {
            exception_thrown = true;
        }

        assert(exception_thrown);
    }
}

int main() {
    test_mesh_construction_and_area();
    test_mesh_bounding_box();
    test_mesh_ray_intersection();
    test_mesh_surface_sampling();
    test_invalid_face_index();

    std::cout << "Mesh tests passed.\n";
    return 0;
}