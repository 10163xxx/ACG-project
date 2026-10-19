#pragma once

#include <memory>
#include <string>
#include <utility>

#include "geometry/hittable_list.h"
#include "geometry/obj_loader.h"
#include "materials/lambertian.h"

inline HittableList make_obj_scene(
    const std::string& obj_path
) {
    HittableList world;

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
