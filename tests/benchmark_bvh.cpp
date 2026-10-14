#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "acceleration/bvh.h"
#include "geometry/hittable.h"
#include "geometry/hittable_list.h"
#include "geometry/sphere.h"

// Baseline implementation matching the previous longest-axis + median split.
class MedianBVHNode : public Hittable {
public:
    explicit MedianBVHNode(std::vector<std::shared_ptr<const Hittable>> objects) {
        if (objects.empty()) throw std::invalid_argument("Empty object list");
        build(objects, 0, objects.size());
    }

    bool hit(const Ray& ray, const Interval& ray_t, HitRecord& record) const override {
        if (!box_.hit(ray, ray_t)) return false;
        if (left_.get() == right_.get()) return left_->hit(ray, ray_t, record);

        HitRecord left_record;
        const bool hit_left = left_->hit(ray, ray_t, left_record);
        const Interval right_interval(ray_t.min, hit_left ? left_record.t : ray_t.max);
        HitRecord right_record;
        const bool hit_right = right_->hit(ray, right_interval, right_record);

        if (hit_right) { record = right_record; return true; }
        if (hit_left) { record = left_record; return true; }
        return false;
    }

    AABB bounding_box() const override { return box_; }

private:
    std::shared_ptr<const Hittable> left_, right_;
    AABB box_;

    static double component(const Vec3& v, int axis) {
        return axis == 0 ? v.x : (axis == 1 ? v.y : v.z);
    }

    void build(std::vector<std::shared_ptr<const Hittable>>& objects,
               std::size_t begin, std::size_t end) {
        const std::size_t count = end - begin;
        AABB bounds = objects[begin]->bounding_box();
        Vec3 cmin = bounds.centroid(), cmax = cmin;
        for (std::size_t i = begin + 1; i < end; ++i) {
            const AABB b = objects[i]->bounding_box();
            bounds = surrounding_box(bounds, b);
            const Vec3 c = b.centroid();
            cmin.x = std::min(cmin.x, c.x); cmax.x = std::max(cmax.x, c.x);
            cmin.y = std::min(cmin.y, c.y); cmax.y = std::max(cmax.y, c.y);
            cmin.z = std::min(cmin.z, c.z); cmax.z = std::max(cmax.z, c.z);
        }
        box_ = bounds;
        if (count == 1) { left_ = right_ = objects[begin]; return; }

        const Vec3 extent = cmax - cmin;
        int axis = 0;
        if (extent.y > extent.x) axis = 1;
        if (component(extent, 2) > component(extent, axis)) axis = 2;
        std::sort(objects.begin() + begin, objects.begin() + end,
            [axis](const auto& a, const auto& b) {
                return component(a->bounding_box().centroid(), axis) <
                       component(b->bounding_box().centroid(), axis);
            });

        const std::size_t mid = begin + count / 2;
        left_ = std::shared_ptr<const Hittable>(new MedianBVHNode(objects, begin, mid));
        right_ = std::shared_ptr<const Hittable>(new MedianBVHNode(objects, mid, end));
        box_ = surrounding_box(left_->bounding_box(), right_->bounding_box());
    }

    MedianBVHNode(std::vector<std::shared_ptr<const Hittable>>& objects,
                  std::size_t begin, std::size_t end) {
        build(objects, begin, end);
    }
};

struct RayResult {
    std::uint64_t hit_count = 0;
    double checksum = 0.0;
};

template <typename World>
static double benchmark_hits(const World& world, const std::vector<Ray>& rays,
                             int repetitions, RayResult& result) {
    // Warm-up: reduce one-time effects before timing.
    HitRecord warm_record;
    for (std::size_t i = 0; i < std::min<std::size_t>(rays.size(), 1000); ++i)
        world.hit(rays[i], Interval(0.001, 1e6), warm_record);

    result = {};
    const auto start = std::chrono::steady_clock::now();
    for (int rep = 0; rep < repetitions; ++rep) {
        for (const Ray& ray : rays) {
            HitRecord record;
            if (world.hit(ray, Interval(0.001, 1e6), record)) {
                ++result.hit_count;
                result.checksum += record.t;
            }
        }
    }
    const auto stop = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(stop - start).count();
}

template <typename Builder>
static double measure_build_ms(Builder&& builder) {
    const auto start = std::chrono::steady_clock::now();
    builder();
    const auto stop = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(stop - start).count();
}

int main(int argc, char** argv) {
    const int sphere_count = argc > 1 ? std::max(100, std::stoi(argv[1])) : 2000;
    const int ray_count = argc > 2 ? std::max(1000, std::stoi(argv[2])) : 50000;
    const int repetitions = argc > 3 ? std::max(1, std::stoi(argv[3])) : 3;

    std::mt19937_64 rng(20261014);
    std::uniform_real_distribution<double> xy(-30.0, 30.0);
    std::uniform_real_distribution<double> zdist(-80.0, -5.0);
    std::uniform_real_distribution<double> radius(0.15, 0.65);

    std::vector<std::shared_ptr<const Hittable>> objects;
    objects.reserve(static_cast<std::size_t>(sphere_count));
    HittableList linear;
    for (int i = 0; i < sphere_count; ++i) {
        auto sphere = std::make_shared<Sphere>(
            Vec3(xy(rng), xy(rng), zdist(rng)), radius(rng));
        objects.push_back(sphere);
        linear.add(sphere);
    }

    // Rays target the scene volume and include misses to resemble a renderer workload.
    std::uniform_real_distribution<double> ox(-35.0, 35.0);
    std::uniform_real_distribution<double> oy(-35.0, 35.0);
    std::uniform_real_distribution<double> dx(-0.6, 0.6);
    std::uniform_real_distribution<double> dy(-0.6, 0.6);
    std::vector<Ray> rays;
    rays.reserve(static_cast<std::size_t>(ray_count));
    for (int i = 0; i < ray_count; ++i) {
        Vec3 origin(ox(rng), oy(rng), 2.0);
        Vec3 direction = normalize(Vec3(dx(rng), dy(rng), -1.0));
        rays.emplace_back(origin, direction);
    }

    std::unique_ptr<MedianBVHNode> median;
    const double median_build_ms = measure_build_ms([&] {
        median = std::make_unique<MedianBVHNode>(objects);
    });
    std::unique_ptr<BVHNode> sah;
    const double sah_build_ms = measure_build_ms([&] {
        sah = std::make_unique<BVHNode>(objects);
    });

    RayResult linear_result, median_result, sah_result;
    const double linear_ms = benchmark_hits(linear, rays, repetitions, linear_result);
    const double median_ms = benchmark_hits(*median, rays, repetitions, median_result);
    const double sah_ms = benchmark_hits(*sah, rays, repetitions, sah_result);

    const bool consistent =
        linear_result.hit_count == median_result.hit_count &&
        linear_result.hit_count == sah_result.hit_count &&
        std::abs(linear_result.checksum - median_result.checksum) < 1e-7 &&
        std::abs(linear_result.checksum - sah_result.checksum) < 1e-7;

    std::cout << "ACG BVH benchmark (deterministic seed=20261014)\n"
              << "Spheres: " << sphere_count << ", rays: " << ray_count
              << ", repetitions: " << repetitions << "\n"
              << "All times are wall-clock milliseconds; build time excludes scene generation.\n\n"
              << std::left << std::setw(22) << "Method"
              << std::right << std::setw(15) << "Build ms"
              << std::setw(15) << "Trace ms"
              << std::setw(15) << "Hit count" << "\n"
              << std::string(67, '-') << "\n"
              << std::left << std::setw(22) << "Linear HittableList"
              << std::right << std::setw(15) << "n/a"
              << std::setw(15) << std::fixed << std::setprecision(3) << linear_ms
              << std::setw(15) << linear_result.hit_count << "\n"
              << std::left << std::setw(22) << "Median BVH"
              << std::right << std::setw(15) << median_build_ms
              << std::setw(15) << median_ms
              << std::setw(15) << median_result.hit_count << "\n"
              << std::left << std::setw(22) << "SAH BVH"
              << std::right << std::setw(15) << sah_build_ms
              << std::setw(15) << sah_ms
              << std::setw(15) << sah_result.hit_count << "\n\n"
              << "Result consistency: " << (consistent ? "PASS" : "FAIL") << "\n";

    if (median_ms > 0.0 && sah_ms > 0.0) {
        std::cout << "SAH trace speedup vs median BVH: "
                  << std::setprecision(2) << (median_ms / sah_ms) << "x\n";
    }
    if (linear_ms > 0.0 && sah_ms > 0.0) {
        std::cout << "SAH trace speedup vs linear: "
                  << std::setprecision(2) << (linear_ms / sah_ms) << "x\n";
    }
    return consistent ? 0 : 2;
}
