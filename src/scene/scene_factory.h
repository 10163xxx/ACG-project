#pragma once

#include <memory>
#include <stdexcept>
#include <utility>
#include <algorithm>

#include "geometry/constant_medium.h"
#include "geometry/inhomogeneous_medium.h"
#include "geometry/obj_loader.h"
#include "geometry/quad.h"
#include "geometry/sphere.h"
#include "materials/diffuse_light.h"
#include "materials/image_texture.h"
#include "materials/lambertian.h"
#include "output/output_paths.h"
#include "scene/greenhouse_scene.h"
#include "scene/obj_scene.h"
#include "scene/render_config.h"
#include "scene/showcase_scene.h"
#include "scene/test_scene.h"

inline RenderSetup make_render_setup(const SceneRequest& request) {
    RenderSetup s;
    s.point_lights.emplace_back(
        Vec3(-1, 2.8, -.8), Vec3(18, 12, 7));

    Vec3 from(4, 2.35, 5.8);
    Vec3 at(0, 1.45, -1.7);
    Vec3 up(0, 1, 0);
    double fov = 42;
    double aperture = .10;
    double focus = 8.6;

    if (request.scene == SceneKind::Showcase) {
        s.geometry = make_showcase_scene();
        s.output = OutputPaths::RenderDirectory + "showcase_scene.ppm";
    } else if (request.scene == SceneKind::Greenhouse) {
        s.width = 640;
        s.height = 360;
        s.samples = 192;
        s.max_depth = 24;
        from = Vec3(2.8, 2.75, 10.5);
        at = Vec3(0.30, 1.80, -2.7);
        fov = 38;
        aperture = 0.0;
        focus = 13.5;
        s.geometry = make_greenhouse_scene();
        s.output = OutputPaths::RenderDirectory + "greenhouse_scene.ppm";
        s.point_lights.clear();
    } else {
        switch (request.test) {
        case TestKind::Mipmap:
        case TestKind::MipmapNoFilter: {
            s.width = 320; s.height = 240;
            s.samples = 16; s.max_depth = 8;
            from = Vec3(0, 0, 3.2); at = Vec3(0, 0, 0);
            fov = 40; aperture = 0; focus = 1;
            const bool filtered = request.test == TestKind::Mipmap;
            auto texture = std::make_shared<ImageTexture>(
                "assets/tests/textures/bump_test.ppm", filtered);
            auto material = std::make_shared<Lambertian>(texture);
            s.geometry.add(std::make_shared<Sphere>(
                Vec3(0, 0, 0), 1, material));
            s.output = OutputPaths::RenderDirectory
                + (filtered ? "mipmap_test.ppm" : "mipmap_test_lod0.ppm");
            break;
        }
        case TestKind::UV: {
            s.height = 360; s.samples = 64; s.max_depth = 12;
            from = Vec3(3.2, 2.4, 4.2); at = Vec3(0, 0, 0);
            fov = 38; aperture = 0; focus = 1;
            auto texture = std::make_shared<UVCheckerTexture>(
                8, Vec3(.95, .12, .08), Vec3(.95, .85, .12));
            auto material = std::make_shared<Lambertian>(texture);
            TriangleMesh mesh = ObjLoader::load_file(
                "assets/tests/models/uv_test.obj", material);
            s.geometry.add(std::make_shared<TriangleMesh>(std::move(mesh)));
            s.output = OutputPaths::RenderDirectory + "uv_test.ppm";
            break;
        }
        case TestKind::Volume: {
            s.width = 320; s.height = 270;
            s.samples = 128; s.max_depth = 12;
            s.geometry = make_showcase_scene();
            auto boundary = std::make_shared<Sphere>(
                Vec3(0, 1.5, -1.8), 1.05, nullptr);
            s.geometry.add(std::make_shared<ConstantMedium>(
                boundary, .9, Vec3(.85, .92, 1)));
            s.output = OutputPaths::RenderDirectory + "volume_test.ppm";
            break;
        }
        case TestKind::InhomogeneousVolume: {
            s.width = 320; s.height = 270;
            s.samples = 128; s.max_depth = 12;
            s.use_environment = false;
            s.point_lights.clear();
            auto floor = std::make_shared<Lambertian>(Vec3(.75, .75, .75));
            s.geometry.add(std::make_shared<Quad>(
                Vec3(-4, 0, 2), Vec3(8, 0, 0), Vec3(0, 0, -8), floor));
            auto light = std::make_shared<DiffuseLight>(Vec3(4, 4, 4));
            s.geometry.add(std::make_shared<Quad>(
                Vec3(-1.5, 3.8, -2.8), Vec3(3, 0, 0),
                Vec3(0, 0, 2), light));
            auto boundary = std::make_shared<Sphere>(
                Vec3(0, 1.5, -1.8), 1.05, nullptr);
            s.geometry.add(std::make_shared<InhomogeneousMedium>(
                boundary, .55, Vec3(.85, .92, 1)));
            s.output = OutputPaths::RenderDirectory
                + "inhomogeneous_volume_test.ppm";
            break;
        }
        case TestKind::OBJ:
            if (request.obj_path.empty()) {
                throw std::invalid_argument("OBJ test requires an OBJ path.");
            }
            from = Vec3(4, -6, 4); at = Vec3(0, 0, .8);
            up = Vec3(0, 0, 1); fov = 38;
            aperture = 0; focus = 1;
            s.geometry = make_obj_scene(request.obj_path);
            s.output = OutputPaths::RenderDirectory + "obj_scene.ppm";
            break;
        case TestKind::Materials:
            s.geometry = make_material_test_scene();
            s.output = OutputPaths::RenderDirectory + "material_test.ppm";
            break;
        }
    }

    if (request.draft) {
        s.samples = 24;
        s.max_depth = 16;

        const std::size_t extension = s.output.rfind(".ppm");
        if (extension != std::string::npos) {
            s.output.insert(extension, "_draft");
        }
    }

    if (request.preview) {
        s.width = std::max(1, s.width / 2);
        s.height = std::max(1, s.height / 2);
        s.samples = 12;
        s.max_depth = 12;

        const std::size_t extension = s.output.rfind(".ppm");
        if (extension != std::string::npos) {
            s.output.insert(extension, "_preview");
        }
    }

    const double aspect =
        static_cast<double>(s.width) / static_cast<double>(s.height);
    s.camera = std::make_shared<Camera>(
        from, at, up, fov, aspect, aperture, focus);
    return s;
}
