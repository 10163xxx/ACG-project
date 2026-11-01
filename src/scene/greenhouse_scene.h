#pragma once

#include <memory>
#include <array>
#include <cmath>
#include <vector>

#include "geometry/mesh.h"
#include "geometry/hittable_list.h"
#include "geometry/quad.h"
#include "geometry/sphere.h"
#include "geometry/obj_loader.h"
#include "materials/dielectric.h"
#include "materials/diffuse_light.h"
#include "materials/lambertian.h"
#include "materials/principled.h"

inline void greenhouse_box(
    HittableList& world,
    const Vec3& lo,
    const Vec3& hi,
    const std::shared_ptr<const Material>& material
) {
    const double x = hi.x - lo.x;
    const double y = hi.y - lo.y;
    const double z = hi.z - lo.z;

    world.add(std::make_shared<Quad>(
        Vec3(lo.x, lo.y, hi.z), Vec3(x, 0, 0), Vec3(0, y, 0), material));
    world.add(std::make_shared<Quad>(
        Vec3(hi.x, lo.y, lo.z), Vec3(-x, 0, 0), Vec3(0, y, 0), material));
    world.add(std::make_shared<Quad>(
        Vec3(lo.x, lo.y, lo.z), Vec3(0, 0, z), Vec3(0, y, 0), material));
    world.add(std::make_shared<Quad>(
        Vec3(hi.x, lo.y, hi.z), Vec3(0, 0, -z), Vec3(0, y, 0), material));
    world.add(std::make_shared<Quad>(
        Vec3(lo.x, hi.y, hi.z), Vec3(x, 0, 0), Vec3(0, 0, -z), material));
    world.add(std::make_shared<Quad>(
        Vec3(lo.x, lo.y, lo.z), Vec3(x, 0, 0), Vec3(0, 0, z), material));
}

inline void greenhouse_add_leaf(
    HittableList& world,
    const Vec3& base,
    const Vec3& direction,
    double width,
    double curvature,
    const std::shared_ptr<const Material>& material
) {
    constexpr int SEGMENTS = 6;

    const double horizontal_length = std::sqrt(
        direction.x * direction.x +
        direction.z * direction.z
    );

    Vec3 side;

    if (horizontal_length > 1e-8) {
        side = Vec3(
            -direction.z / horizontal_length,
            0.0,
            direction.x / horizontal_length
        );
    } else {
        side = Vec3(1.0, 0.0, 0.0);
    }

    std::vector<Vec3> vertices;
    std::vector<TriangleMesh::Face> faces;

    vertices.reserve((SEGMENTS + 1) * 3);
    faces.reserve(SEGMENTS * 4);

    for (int i = 0; i <= SEGMENTS; ++i) {
        const double t =
            static_cast<double>(i) / SEGMENTS;

        const double profile =
            0.04 + 0.96 * std::pow(
                std::sin(3.14159265358979323846 * t),
                0.85
            );

        const double local_width =
            width * profile;

        const Vec3 center =
            base
            + t * direction
            + Vec3(
                0.0,
                curvature * std::sin(3.14159265358979323846 * t),
                0.0
            );

        vertices.push_back(
            center - local_width * side
        );

        vertices.push_back(
            center + Vec3(
                0.0,
                0.045 * profile,
                0.0
            )
        );

        vertices.push_back(
            center + local_width * side
        );
    }

    for (int i = 0; i < SEGMENTS; ++i) {
        const std::size_t a =
            static_cast<std::size_t>(i * 3);

        const std::size_t b = a + 3;

        faces.push_back({a, b, a + 1});
        faces.push_back({a + 1, b, b + 1});
        faces.push_back({a + 1, b + 1, a + 2});
        faces.push_back({a + 2, b + 1, b + 2});
    }

    world.add(std::make_shared<TriangleMesh>(
        vertices,
        faces,
        material
    ));
}

inline void greenhouse_add_plant(
    HittableList& world,
    double x,
    double z,
    double radius,
    double height,
    int leaf_count,
    const std::shared_ptr<const Material>& leaf_a,
    const std::shared_ptr<const Material>& leaf_b,
    const std::shared_ptr<const Material>& pot_material
) {
    const double pot_width = radius * 1.35;
    const double pot_height = 0.42;

    greenhouse_box(
        world,
        Vec3(x - pot_width, 0.0, z - pot_width),
        Vec3(x + pot_width, pot_height, z + pot_width),
        pot_material
    );

    auto soil = std::make_shared<Lambertian>(
        Vec3(0.095, 0.065, 0.035)
    );

    world.add(std::make_shared<Quad>(
        Vec3(x - pot_width * 0.92, pot_height + 0.005,
            z - pot_width * 0.92),
        Vec3(pot_width * 1.84, 0.0, 0.0),
        Vec3(0.0, 0.0, pot_width * 1.84),
        soil
    ));

    const Vec3 base(
        x,
        pot_height + 0.035,
        z
    );

    for (int i = 0; i < leaf_count; ++i) {
        const double angle =
            6.28318530717958647692
            * static_cast<double>(i)
            / leaf_count;

        const double horizontal =
            radius * (0.65 + 0.18 * (i % 3));

        const double leaf_height =
            height * (0.68 + 0.08 * (i % 4));

        const Vec3 direction(
            horizontal * std::cos(angle),
            leaf_height,
            horizontal * std::sin(angle)
        );

        greenhouse_add_leaf(
            world,
            base,
            direction,
            radius * (0.34 + 0.035 * (i % 3)),
            height * 0.10,
            (i % 3 == 0) ? leaf_b : leaf_a
        );
    }

    // A few upright leaves give the plant a fuller silhouette.
    for (int i = 0; i < 2; ++i) {
        const double angle =
            3.14159265358979323846 * i + 0.6;

        const Vec3 direction(
            radius * 0.22 * std::cos(angle),
            height * (0.92 + 0.08 * i),
            radius * 0.22 * std::sin(angle)
        );

        greenhouse_add_leaf(
            world,
            base,
            direction,
            radius * 0.22,
            height * 0.06,
            leaf_b
        );
    }
}

inline HittableList make_greenhouse_scene() {
    HittableList world;

    auto floor = std::make_shared<PrincipledBSDF>(
        Vec3(0.34, 0.35, 0.32), 0.05, 0.32);
    auto stone = std::make_shared<PrincipledBSDF>(
        Vec3(0.38, 0.34, 0.28), 0.08, 0.48);
    auto iron = std::make_shared<PrincipledBSDF>(
        Vec3(0.11, 0.12, 0.13), 0.82, 0.28);
    auto copper = std::make_shared<PrincipledBSDF>(
        Vec3(0.58, 0.27, 0.12), 0.78, 0.24);

    // Neutral, lightly reflective architectural glazing.
    auto glass_pane = std::make_shared<PrincipledBSDF>(
        Vec3(0.72, 0.75, 0.77), 0.02, 0.12);

    world.add(std::make_shared<Quad>(
        Vec3(-4, 0, 4), Vec3(8, 0, 0), Vec3(0, 0, -12), floor));

    greenhouse_box(world, Vec3(-4, 0, -8), Vec3(-3.72, .62, 4), stone);
    greenhouse_box(world, Vec3(3.72, 0, -8), Vec3(4, .62, 4), stone);

    // Side columns and transverse ties.
    for (int z = 3; z >= -6; z -= 3) {
        greenhouse_box(world, Vec3(-3.88, .62, z - .11),
            Vec3(-3.68, 4.05, z + .11), iron);
        greenhouse_box(world, Vec3(3.68, .62, z - .11),
            Vec3(3.88, 4.05, z + .11), iron);
        greenhouse_box(world, Vec3(-3.82, 3.82, z - .075),
            Vec3(3.82, 4.02, z + .075), iron);
    }

    // Side glazing.
    for (int z = 2; z >= -6; z -= 2) {
        world.add(std::make_shared<Quad>(
            Vec3(-3.79, .85, z), Vec3(0, 0, -1.82),
            Vec3(0, 2.78, 0), glass_pane));
        world.add(std::make_shared<Quad>(
            Vec3(3.79, .85, z - 1.82), Vec3(0, 0, 1.82),
            Vec3(0, 2.78, 0), glass_pane));
    }

    // Two pitched roof slopes, divided into five longitudinal bays.
    for (int i = 0; i < 5; ++i) {
        const double z = 3.8 - 2.4 * i;
        world.add(std::make_shared<Quad>(
            Vec3(-3.96, 4.05, z), Vec3(3.94, 1.94, 0),
            Vec3(0, 0, -2.25), glass_pane));
        world.add(std::make_shared<Quad>(
            Vec3(.02, 5.99, z), Vec3(3.94, -1.94, 0),
            Vec3(0, 0, -2.25), glass_pane));
    }

    greenhouse_box(world, Vec3(-.10, 5.98, -8),
        Vec3(.10, 6.20, 4), copper);
    greenhouse_box(world, Vec3(-4, 3.91, -8),
        Vec3(-3.82, 4.12, 4), iron);
    greenhouse_box(world, Vec3(3.82, 3.91, -8),
        Vec3(4, 4.12, 4), iron);

    // Roof cross rafters.
    for (int i = 0; i <= 5; ++i) {
        const double z = 3.85 - 2.4 * i;
        world.add(std::make_shared<Quad>(
            Vec3(-4, 4.04, z), Vec3(4, 1.98, 0),
            Vec3(0, 0, -.075), iron));
        world.add(std::make_shared<Quad>(
            Vec3(0, 6.02, z), Vec3(4, -1.98, 0),
            Vec3(0, 0, -.075), iron));
    }

    // Rear glazed wall and mullions.
    world.add(std::make_shared<Quad>(
        Vec3(-3.68, .72, -7.88), Vec3(7.36, 0, 0),
        Vec3(0, 3.18, 0), glass_pane));
    for (double x : {-2.4, -1.2, 0.0, 1.2, 2.4}) {
        greenhouse_box(world, Vec3(x - .035, .72, -7.98),
            Vec3(x + .035, 3.95, -7.82), iron);
    }

    // Imported botanical assets.
    auto anthurium_fallback =
        std::make_shared<PrincipledBSDF>(
            Vec3(0.12, 0.30, 0.075),
            0.0,
            0.48
        );

    auto fern_fallback =
        std::make_shared<PrincipledBSDF>(
            Vec3(0.075, 0.24, 0.055),
            0.0,
            0.52
        );

    // Anthurium: the main botanical subject on the exhibition plinth.
    TriangleMesh anthurium = ObjLoader::load_file(
        "assets/anthurium_botany_01_2k.blend/"
        "anthurium_botany_01_2k.obj",
        anthurium_fallback,
        2.35,
        Vec3(0.0, 1.10, -2.20)
    );

    world.add(
        std::make_shared<TriangleMesh>(
            std::move(anthurium)
        )
    );

    // Fern planters provide foreground and background depth.
    auto fern_planter = std::make_shared<PrincipledBSDF>(
        Vec3(0.16, 0.12, 0.085),
        0.0,
        0.72
    );

    auto fern_soil = std::make_shared<Lambertian>(
        Vec3(0.075, 0.052, 0.028)
    );

    auto add_fern = [&](double x, double z, double height) {
        const double half_width = 0.38;
        const double pot_height = 0.42;

        greenhouse_box(
            world,
            Vec3(
                x - half_width,
                0.0,
                z - half_width
            ),
            Vec3(
                x + half_width,
                pot_height,
                z + half_width
            ),
            fern_planter
        );

        world.add(std::make_shared<Quad>(
            Vec3(
                x - half_width * 0.88,
                pot_height + 0.005,
                z - half_width * 0.88
            ),
            Vec3(half_width * 1.76, 0.0, 0.0),
            Vec3(0.0, 0.0, half_width * 1.76),
            fern_soil
        ));

        TriangleMesh fern = ObjLoader::load_file(
            "assets/fern_02_2k.blend/fern_02_2k.obj",
            fern_fallback,
            height,
            Vec3(x, pot_height + 0.01, z)
        );

        world.add(
            std::make_shared<TriangleMesh>(
                std::move(fern)
            )
        );
    };

    add_fern(-2.45, -1.35, 1.65);
    add_fern(2.45, -4.35, 1.85);

    // Central exhibition plinth.
    greenhouse_box(world, Vec3(-.95, 0, -3.15),
        Vec3(.95, .82, -1.25), stone);
    greenhouse_box(world, Vec3(-1.02, .82, -3.22),
        Vec3(1.02, .96, -1.18), copper);
    greenhouse_box(world, Vec3(-.72, .96, -2.92),
        Vec3(.72, 1.10, -1.48), iron);

    // Downward-facing rectangular emitter (normal points toward -Y).
    auto emitter = std::make_shared<DiffuseLight>(Vec3(4, 3, 2));
    world.add(std::make_shared<Quad>(
        Vec3(-1.35, 5.45, -4), Vec3(2.7, 0, 0),
        Vec3(0, 0, .12), emitter));

    return world;
}
