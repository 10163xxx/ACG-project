#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "acceleration/bvh.h"
#include "camera/camera.h"
#include "core/random.h"
#include "geometry/constant_medium.h"
#include "geometry/obj_loader.h"
#include "geometry/sphere.h"
#include "integrator/path_tracer.h"
#include "materials/environment_map.h"
#include "materials/lambertian.h"
#include "materials/point_light.h"
#include "materials/texture.h"
#include "output/image.h"
#include "output/output_paths.h"
#include "scene/obj_scene.h"
#include "scene/showcase_scene.h"
#include "scene/test_scene.h"

int main(int argc, char* argv[]) {
    const std::string mode =
        argc > 1 ? std::string(argv[1]) : "";

    const bool uv_test = mode == "--uv-test";
    const bool volume_test = mode == "--volume-test";
    const bool obj_test = mode == "--obj";

    if (obj_test && argc < 3) {
        std::cerr
            << "Usage: renderer --obj <path-to-model.obj>\n";
        return 1;
    }

    if (!mode.empty() &&
        mode != "--uv-test" &&
        mode != "--volume-test" &&
        mode != "--obj") {
        std::cerr
            << "Unknown option: " << mode << '\n'
            << "Available modes:\n"
            << "  renderer\n"
            << "  renderer --uv-test\n"
            << "  renderer --volume-test\n"
            << "  renderer --obj <path-to-model.obj>\n";
        return 1;
    }

    const std::string obj_path =
        obj_test ? std::string(argv[2]) : "";

    const int image_width =
        volume_test ? 320 : 480;

    const int image_height =
        volume_test ? 270 : (uv_test ? 360 : 270);

    const int samples_per_pixel =
        volume_test ? 128 : (uv_test ? 64 : 256);

    const int max_depth =
        volume_test ? 12 : (uv_test ? 12 : 24);

    std::string output_file;

    if (uv_test) {
        output_file =
            OutputPaths::RenderDirectory + "uv_test.ppm";
    } else if (volume_test) {
        output_file =
            OutputPaths::RenderDirectory + "volume_test.ppm";
    } else if (obj_test) {
        output_file =
            OutputPaths::RenderDirectory + "obj_scene.ppm";
    } else {
        output_file =
            OutputPaths::RenderDirectory + "showcase_scene.ppm";
    }

    Random::seed(2026);

    Image image(image_width, image_height);

    const double aspect_ratio =
        static_cast<double>(image_width) / image_height;


    Camera camera = obj_test
        ? Camera(
            Vec3(4.0, -6.0, 4.0),
            Vec3(0.0, 0.0, 0.8),
            Vec3(0.0, 0.0, 1.0),
            38.0,
            aspect_ratio
        )
        : (uv_test
            ? Camera(
                Vec3(3.2, 2.4, 4.2),
                Vec3(0.0, 0.0, 0.0),
                Vec3(0.0, 1.0, 0.0),
                38.0,
                aspect_ratio
            )
            : Camera(
                Vec3(4.0, 2.35, 5.8),
                Vec3(0.0, 1.45, -1.7),
                Vec3(0.0, 1.0, 0.0),
                42.0,
                aspect_ratio,
                0.10,
                8.6
            ));


    HittableList scene;

    if (uv_test) {
        auto checker = std::make_shared<UVCheckerTexture>(
            8.0,
            Vec3(0.95, 0.12, 0.08),
            Vec3(0.95, 0.85, 0.12)
        );

        auto material =
            std::make_shared<Lambertian>(checker);

        TriangleMesh mesh = ObjLoader::load_file(
            "assets/uv_test.obj",
            material
        );

        scene.add(
            std::make_shared<TriangleMesh>(
                std::move(mesh)
            )
        );
    } else if (obj_test) {
        scene = make_obj_scene(obj_path);
    } else {
        scene = make_showcase_scene();

        if (volume_test) {
            auto boundary = std::make_shared<Sphere>(
                Vec3(0.0, 1.5, -1.8),
                1.05,
                nullptr
            );

            scene.add(
                std::make_shared<ConstantMedium>(
                    boundary,
                    0.9,
                    Vec3(0.85, 0.92, 1.0)
                )
            );
        }
    }

    BVHNode world(scene.objects());

    PathTracer tracer(max_depth);

    EnvironmentMap environment(
        "assets/studio_small_01_2k.hdr"
    );

    const std::vector<PointLight> point_lights = {
        PointLight(
            Vec3(-1.0, 2.8, -0.8),
            Vec3(18.0, 12.0, 7.0)
        )
    };

    const std::vector<std::size_t> light_indices =
        DirectLighting::collect_light_indices(scene);

    for (int y = 0; y < image_height; ++y) {
        std::cerr
            << "\rRendering row "
            << (y + 1) << " / " << image_height
            << std::flush;

        for (int x = 0; x < image_width; ++x) {
            Vec3 pixel_color(0.0, 0.0, 0.0);

            for (int sample = 0;
                 sample < samples_per_pixel;
                 ++sample) {

                const double u =
                    (x + Random::random_double())
                    / image_width;

                const double v =
                    1.0 - (y + Random::random_double())
                    / image_height;

                const Ray ray = camera.get_ray(
                    u,
                    v,
                    1.0 / image_width,
                    1.0 / image_height
                );

                pixel_color =
                    pixel_color
                    + tracer.trace(
                        ray,
                        world,
                        scene,
                        &point_lights,
                        &environment,
                        &light_indices
                    );
            }

            pixel_color =
                pixel_color / samples_per_pixel;

            pixel_color = Vec3(
                std::sqrt(std::max(0.0, pixel_color.x)),
                std::sqrt(std::max(0.0, pixel_color.y)),
                std::sqrt(std::max(0.0, pixel_color.z))
            );

            image.set_pixel(x, y, pixel_color);
        }
    }

    image.write_ppm(output_file);

    std::cerr
        << "\nRender complete: "
        << output_file << '\n';

    return 0;
}