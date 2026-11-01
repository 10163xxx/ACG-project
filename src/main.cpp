#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "acceleration/bvh.h"
#include "core/random.h"
#include "integrator/path_tracer.h"
#include "materials/environment_map.h"
#include "output/image.h"
#include "output/output_paths.h"
#include "scene/render_config.h"
#include "scene/scene_factory.h"

namespace {

bool parse_request(int argc, char* argv[], SceneRequest& request) {
    if (argc == 1) return true;

    std::string scene_name;
    std::string test_name;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--scene" || arg == "--test" || arg == "--obj") {
            if (++i >= argc) {
                std::cerr << "Missing value after " << arg << ".\n";
                return false;
            }
            if (arg == "--scene") scene_name = argv[i];
            else if (arg == "--test") test_name = argv[i];
            else request.obj_path = argv[i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage:\n"
                << "  path_tracer --scene showcase\n"
                << "  path_tracer --scene greenhouse\n"
                << "  path_tracer --scene test --test <uv|volume|inhomogeneous-volume|mipmap|mipmap-no-filter|obj|materials> [--obj path]\n";
            return false;
        } else {
            std::cerr << "Unknown argument: " << arg << '\n';
            return false;
        }
    }

    if (scene_name == "showcase") {
        request.scene = SceneKind::Showcase;
        return test_name.empty() && request.obj_path.empty();
    }
    if (scene_name == "greenhouse") {
        request.scene = SceneKind::Greenhouse;
        return test_name.empty() && request.obj_path.empty();
    }
    if (scene_name != "test" || test_name.empty()) {
        std::cerr << "Select --scene showcase, greenhouse, or test.\n";
        return false;
    }

    request.scene = SceneKind::Test;
    if (test_name == "uv") request.test = TestKind::UV;
    else if (test_name == "volume") request.test = TestKind::Volume;
    else if (test_name == "inhomogeneous-volume")
        request.test = TestKind::InhomogeneousVolume;
    else if (test_name == "mipmap") request.test = TestKind::Mipmap;
    else if (test_name == "mipmap-no-filter")
        request.test = TestKind::MipmapNoFilter;
    else if (test_name == "obj") request.test = TestKind::OBJ;
    else if (test_name == "materials") request.test = TestKind::Materials;
    else {
        std::cerr << "Unknown test: " << test_name << '\n';
        return false;
    }

    if ((request.test == TestKind::OBJ) != !request.obj_path.empty()) {
        std::cerr << "Only --test obj accepts and requires --obj <path>.\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        SceneRequest request;
        if (!parse_request(argc, argv, request)) return 1;

        RenderSetup setup = make_render_setup(request);
        Random::seed(2026);

        Image image(setup.width, setup.height);
        BVHNode world(setup.geometry.objects());
        PathTracer tracer(setup.max_depth);

        EnvironmentMap environment;
        const EnvironmentMap* active_environment = nullptr;
        if (setup.use_environment) {
            environment.load("assets/environments/studio_small_01_2k.hdr");
            active_environment = &environment;
        }

        const auto light_indices =
            DirectLighting::collect_light_indices(setup.geometry);
        const auto* point_lights = setup.point_lights.empty()
            ? nullptr : &setup.point_lights;

        for (int y = 0; y < setup.height; ++y) {
            std::cerr << "\rRendering row " << (y + 1)
                      << " / " << setup.height << std::flush;
            for (int x = 0; x < setup.width; ++x) {
                Vec3 color(0, 0, 0);
                for (int sample = 0; sample < setup.samples; ++sample) {
                    const double u =
                        (x + Random::random_double()) / setup.width;
                    const double v =
                        1.0 - (y + Random::random_double()) / setup.height;
                    const Ray ray = setup.camera->get_ray(
                        u, v, 1.0 / setup.width, -1.0 / setup.height);
                    color = color + tracer.trace(
                        ray, world, setup.geometry, point_lights,
                        active_environment, &light_indices);
                }
                color = color / setup.samples;
                image.set_pixel(x, y, color);
            }
        }

        image.write_ppm(setup.output);
        std::cerr << "\nRender complete: " << setup.output << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "\nRender failed: " << error.what() << '\n';
        return 1;
    }
}
