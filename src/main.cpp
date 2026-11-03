
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
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

        if (arg == "--preview") {
            request.preview = true;
        } else if (arg == "--draft") {
            request.draft = true;
        } else if (arg == "--scene" || arg == "--test" || arg == "--obj") {
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
                << "  Add --preview for a low-resolution preview render.\n"
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

    if (request.preview && request.draft) {
        std::cerr << "--preview and --draft cannot be used together.\n";
        return false;
    }

    if ((request.preview || request.draft) &&
        request.scene != SceneKind::Greenhouse) {
        std::cerr << "--preview and --draft are currently for Greenhouse only.\n";
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        SceneRequest request;

        if (!parse_request(argc, argv, request)) {
            return 1;
        }

        RenderSetup setup = make_render_setup(request);

        Image image(setup.width, setup.height);
        BVHNode world(setup.geometry.objects());
        PathTracer tracer(setup.max_depth);

        EnvironmentMap environment;
        const EnvironmentMap* active_environment = nullptr;

        if (setup.use_environment) {
            environment.load(
                "assets/environments/"
                "kloofendal_48d_partly_cloudy_puresky_2k.hdr"
            );
            active_environment = &environment;
        }

        const auto light_indices =
            DirectLighting::collect_light_indices(setup.geometry);

        const auto* point_lights = setup.point_lights.empty()
            ? nullptr
            : &setup.point_lights;

        const unsigned int hardware_threads =
            std::max(1U, std::thread::hardware_concurrency());

        const unsigned int worker_count =
            std::min(
                hardware_threads,
                static_cast<unsigned int>(setup.height)
            );

        std::atomic<int> next_row{0};
        std::atomic<int> completed_rows{0};
        std::atomic<bool> stop_workers{false};

        std::mutex output_mutex;
        std::mutex exception_mutex;
        std::exception_ptr worker_exception;

        const auto render_start =
            std::chrono::steady_clock::now();

        std::cout
            << "Rendering "
            << setup.width << " x " << setup.height
            << ", " << setup.samples << " samples, "
            << setup.max_depth << " max depth\n"
            << "Using " << worker_count
            << " worker threads\n";

        auto render_worker = [&](unsigned int worker_index) {
            // Each worker owns an independent thread-local RNG state.
            const std::uint64_t seed =
                2026ULL
                + static_cast<std::uint64_t>(worker_index)
                    * 0x9E3779B97F4A7C15ULL;

            Random::seed(seed);

            try {
                while (!stop_workers.load(std::memory_order_relaxed)) {
                    const int y =
                        next_row.fetch_add(1, std::memory_order_relaxed);

                    if (y >= setup.height) {
                        break;
                    }

                    for (int x = 0; x < setup.width; ++x) {
                        Vec3 color(0.0, 0.0, 0.0);

                        for (int sample = 0;
                             sample < setup.samples;
                             ++sample) {
                            const double u =
                                (x + Random::random_double())
                                / setup.width;

                            const double v =
                                1.0
                                - (y + Random::random_double())
                                    / setup.height;

                            const Ray ray = setup.camera->get_ray(
                                u,
                                v,
                                1.0 / setup.width,
                                -1.0 / setup.height
                            );

                            color = color + tracer.trace(
                                ray,
                                world,
                                setup.geometry,
                                point_lights,
                                active_environment,
                                &light_indices
                            );
                        }

                        color = color / setup.samples;
                        image.set_pixel(x, y, color);
                    }

                    const int finished =
                        completed_rows.fetch_add(
                            1,
                            std::memory_order_relaxed
                        ) + 1;

                    // Avoid excessive console synchronization.
                    if (finished % 5 == 0 ||
                        finished == setup.height) {
                        std::lock_guard<std::mutex> lock(output_mutex);

                        std::cout
                            << "\rRendering rows: "
                            << finished << " / " << setup.height
                            << std::flush;
                    }
                }
            } catch (...) {
                stop_workers.store(true, std::memory_order_relaxed);

                std::lock_guard<std::mutex> lock(exception_mutex);

                if (!worker_exception) {
                    worker_exception = std::current_exception();
                }
            }
        };

        std::vector<std::thread> workers;
        workers.reserve(worker_count);

        try {
            for (unsigned int i = 0; i < worker_count; ++i) {
                workers.emplace_back(render_worker, i);
            }
        } catch (...) {
            stop_workers.store(true, std::memory_order_relaxed);

            for (std::thread& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }

            throw;
        }

        for (std::thread& worker : workers) {
            worker.join();
        }

        if (worker_exception) {
            std::rethrow_exception(worker_exception);
        }

        const auto render_end =
            std::chrono::steady_clock::now();

        const double elapsed_seconds =
            std::chrono::duration<double>(
                render_end - render_start
            ).count();

        std::cout << '\n';

        image.write_ppm(setup.output);

        std::cout
            << "Render complete: " << setup.output << '\n'
            << "Render time: " << elapsed_seconds << " seconds\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "\nRender failed: " << error.what() << '\n';
        return 1;
    }
}
