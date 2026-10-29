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
#include "geometry/inhomogeneous_medium.h"
#include "geometry/obj_loader.h"
#include "geometry/quad.h"
#include "geometry/sphere.h"
#include "integrator/path_tracer.h"
#include "materials/environment_map.h"
#include "materials/diffuse_light.h"
#include "materials/lambertian.h"
#include "materials/image_texture.h"
#include "materials/point_light.h"
#include "materials/texture.h"
#include "output/image.h"
#include "output/output_paths.h"
#include "scene/obj_scene.h"
#include "scene/showcase_scene.h"
#include "scene/test_scene.h"

int main(int argc, char* argv[]) {
    std::string mode;

    if (argc > 1) {
        mode = argv[1];
    }

    const bool uv_test = mode == "--uv-test";
    const bool volume_test = mode == "--volume-test";
    const bool inhomogeneous_volume_test =
        mode == "--inhomogeneous-volume-test";
    const bool obj_test = mode == "--obj";
    const bool mipmap_test = mode == "--mipmap-test";
    const bool mipmap_test_no_filter =
        mode == "--mipmap-test-lod0";

    if (obj_test && argc < 3) {
        std::cerr
            << "Usage: renderer --obj <path-to-model.obj>\n";
        return 1;
    }

    if (!mode.empty() &&
        mode != "--uv-test" &&
        mode != "--volume-test" &&
        mode != "--inhomogeneous-volume-test" &&
        mode != "--obj" &&
        mode != "--mipmap-test" &&
        mode != "--mipmap-test-lod0") {
        std::cerr
            << "Unknown option: " << mode << '\n'
            << "Available modes:\n"
            << "  renderer\n"
            << "  renderer --uv-test\n"
            << "  renderer --volume-test\n"
            << "  renderer --inhomogeneous-volume-test\n"
            << "  renderer --obj <path-to-model.obj>\n"
            << "  renderer --mipmap-test\n"
            << "  renderer --mipmap-test-lod0\n";
        return 1;
    }

    std::string obj_path;

    if (obj_test) {
        obj_path = argv[2];
    }

    int image_width = 480;
    int image_height = 270;
    int samples_per_pixel = 256;
    int max_depth = 24;

    if (mipmap_test || mipmap_test_no_filter) {
        image_width = 320;
        image_height = 240;
        samples_per_pixel = 16;
        max_depth = 8;
    } else if (volume_test || inhomogeneous_volume_test) {
        image_width = 320;
        image_height = 270;
        samples_per_pixel = 128;
        max_depth = 12;
    } else if (uv_test) {
        image_height = 360;
        samples_per_pixel = 64;
        max_depth = 12;
    }

    std::string output_file;

    if (mipmap_test) {
        output_file =
            OutputPaths::RenderDirectory + "mipmap_test.ppm";
    } else if (mipmap_test_no_filter) {
        output_file =
            OutputPaths::RenderDirectory + "mipmap_test_lod0.ppm";
    } else if (uv_test) {
        output_file =
            OutputPaths::RenderDirectory + "uv_test.ppm";
    } else if (inhomogeneous_volume_test) {
        output_file =
            OutputPaths::RenderDirectory
            + "inhomogeneous_volume_test.ppm";
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

    std::shared_ptr<Camera> camera;

    if (mipmap_test || mipmap_test_no_filter) {
        camera = std::make_shared<Camera>(
            Vec3(0.0, 0.0, 3.2),
            Vec3(0.0, 0.0, 0.0),
            Vec3(0.0, 1.0, 0.0),
            40.0,
            aspect_ratio
        );
    } else if (obj_test) {
        camera = std::make_shared<Camera>(
            Vec3(4.0, -6.0, 4.0),
            Vec3(0.0, 0.0, 0.8),
            Vec3(0.0, 0.0, 1.0),
            38.0,
            aspect_ratio
        );
    } else if (uv_test) {
        camera = std::make_shared<Camera>(
            Vec3(3.2, 2.4, 4.2),
            Vec3(0.0, 0.0, 0.0),
            Vec3(0.0, 1.0, 0.0),
            38.0,
            aspect_ratio
        );
    } else {
        camera = std::make_shared<Camera>(
            Vec3(4.0, 2.35, 5.8),
            Vec3(0.0, 1.45, -1.7),
            Vec3(0.0, 1.0, 0.0),
            42.0,
            aspect_ratio,
            0.10,
            8.6
        );
    }

    HittableList scene;

    if (mipmap_test || mipmap_test_no_filter) {
        auto image_texture =
            std::make_shared<ImageTexture>(
                "assets/textures/bump_test.ppm",
                !mipmap_test_no_filter
            );

        auto material =
            std::make_shared<Lambertian>(image_texture);

        scene.add(std::make_shared<Sphere>(
            Vec3(0.0, 0.0, 0.0),
            1.0,
            material
        ));
    } else if (uv_test) {
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
    } else if (inhomogeneous_volume_test) {
        auto floor_material =
            std::make_shared<Lambertian>(
                Vec3(0.75, 0.75, 0.75)
            );

        scene.add(std::make_shared<Quad>(
            Vec3(-4.0, 0.0, 2.0),
            Vec3(8.0, 0.0, 0.0),
            Vec3(0.0, 0.0, -8.0),
            floor_material
        ));

        auto light_material =
            std::make_shared<DiffuseLight>(
                Vec3(12.0, 12.0, 12.0)
            );

        scene.add(std::make_shared<Quad>(
            Vec3(-1.5, 3.8, -2.8),
            Vec3(3.0, 0.0, 0.0),
            Vec3(0.0, 0.0, 2.0),
            light_material
        ));

        auto boundary = std::make_shared<Sphere>(
            Vec3(0.0, 1.5, -1.8),
            1.05,
            nullptr
        );

        scene.add(
            std::make_shared<InhomogeneousMedium>(
                boundary,
                0.9,
                Vec3(0.85, 0.92, 1.0)
            )
        );
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

                const Ray ray = camera->get_ray(
                    u,
                    v,
                    1.0 / image_width,
                    -1.0 / image_height
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