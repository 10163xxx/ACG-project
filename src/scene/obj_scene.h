#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "geometry/hittable_list.h"
#include "geometry/obj_loader.h"
#include "geometry/sphere.h"

#include "materials/lambertian.h"
#include "materials/diffuse_light.h"

#include "scene/test_scene.h"

inline HittableList make_obj_scene(
    const std::string& obj_path
) {
    HittableList world = make_material_test_scene();

    auto mesh_material =
        std::make_shared<Lambertian>(
            Vec3(0.7, 0.25, 0.15)
        );

    TriangleMesh mesh = ObjLoader::load_file(
        obj_path,
        mesh_material
    );

    world.add(
        std::make_shared<TriangleMesh>(
            std::move(mesh)
        )
    );

    return world;
}
