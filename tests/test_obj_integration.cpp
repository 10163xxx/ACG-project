#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "acceleration/bvh.h"
#include "geometry/hittable_list.h"
#include "geometry/obj_loader.h"
#include "geometry/sphere.h"
#include "materials/diffuse_light.h"
#include "materials/lambertian.h"

namespace {
    bool approximately_equal(double a, double b) {
        return std::abs(a - b) < 1e-8;
    }

    void test_imported_mesh_in_bvh() {
        std::istringstream input(
            "v -1 -1 0\n"
            "v -1  1 0\n"
            "v  1  1 0\n"
            "v  1 -1 0\n"
            "f 1 2 3 4\n"
        );

        TriangleMesh mesh = ObjLoader::load(input);

        assert(mesh.triangle_count() == 2);
        assert(approximately_equal(mesh.area(), 4.0));

        auto mesh_ptr = std::make_shared<TriangleMesh>(
            std::move(mesh)
        );

        HittableList scene;
        scene.add(mesh_ptr);

        BVHNode world(scene.objects());

        HitRecord record;

        const Ray ray(
            Vec3(0.0, 0.0, 2.0),
            Vec3(0.0, 0.0, -1.0)
        );

        assert(world.hit(
            ray,
            Interval(0.001, 10.0),
            record
        ));

        assert(approximately_equal(record.t, 2.0));
        assert(approximately_equal(record.point.z, 0.0));
    }

    void test_imported_mesh_as_area_light() {
        auto light_material =
            std::make_shared<DiffuseLight>(
                Vec3(5.0, 4.0, 3.0)
            );

        std::istringstream input(
            "v -1 -1 0\n"
            "v -1  1 0\n"
            "v  1  1 0\n"
            "v  1 -1 0\n"
            "f 1 2 3 4\n"
        );

        TriangleMesh mesh = ObjLoader::load(
            input,
            light_material
        );

        assert(mesh.triangle_count() == 2);
        assert(approximately_equal(mesh.area(), 4.0));

        HitRecord sample;
        double pdf_area = 0.0;

        assert(mesh.sample_surface(
            0.25,
            0.75,
            sample,
            pdf_area
        ));

        assert(approximately_equal(pdf_area, 0.25));
        assert(sample.material != nullptr);
        assert(sample.material->is_emissive());

        // The chosen point must lie inside the imported square.
        assert(sample.point.x >= -1.0 - 1e-8);
        assert(sample.point.x <=  1.0 + 1e-8);
        assert(sample.point.y >= -1.0 - 1e-8);
        assert(sample.point.y <=  1.0 + 1e-8);
        assert(approximately_equal(sample.point.z, 0.0));
    }

    void test_imported_mesh_in_scene_with_other_objects() {
        auto diffuse_material =
            std::make_shared<Lambertian>(
                Vec3(0.7, 0.7, 0.7)
            );

        std::istringstream input(
            "v -1 -1 0\n"
            "v -1  1 0\n"
            "v  1  1 0\n"
            "v  1 -1 0\n"
            "f 1 2 3 4\n"
        );

        auto imported_mesh =
            std::make_shared<TriangleMesh>(
                ObjLoader::load(input, diffuse_material)
            );

        HittableList scene;

        scene.add(imported_mesh);

        scene.add(
            std::make_shared<Sphere>(
                Vec3(0.0, 0.0, -2.0),
                0.5,
                diffuse_material
            )
        );

        BVHNode world(scene.objects());

        HitRecord record;

        const Ray ray(
            Vec3(0.0, 0.0, 2.0),
            Vec3(0.0, 0.0, -1.0)
        );

        assert(world.hit(
            ray,
            Interval(0.001, 10.0),
            record
        ));

        // The ray must hit the imported mesh first.
        assert(approximately_equal(record.t, 2.0));
        assert(approximately_equal(record.point.z, 0.0));
    }
}

int main() {
    test_imported_mesh_in_bvh();
    test_imported_mesh_as_area_light();
    test_imported_mesh_in_scene_with_other_objects();

    std::cout << "OBJ integration tests passed.\n";
    return 0;
}
