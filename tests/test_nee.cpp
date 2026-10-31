#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

#include "core/random.h"
#include "core/ray.h"
#include "geometry/hittable_list.h"
#include "geometry/sphere.h"
#include "geometry/mesh.h"
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
            Vec3(0.0, 1.5, 2.5),
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
    
    // 4. A blocker between the diffuse surface and the light
    // should eliminate the direct-light contribution.
    HittableList blocked_scene;

    blocked_scene.add(
        std::make_shared<Sphere>(
            Vec3(0.0, 0.0, 0.0),
            1.0,
            diffuse_material
        )
    );

    // The blocker is placed along the line from the visible
    // point on the diffuse sphere to the center of the light.
    // Its projected size is larger than the light's.
    blocked_scene.add(
        std::make_shared<Sphere>(
            Vec3(1.0, 1.0, 1.25),
            0.6,
            diffuse_material
        )
    );

    blocked_scene.add(
        std::make_shared<Sphere>(
            Vec3(2.0, 2.0, 1.5),
            0.5,
            light_material
        )
    );

    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        Random::seed(seed);

        const Vec3 blocked_result = tracer.trace(
            camera_ray,
            blocked_scene,
            blocked_scene
        );

        assert(std::abs(blocked_result.x) < 1e-12);
        assert(std::abs(blocked_result.y) < 1e-12);
        assert(std::abs(blocked_result.z) < 1e-12);
    }

    // 5. A triangle mesh should work as an area light for NEE.
    //
    // The mesh consists of two triangles forming a square.
    // Its winding order gives it a downward-facing normal,
    // so it emits toward the diffuse sphere below it.
    HittableList mesh_light_scene;

    mesh_light_scene.add(
        std::make_shared<Sphere>(
            Vec3(0.0, 0.0, 0.0),
            1.0,
            diffuse_material
        )
    );

    const std::vector<Vec3> light_vertices = {
        Vec3(1.0, -1.0, 2.0),
        Vec3(1.0,  1.0, 2.0),
        Vec3(3.0,  1.0, 2.0),
        Vec3(3.0, -1.0, 2.0)
    };

    const std::vector<TriangleMesh::Face> light_faces = {
        {0, 1, 2},
        {0, 2, 3}
    };

    const auto mesh_light =
        std::make_shared<TriangleMesh>(
            light_vertices,
            light_faces,
            light_material
        );

    mesh_light_scene.add(mesh_light);

    // The mesh has total area 4, so its uniform area PDF
    // should be 1 / 4.
    assert(std::abs(mesh_light->area() - 4.0) < 1e-8);

    HitRecord mesh_light_sample;
    double mesh_light_pdf_area = 0.0;

    assert(mesh_light->sample_surface(
        0.25,
        0.75,
        mesh_light_sample,
        mesh_light_pdf_area
    ));

    assert(std::abs(mesh_light_pdf_area - 0.25) < 1e-8);
    assert(mesh_light_sample.material->is_emissive());
    assert(mesh_light_sample.normal.z < 0.0);

    // Use deterministic seeds to verify that NEE can obtain
    // a positive direct-light contribution from the mesh.
    bool found_mesh_light_contribution = false;

    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        Random::seed(seed);

        const Vec3 result = tracer.trace(
            camera_ray,
            mesh_light_scene,
            mesh_light_scene
        );

        if (result.x > 1e-9 ||
            result.y > 1e-9 ||
            result.z > 1e-9) {
            found_mesh_light_contribution = true;
            break;
        }
    }

    assert(found_mesh_light_contribution);

    // 6. A mesh area light fully blocked by a mesh occluder
    // should produce no direct-light contribution.
    HittableList blocked_mesh_scene;

    blocked_mesh_scene.add(
        std::make_shared<Sphere>(
            Vec3(0.0, 0.0, 0.0),
            1.0,
            diffuse_material
        )
    );

    // The occluder lies between the visible sphere surface
    // and the mesh light, but does not intersect the camera ray.
    const std::vector<Vec3> blocker_vertices = {
        Vec3(0.45, -2.0, 1.5),
        Vec3(3.50, -2.0, 1.5),
        Vec3(3.50,  2.0, 1.5),
        Vec3(0.45,  2.0, 1.5)
    };

    const std::vector<TriangleMesh::Face> blocker_faces = {
        {0, 1, 2},
        {0, 2, 3}
    };

    blocked_mesh_scene.add(
        std::make_shared<TriangleMesh>(
            blocker_vertices,
            blocker_faces,
            diffuse_material
        )
    );

    // The light faces downward, toward the receiving sphere.
    const std::vector<Vec3> mesh_light_vertices = {
        Vec3(1.0, -1.0, 2.0),
        Vec3(1.0,  1.0, 2.0),
        Vec3(3.0,  1.0, 2.0),
        Vec3(3.0, -1.0, 2.0)
    };

    const std::vector<TriangleMesh::Face> mesh_light_faces = {
        {0, 1, 2},
        {0, 2, 3}
    };

    blocked_mesh_scene.add(
        std::make_shared<TriangleMesh>(
            mesh_light_vertices,
            mesh_light_faces,
            light_material
        )
    );

    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        Random::seed(seed);

        const Vec3 result = tracer.trace(
            camera_ray,
            blocked_mesh_scene,
            blocked_mesh_scene
        );

        assert(std::abs(result.x) < 1e-12);
        assert(std::abs(result.y) < 1e-12);
        assert(std::abs(result.z) < 1e-12);
    }

        // Mesh area light facing away from the receiving surface
    // must not contribute direct illumination.
    HittableList back_facing_mesh_scene;

    back_facing_mesh_scene.add(
        std::make_shared<Sphere>(
            Vec3(0.0, 0.0, 0.0),
            1.0,
            diffuse_material
        )
    );

    const std::vector<Vec3> back_facing_light_vertices = {
        Vec3(1.0, -1.0, 2.0),
        Vec3(1.0,  1.0, 2.0),
        Vec3(3.0,  1.0, 2.0),
        Vec3(3.0, -1.0, 2.0)
    };

    // Reverse the winding order so the geometric normal
    // points toward +Z, away from the sphere below.
    const std::vector<TriangleMesh::Face> back_facing_light_faces = {
        {0, 2, 1},
        {0, 3, 2}
    };

    back_facing_mesh_scene.add(
        std::make_shared<TriangleMesh>(
            back_facing_light_vertices,
            back_facing_light_faces,
            light_material
        )
    );

    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        Random::seed(seed);

        const Vec3 result = tracer.trace(
            camera_ray,
            back_facing_mesh_scene,
            back_facing_mesh_scene
        );

        assert(std::abs(result.x) < 1e-12);
        assert(std::abs(result.y) < 1e-12);
        assert(std::abs(result.z) < 1e-12);
    }

    std::cout << "All NEE tests passed.\n";
    return 0;
}
