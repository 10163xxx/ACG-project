#include <algorithm>
#include <cmath>
#include <iostream>
#include <chrono>
#include <string>

#include "acceleration/bvh.h"
#include "core/random.h"
#include "camera/camera.h"
#include "integrator/path_tracer.h"
#include "output/image.h"
#include "output/output_paths.h"
#include "scene/test_scene.h"
#include "scene/obj_scene.h"
#include "scene/showcase_scene.h"

int main() {
    constexpr int image_width = 480;
    constexpr int image_height = 270;
    constexpr int samples_per_pixel = 64;
    constexpr int max_depth = 24;

    Random::seed(2026);

    Image image(image_width, image_height);

    const double aspect_ratio =
        static_cast<double>(image_width) / image_height;

    Camera camera(
        Vec3(3.8, 2.2, 5.5),
        Vec3(0.0, 1.35, -1.7),
        Vec3(0.0, 1.0, 0.0),
        40.0,
        aspect_ratio
    );

    HittableList scene = make_showcase_scene();

    BVHNode world(scene.objects());

    PathTracer tracer(max_depth);

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

                const Ray ray = camera.get_ray(u, v);

                pixel_color =
                    pixel_color + tracer.trace(ray, world, scene);
            }

            pixel_color =
                pixel_color / samples_per_pixel;

            // Simple gamma correction for display.
            pixel_color = Vec3(
                std::sqrt(std::max(0.0, pixel_color.x)),
                std::sqrt(std::max(0.0, pixel_color.y)),
                std::sqrt(std::max(0.0, pixel_color.z))
            );

            image.set_pixel(x, y, pixel_color);
        }
    }

    const std::string output_file =
        OutputPaths::RenderDirectory + "showcase_scene.ppm";

    image.write_ppm(output_file);

    std::cerr << "\nRender complete: "
              << output_file << '\n';

    return 0;
}
