#pragma once

#include <memory>

#include "geometry/hittable_list.h"
#include "geometry/quad.h"
#include "geometry/sphere.h"

#include "materials/lambertian.h"
#include "materials/metal.h"
#include "materials/dielectric.h"
#include "materials/diffuse_light.h"
#include "materials/principled.h"
#include "materials/procedural_texture.h"

inline void add_showcase_box(
    HittableList& world,
    const Vec3& minimum,
    const Vec3& maximum,
    std::shared_ptr<const Material> material
) {
    const double x = maximum.x - minimum.x;
    const double y = maximum.y - minimum.y;
    const double z = maximum.z - minimum.z;

    // Front and back faces.
    world.add(std::make_shared<Quad>(
        Vec3(minimum.x, minimum.y, maximum.z),
        Vec3(x, 0.0, 0.0),
        Vec3(0.0, y, 0.0),
        material
    ));

    world.add(std::make_shared<Quad>(
        Vec3(maximum.x, minimum.y, minimum.z),
        Vec3(-x, 0.0, 0.0),
        Vec3(0.0, y, 0.0),
        material
    ));

    // Left and right faces.
    world.add(std::make_shared<Quad>(
        Vec3(minimum.x, minimum.y, minimum.z),
        Vec3(0.0, 0.0, z),
        Vec3(0.0, y, 0.0),
        material
    ));

    world.add(std::make_shared<Quad>(
        Vec3(maximum.x, minimum.y, maximum.z),
        Vec3(0.0, 0.0, -z),
        Vec3(0.0, y, 0.0),
        material
    ));

    // Top and bottom faces.
    world.add(std::make_shared<Quad>(
        Vec3(minimum.x, maximum.y, maximum.z),
        Vec3(x, 0.0, 0.0),
        Vec3(0.0, 0.0, -z),
        material
    ));

    world.add(std::make_shared<Quad>(
        Vec3(minimum.x, minimum.y, minimum.z),
        Vec3(x, 0.0, 0.0),
        Vec3(0.0, 0.0, z),
        material
    ));
}

inline HittableList make_showcase_scene() {
    HittableList world;

    // Room materials.
    auto floor_material =
        std::make_shared<Lambertian>(
            Vec3(0.68, 0.66, 0.60)
        );

    auto back_wall_material =
        std::make_shared<Lambertian>(
            Vec3(0.72, 0.75, 0.80)
        );

    auto left_wall_material =
        std::make_shared<Lambertian>(
            Vec3(0.72, 0.24, 0.18)
        );

    auto right_wall_material =
        std::make_shared<Lambertian>(
            Vec3(0.20, 0.40, 0.68)
        );

    // Room floor.
    world.add(std::make_shared<Quad>(
        Vec3(-3.0, 0.0, 1.0),
        Vec3(6.0, 0.0, 0.0),
        Vec3(0.0, 0.0, -5.0),
        floor_material
    ));

    // Back wall.
    world.add(std::make_shared<Quad>(
        Vec3(-3.0, 0.0, -4.0),
        Vec3(6.0, 0.0, 0.0),
        Vec3(0.0, 4.0, 0.0),
        back_wall_material
    ));

    // Left wall.
    world.add(std::make_shared<Quad>(
        Vec3(-3.0, 0.0, 1.0),
        Vec3(0.0, 0.0, -5.0),
        Vec3(0.0, 4.0, 0.0),
        left_wall_material
    ));

    // Right wall.
    world.add(std::make_shared<Quad>(
        Vec3(3.0, 0.0, -4.0),
        Vec3(0.0, 0.0, 5.0),
        Vec3(0.0, 4.0, 0.0),
        right_wall_material
    ));

    // Ceiling: close the room and keep the camera inside the enclosure.
    auto ceiling_material =
        std::make_shared<Lambertian>(
            Vec3(0.78, 0.77, 0.73)
        );

    world.add(std::make_shared<Quad>(
        Vec3(-3.0, 4.0, 1.0),
        Vec3(6.0, 0.0, 0.0),
        Vec3(0.0, 0.0, -5.0),
        ceiling_material
    ));

    // Rectangular area light facing down into the room.
    auto light_material =
        std::make_shared<DiffuseLight>(
            Vec3(7.0, 6.5, 5.5)
        );

    world.add(std::make_shared<Quad>(
        Vec3(-1.2, 3.85, -2.5),
        Vec3(2.4, 0.0, 0.0),
        Vec3(0.0, 0.0, 1.4),
        light_material
    ));

    // Pedestals with a 3D procedural stone texture.
    auto pedestal_texture =
        std::make_shared<PerlinNoiseTexture>(
            Vec3(0.04, 0.05, 0.07),
            Vec3(0.85, 0.72, 0.50),
            2.4,
            5,
            20261027
        );

    auto pedestal_material =
        std::make_shared<Lambertian>(
            pedestal_texture
        );

    add_showcase_box(
        world,
        Vec3(-1.85, 0.0, -2.1),
        Vec3(-0.45, 0.65, -0.7),
        pedestal_material
    );

    add_showcase_box(
        world,
        Vec3(0.35, 0.0, -2.1),
        Vec3(1.75, 0.65, -0.7),
        pedestal_material
    );

    // Red diffuse sphere.
    auto red_material =
        std::make_shared<Lambertian>(
            Vec3(0.80, 0.12, 0.08)
        );

    world.add(std::make_shared<Sphere>(
        Vec3(-1.15, 1.15, -1.4),
        0.50,
        red_material
    ));

    // Metallic sphere with slightly rough reflections.
    auto metal_material =
        std::make_shared<Metal>(
            Vec3(0.82, 0.86, 0.90),
            0.08
        );

    world.add(std::make_shared<Sphere>(
        Vec3(1.05, 1.12, -1.4),
        0.47,
        metal_material
    ));

    // Glass sphere.
    auto glass_material =
        std::make_shared<Dielectric>(1.5);

    world.add(std::make_shared<Sphere>(
        Vec3(0.0, 0.44, -2.45),
        0.42,
        glass_material
    ));

    auto gold_material =
        std::make_shared<PrincipledBSDF>(
            Vec3(0.85, 0.58, 0.16),
            0.65,  // Metallic
            0.24   // Roughness
        );

    world.add(std::make_shared<Sphere>(
        Vec3(-0.05, 0.28, -0.45),
        0.28,
        gold_material
    ));

    // Deep blue diffuse sphere for material and lighting comparison.
    auto blue_material =
        std::make_shared<Lambertian>(
            Vec3(0.08, 0.16, 0.62)
        );

    world.add(std::make_shared<Sphere>(
        Vec3(2.05, 0.34, -2.9),
        0.34,
        blue_material
    ));

    return world;
}
