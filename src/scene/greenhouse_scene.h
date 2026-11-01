#pragma once

#include <memory>

#include "geometry/hittable_list.h"
#include "geometry/quad.h"
#include "geometry/sphere.h"
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

inline HittableList make_greenhouse_scene() {
    HittableList world;

    auto floor = std::make_shared<PrincipledBSDF>(
        Vec3(0.34, 0.35, 0.32), 0.05, 0.32);
    auto stone = std::make_shared<PrincipledBSDF>(
        Vec3(0.38, 0.34, 0.28), 0.08, 0.48);
    auto iron = std::make_shared<PrincipledBSDF>(
        Vec3(0.075, 0.095, 0.085), 0.82, 0.26);
    auto copper = std::make_shared<PrincipledBSDF>(
        Vec3(0.58, 0.27, 0.12), 0.78, 0.24);
    // Architectural panes are reflective surfaces, not open dielectric volumes.
    auto glass_pane = std::make_shared<PrincipledBSDF>(
        Vec3(0.20, 0.30, 0.31), 0.32, 0.16);

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

    // Central exhibition plinth.
    greenhouse_box(world, Vec3(-.95, 0, -3.15),
        Vec3(.95, .82, -1.25), stone);
    greenhouse_box(world, Vec3(-1.02, .82, -3.22),
        Vec3(1.02, .96, -1.18), copper);
    greenhouse_box(world, Vec3(-.72, .96, -2.92),
        Vec3(.72, 1.10, -1.48), iron);

    auto dielectric = std::make_shared<Dielectric>(
        1.48, Vec3(.96, .99, 1), Vec3(.015, .006, .003));
    auto gold = std::make_shared<PrincipledBSDF>(
        Vec3(.82, .48, .16), .88, .19);

    world.add(std::make_shared<Sphere>(
        Vec3(0, 1.78, -2.20), .58, dielectric));
    world.add(std::make_shared<Sphere>(
        Vec3(0, 1.78, -2.20), .15, gold));

    // Downward-facing rectangular emitter (normal points toward -Y).
    auto emitter = std::make_shared<DiffuseLight>(Vec3(4, 3, 2));
    world.add(std::make_shared<Quad>(
        Vec3(-1.35, 5.45, -4), Vec3(2.7, 0, 0),
        Vec3(0, 0, .12), emitter));

    return world;
}
