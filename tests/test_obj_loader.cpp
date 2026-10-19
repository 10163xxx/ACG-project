#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "geometry/obj_loader.h"

namespace {
    bool approximately_equal(double a, double b) {
        return std::abs(a - b) < 1e-8;
    }

    void test_quad_triangulation_and_slash_indices() {
        std::istringstream input(
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 1 1 0\n"
            "v 0 1 0\n"
            "vt 0 0\n"
            "vt 1 0\n"
            "vt 1 1\n"
            "vt 0 1\n"
            "vn 0 0 1\n"
            "f 1/1/1 2/2/1 3/3/1 4/4/1\n"
        );

        const TriangleMesh mesh = ObjLoader::load(input);

        assert(mesh.triangle_count() == 2);
        assert(approximately_equal(mesh.area(), 1.0));

        HitRecord record;

        const Ray ray(
            Vec3(0.25, 0.75, 1.0),
            Vec3(0.0, 0.0, -1.0)
        );

        assert(mesh.hit(ray, Interval(0.001, 10.0), record));
        assert(approximately_equal(record.t, 1.0));
    }

    void test_negative_indices() {
        std::istringstream input(
            "v 0 0 0\n"
            "v 2 0 0\n"
            "v 0 1 0\n"
            "f -3 -2 -1\n"
        );

        const TriangleMesh mesh = ObjLoader::load(input);

        assert(mesh.triangle_count() == 1);
        assert(approximately_equal(mesh.area(), 1.0));
    }

    void test_vertex_only_face_indices() {
        std::istringstream input(
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "f 1 2 3\n"
        );

        const TriangleMesh mesh = ObjLoader::load(input);

        assert(mesh.triangle_count() == 1);
        assert(approximately_equal(mesh.area(), 0.5));
    }

    void test_out_of_range_index() {
        std::istringstream input(
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "f 1 2 4\n"
        );

        bool exception_thrown = false;

        try {
            const TriangleMesh mesh = ObjLoader::load(input);
        } catch (const std::out_of_range&) {
            exception_thrown = true;
        }

        assert(exception_thrown);
    }

    void test_missing_faces() {
        std::istringstream input(
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
        );

        bool exception_thrown = false;

        try {
            const TriangleMesh mesh = ObjLoader::load(input);
        } catch (const std::invalid_argument&) {
            exception_thrown = true;
        }

        assert(exception_thrown);
    }
}

int main() {
    test_quad_triangulation_and_slash_indices();
    test_negative_indices();
    test_vertex_only_face_indices();
    test_out_of_range_index();
    test_missing_faces();

    std::cout << "OBJ loader tests passed.\n";
    return 0;
}
