#pragma once

#include <memory>

#include "geometry/hittable_list.h"
#include "geometry/sphere.h"

#include "materials/lambertian.h"
#include "materials/metal.h"
#include "materials/dielectric.h"
#include "materials/texture.h"
#include "materials/normal_mapped_lambertian.h"
#include "materials/diffuse_light.h"

inline HittableList make_material_test_scene() {
    HittableList world;

    auto ground_texture =
        std::make_shared<UVCheckerTexture>(
            16.0,
            Vec3(0.85, 0.85, 0.85),
            Vec3(0.12, 0.12, 0.12)
        );

    auto ground_material =
        std::make_shared<Lambertian>(ground_texture);
        
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

    auto sphere_texture =
        std::make_shared<UVCheckerTexture>(
            12.0,
            Vec3(0.95, 0.75, 0.25),
            Vec3(0.12, 0.25, 0.75)
        );

    auto diffuse_material =
        std::make_shared<Lambertian>(sphere_texture);

    world.add(std::make_shared<Sphere>(
        Vec3(0.0, 0.0, -1.0),
        0.5,
        diffuse_material
    ));

    auto normal_texture =
        std::make_shared<UVNormalTexture>(8.0);

    auto normal_mapped_material =
        std::make_shared<NormalMappedMaterial>(
            std::make_shared<Lambertian>(
                Vec3(0.8, 0.7, 0.3)
            ),
            normal_texture
        );
    world.add(std::make_shared<Sphere>(
        Vec3(0.0, 0.85, -1.0),
        0.35,
        normal_mapped_material
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
    
    auto light_material =
        std::make_shared<DiffuseLight>(
            Vec3(4.0, 3.5, 2.5)
        );

    world.add(std::make_shared<Sphere>(
        Vec3(0.0, 2.0, -1.0),
        0.4,
        light_material
    ));

    return world;
}
