#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>
#include <stdexcept>

#include "acceleration/bvh.h"
#include "geometry/hittable_list.h"
#include "geometry/obj_loader.h"
#include "geometry/sphere.h"
#include "materials/diffuse_light.h"
#include "materials/lambertian.h"
#include "materials/mtl_loader.h"

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

    void test_obj_mtl_diffuse_texture() {
        namespace fs = std::filesystem;

        const fs::path test_dir =
            fs::current_path() / "obj_mtl_texture_test";

        fs::create_directories(test_dir);

        const fs::path texture_path =
            test_dir / "green.ppm";

        const fs::path mtl_path =
            test_dir / "test.mtl";

        const fs::path obj_path =
            test_dir / "test.obj";

        // Create a 1x1 green image texture.
        {
            std::ofstream file(texture_path);

            assert(file.good());

            file << "P3\n";
            file << "1 1\n";
            file << "255\n";
            file << "0 255 0\n";
        }

        // The material uses a white diffuse color and a green map_Kd.
        {
            std::ofstream file(mtl_path);

            assert(file.good());

            file << "newmtl textured_material\n";
            file << "Kd 1.0 1.0 1.0\n";
            file << "map_Kd green.ppm\n";
        }

        // The OBJ references the MTL and assigns its material to a triangle.
        {
            std::ofstream file(obj_path);

            assert(file.good());

            file << "mtllib test.mtl\n";

            file << "v -1 -1 0\n";
            file << "v  1 -1 0\n";
            file << "v  0  1 0\n";

            file << "vt 0 0\n";
            file << "vt 1 0\n";
            file << "vt 0.5 1\n";

            file << "usemtl textured_material\n";
            file << "f 1/1 2/2 3/3\n";
        }

        {
            // This must load the MTL and resolve green.ppm
            // relative to the MTL file's directory.
            TriangleMesh mesh = ObjLoader::load_file(
                obj_path.string()
            );

            assert(mesh.triangle_count() == 1);

            const Ray ray(
                Vec3(0.0, 0.0, 2.0),
                Vec3(0.0, 0.0, -1.0)
            );

            HitRecord record;

            const bool hit = mesh.hit(
                ray,
                Interval(0.001, 10.0),
                record
            );

            assert(hit);

            if (!hit) {
                fs::remove_all(test_dir);
                throw std::runtime_error(
                    "OBJ/MTL texture integration test failed to hit the mesh."
                );
            }

            if (record.material == nullptr) {
                fs::remove_all(test_dir);
                throw std::runtime_error(
                    "OBJ/MTL texture integration test hit the mesh, "
                    "but the material is null."
                );
            }

            // A white base diffuse color multiplied by the
            // green texture should produce a green-only BSDF.
            const Vec3 bsdf = record.material->evaluate_bsdf(
                record,
                Vec3(0.0, 0.0, 1.0),
                Vec3(0.0, 0.0, 1.0)
            );

            assert(bsdf.y > 0.0);
            assert(std::abs(bsdf.x) < 1e-8);
            assert(std::abs(bsdf.z) < 1e-8);
        }

        fs::remove_all(test_dir);
    }

    void test_mtl_metallic_and_roughness() {
        namespace fs = std::filesystem;

        const fs::path test_dir =
            fs::current_path() / "obj_mtl_parameter_map_test";

        fs::create_directories(test_dir);

        auto write_ppm = [](
            const fs::path& path,
            int value
        ) {
            std::ofstream file(path);
            if (!file) {
                throw std::runtime_error(
                    "Failed to create parameter-map test texture."
                );
            }

            file << "P3\n";
            file << "1 1\n";
            file << "255\n";
            file << value << " " << value << " " << value << "\n";
        };

        write_ppm(test_dir / "black.ppm", 0);
        write_ppm(test_dir / "white.ppm", 255);
        write_ppm(test_dir / "rough_low.ppm", 64);
        write_ppm(test_dir / "rough_high.ppm", 220);

        std::istringstream input(
            "newmtl dielectric\n"
            "Kd 0.8 0.4 0.2\n"
            "Ks 0.04 0.04 0.04\n"
            "Pr 0.5\n"
            "Pm 0.0\n"

            "newmtl metallic\n"
            "Kd 0.8 0.4 0.2\n"
            "Ks 0.04 0.04 0.04\n"
            "Pr 0.5\n"
            "Pm 1.0\n"

            "newmtl roughness_override\n"
            "Kd 0.8 0.4 0.2\n"
            "Ks 0.04 0.04 0.04\n"
            "Ns 1000.0\n"
            "Pr 1.0\n"

            "newmtl metallic_map_black\n"
            "Kd 0.8 0.4 0.2\n"
            "Ks 0.04 0.04 0.04\n"
            "Pr 0.5\n"
            "map_Pm black.ppm\n"

            "newmtl metallic_map_white\n"
            "Kd 0.8 0.4 0.2\n"
            "Ks 0.04 0.04 0.04\n"
            "Pr 0.5\n"
            "map_Pm white.ppm\n"

            "newmtl roughness_map_low\n"
            "Kd 0.8 0.4 0.2\n"
            "Ks 0.04 0.04 0.04\n"
            "map_Pr rough_low.ppm\n"

            "newmtl roughness_map_high\n"
            "Kd 0.8 0.4 0.2\n"
            "Ks 0.04 0.04 0.04\n"
            "map_Pr rough_high.ppm\n"
        );

        try {
            const auto materials =
                MtlLoader::load(input, test_dir);

            assert(materials.count("dielectric") == 1);
            assert(materials.count("metallic") == 1);
            assert(materials.count("roughness_override") == 1);
            assert(materials.count("metallic_map_black") == 1);
            assert(materials.count("metallic_map_white") == 1);
            assert(materials.count("roughness_map_low") == 1);
            assert(materials.count("roughness_map_high") == 1);

            HitRecord hit;
            hit.normal = Vec3(0.0, 0.0, 1.0);
            hit.u = 0.5;
            hit.v = 0.5;

            const Vec3 outgoing =
                normalize(Vec3(0.3, 0.0, 1.0));

            const Vec3 incoming =
                normalize(Vec3(-0.2, 0.1, 1.0));

            auto evaluate = [&](
                const std::string& name
            ) {
                return materials.at(name)->evaluate_bsdf(
                    hit,
                    outgoing,
                    incoming
                );
            };

            auto difference = [](
                const Vec3& a,
                const Vec3& b
            ) {
                return
                    std::abs(a.x - b.x)
                    + std::abs(a.y - b.y)
                    + std::abs(a.z - b.z);
            };

            const Vec3 dielectric_bsdf =
                evaluate("dielectric");

            const Vec3 metallic_bsdf =
                evaluate("metallic");

            // Constant Pm must affect the BSDF.
            assert(
                difference(
                    dielectric_bsdf,
                    metallic_bsdf
                ) > 1e-6
            );

            const Vec3 override_bsdf =
                evaluate("roughness_override");

            // Explicit Pr=1.0 must override Ns.
            assert(
                difference(
                    metallic_bsdf,
                    override_bsdf
                ) > 1e-6
            );

            // A black metallic map should behave like Pm=0.
            const Vec3 black_map_bsdf =
                evaluate("metallic_map_black");

            assert(
                difference(
                    dielectric_bsdf,
                    black_map_bsdf
                ) < 1e-6
            );

            // A white metallic map should behave like Pm=1.
            const Vec3 white_map_bsdf =
                evaluate("metallic_map_white");

            assert(
                difference(
                    metallic_bsdf,
                    white_map_bsdf
                ) < 1e-6
            );

            // Different roughness maps must produce different BSDFs.
            const Vec3 low_roughness_bsdf =
                evaluate("roughness_map_low");

            const Vec3 high_roughness_bsdf =
                evaluate("roughness_map_high");

            assert(
                difference(
                    low_roughness_bsdf,
                    high_roughness_bsdf
                ) > 1e-6
            );
        } catch (...) {
            fs::remove_all(test_dir);
            throw;
        }

        fs::remove_all(test_dir);
    }
}

int main() {
    test_imported_mesh_in_bvh();
    test_imported_mesh_as_area_light();
    test_imported_mesh_in_scene_with_other_objects();
    test_obj_mtl_diffuse_texture();
    test_mtl_metallic_and_roughness();

    std::cout << "OBJ integration tests passed.\n";
    return 0;
}
