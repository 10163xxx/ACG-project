#pragma once

#include <memory>

#include "geometry/hittable_list.h"
#include "geometry/sphere.h"

#include "materials/lambertian.h"
#include "materials/metal.h"
#include "materials/dielectric.h"

inline HittableList make_material_test_scene() {
    HittableList world;

    auto ground_material =
        std::make_shared<Lambertian>(
            Vec3(0.7, 0.7, 0.7)
        );

    world.add(std::make_shared<Sphere>(
        Vec3(0.0, -100.5, -1.0),
        100.0,
        ground_material
    ));

    auto glass_material =
        std::make_shared<Dielectric>(1.5);

    world.add(std::make_shared<Sphere>(
        Vec3(-1.0, 0.0, -1.0),
        0.5,
        glass_material
    ));

    auto diffuse_material =
        std::make_shared<Lambertian>(
            Vec3(0.8, 0.2, 0.2)
        );

    world.add(std::make_shared<Sphere>(
        Vec3(0.0, 0.0, -1.0),
        0.5,
        diffuse_material
    ));

    auto metal_material =
        std::make_shared<Metal>(
            Vec3(0.8, 0.8, 0.8),
            0.15
        );

    world.add(std::make_shared<Sphere>(
        Vec3(1.0, 0.0, -1.0),
        0.5,
        metal_material
    ));

    return world;
}
