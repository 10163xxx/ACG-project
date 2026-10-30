#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include "geometry/aabb.h"
#include "geometry/hittable.h"

// A binned Surface Area Heuristic (SAH) BVH.
// SAH chooses a split by estimating the traversal cost of each candidate:
//   cost = 1 + (A_left * N_left + A_right * N_right) / A_parent
// where A is the surface area of a bounding box and N is the number of
// primitives in that child. Small leaves are retained when splitting would
// not improve this estimate.
class BVHNode : public Hittable {
public:
    explicit BVHNode(
        std::vector<std::shared_ptr<const Hittable>> objects
    ) {
        if (objects.empty()) {
            throw std::invalid_argument(
                "Cannot build BVH from an empty object list."
            );
        }

        for (const auto& object : objects) {
            if (!object) {
                throw std::invalid_argument(
                    "Cannot build BVH with a null object."
                );
            }
        }

        build_node(objects, 0, objects.size());
    }

    bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override {
        if (!box_.hit(ray, ray_t)) {
            return false;
        }

        if (is_leaf_) {
            bool hit_anything = false;
            double closest_so_far = ray_t.max;
            HitRecord candidate;

            for (const auto& object : leaf_objects_) {
                const Interval current_interval(
                    ray_t.min,
                    closest_so_far
                );

                if (object->hit(ray, current_interval, candidate)) {
                    hit_anything = true;
                    closest_so_far = candidate.t;
                    record = candidate;
                }
            }

            return hit_anything;
        }

        HitRecord left_record;
        const bool hit_left =
            left_->hit(ray, ray_t, left_record);

        const Interval right_interval(
            ray_t.min,
            hit_left ? left_record.t : ray_t.max
        );

        HitRecord right_record;
        const bool hit_right =
            right_->hit(ray, right_interval, right_record);

        if (hit_right) {
            record = right_record;
            return true;
        }

        if (hit_left) {
            record = left_record;
            return true;
        }

        return false;
    }

    void all_intersections(
        const Ray& ray,
        const Interval& ray_t,
        std::vector<HitRecord>& intersections
    ) const override {
        if (!box_.hit(ray, ray_t)) {
            return;
        }

        if (is_leaf_) {
            for (const auto& object : leaf_objects_) {
                object->all_intersections(
                    ray,
                    ray_t,
                    intersections
                );
            }
        } else {
            left_->all_intersections(
                ray,
                ray_t,
                intersections
            );

            right_->all_intersections(
                ray,
                ray_t,
                intersections
            );
        }

        std::sort(
            intersections.begin(),
            intersections.end(),
            [](const HitRecord& a, const HitRecord& b) {
                return a.t < b.t;
            }
        );
    }

    bool hit_surface(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override {
        if (!box_.hit(ray, ray_t)) {
            return false;
        }

        if (is_leaf_) {
            bool hit_anything = false;
            double closest_so_far = ray_t.max;
            HitRecord candidate;

            for (const auto& object : leaf_objects_) {
                const Interval current_interval(
                    ray_t.min,
                    closest_so_far
                );

                if (object->hit_surface(
                        ray,
                        current_interval,
                        candidate)) {
                    hit_anything = true;
                    closest_so_far = candidate.t;
                    record = candidate;
                }
            }

            return hit_anything;
        }

        HitRecord left_record;
        const bool hit_left =
            left_->hit_surface(ray, ray_t, left_record);

        const Interval right_interval(
            ray_t.min,
            hit_left ? left_record.t : ray_t.max
        );

        HitRecord right_record;
        const bool hit_right =
            right_->hit_surface(
                ray,
                right_interval,
                right_record
            );

        if (hit_right) {
            record = right_record;
            return true;
        }

        if (hit_left) {
            record = left_record;
            return true;
        }

        return false;
    }

    Vec3 transmittance(
        const Ray& ray,
        const Interval& ray_t
    ) const override {
        if (!box_.hit(ray, ray_t)) {
            return Vec3(1.0, 1.0, 1.0);
        }

        if (is_leaf_) {
            Vec3 result(1.0, 1.0, 1.0);

            for (const auto& object : leaf_objects_) {
                result = result
                    * object->transmittance(ray, ray_t);

                if (result.near_zero()) {
                    return Vec3(0.0, 0.0, 0.0);
                }
            }

            return result;
        }

        return left_->transmittance(ray, ray_t)
            * right_->transmittance(ray, ray_t);
    }

    AABB bounding_box() const override {
        return box_;
    }

private:
    static constexpr int kBucketCount = 12;
    static constexpr std::size_t kMaxLeafSize = 4;
    static constexpr double kExtentEpsilon = 1e-12;
    static constexpr double kAreaEpsilon = 1e-18;

    struct Bucket {
        std::size_t count = 0;
        bool has_bounds = false;
        AABB bounds;
    };

    std::shared_ptr<const Hittable> left_;
    std::shared_ptr<const Hittable> right_;
    std::vector<std::shared_ptr<const Hittable>> leaf_objects_;
    AABB box_;
    bool is_leaf_ = false;

    static double component(const Vec3& v, int axis) {
        if (axis == 0) return v.x;
        if (axis == 1) return v.y;
        return v.z;
    }

    static int bucket_index(
        double centroid,
        double centroid_min,
        double centroid_extent
    ) {
        const double normalized =
            (centroid - centroid_min) / centroid_extent;
        const int index = static_cast<int>(normalized * kBucketCount);
        return std::max(0, std::min(kBucketCount - 1, index));
    }

    void make_leaf(
        const std::vector<std::shared_ptr<const Hittable>>& objects,
        std::size_t begin,
        std::size_t end,
        const AABB& bounds
    ) {
        is_leaf_ = true;
        box_ = bounds;
        leaf_objects_.assign(objects.begin() + begin, objects.begin() + end);
        left_.reset();
        right_.reset();
    }

    void build_node(
        std::vector<std::shared_ptr<const Hittable>>& objects,
        std::size_t begin,
        std::size_t end
    ) {
        const std::size_t count = end - begin;
        if (count == 0) {
            throw std::logic_error("BVH builder received an empty range.");
        }

        std::vector<AABB> object_bounds;
        std::vector<Vec3> centroids;
        object_bounds.reserve(count);
        centroids.reserve(count);

        AABB bounds = objects[begin]->bounding_box();
        Vec3 centroid_min = objects[begin]->bounding_box().centroid();
        Vec3 centroid_max = centroid_min;
        object_bounds.push_back(bounds);
        centroids.push_back(centroid_min);

        for (std::size_t i = begin + 1; i < end; ++i) {
            const AABB object_box = objects[i]->bounding_box();
            const Vec3 centroid = object_box.centroid();
            object_bounds.push_back(object_box);
            centroids.push_back(centroid);
            bounds = surrounding_box(bounds, object_box);

            centroid_min = Vec3(
                std::min(centroid_min.x, centroid.x),
                std::min(centroid_min.y, centroid.y),
                std::min(centroid_min.z, centroid.z)
            );
            centroid_max = Vec3(
                std::max(centroid_max.x, centroid.x),
                std::max(centroid_max.y, centroid.y),
                std::max(centroid_max.z, centroid.z)
            );
        }

        box_ = bounds;

        if (count == 1) {
            make_leaf(objects, begin, end, bounds);
            return;
        }

        const Vec3 centroid_extent = centroid_max - centroid_min;
        const double parent_area = bounds.surface_area();

        int best_axis = -1;
        int best_split = -1;
        double best_cost = std::numeric_limits<double>::infinity();

        if (parent_area > kAreaEpsilon) {
            for (int axis = 0; axis < 3; ++axis) {
                const double axis_extent = component(centroid_extent, axis);
                if (axis_extent <= kExtentEpsilon) {
                    continue;
                }

                const double axis_min = component(centroid_min, axis);
                std::array<Bucket, kBucketCount> buckets{};

                for (std::size_t i = 0; i < count; ++i) {
                    const int index = bucket_index(
                        component(centroids[i], axis),
                        axis_min,
                        axis_extent
                    );
                    Bucket& bucket = buckets[index];
                    ++bucket.count;
                    if (!bucket.has_bounds) {
                        bucket.bounds = object_bounds[i];
                        bucket.has_bounds = true;
                    } else {
                        bucket.bounds = surrounding_box(
                            bucket.bounds,
                            object_bounds[i]
                        );
                    }
                }

                std::array<std::size_t, kBucketCount> left_count{};
                std::array<std::size_t, kBucketCount> right_count{};
                std::array<AABB, kBucketCount> left_bounds{};
                std::array<AABB, kBucketCount> right_bounds{};
                std::array<bool, kBucketCount> left_valid{};
                std::array<bool, kBucketCount> right_valid{};

                std::size_t running_count = 0;
                bool running_valid = false;
                AABB running_bounds;
                for (int i = 0; i < kBucketCount; ++i) {
                    const Bucket& bucket = buckets[i];
                    running_count += bucket.count;
                    if (bucket.has_bounds) {
                        running_bounds = running_valid
                            ? surrounding_box(running_bounds, bucket.bounds)
                            : bucket.bounds;
                        running_valid = true;
                    }
                    left_count[i] = running_count;
                    left_valid[i] = running_valid;
                    if (running_valid) left_bounds[i] = running_bounds;
                }

                running_count = 0;
                running_valid = false;
                for (int i = kBucketCount - 1; i >= 0; --i) {
                    const Bucket& bucket = buckets[i];
                    running_count += bucket.count;
                    if (bucket.has_bounds) {
                        running_bounds = running_valid
                            ? surrounding_box(running_bounds, bucket.bounds)
                            : bucket.bounds;
                        running_valid = true;
                    }
                    right_count[i] = running_count;
                    right_valid[i] = running_valid;
                    if (running_valid) right_bounds[i] = running_bounds;
                }

                for (int split = 0; split < kBucketCount - 1; ++split) {
                    if (left_count[split] == 0 ||
                        right_count[split + 1] == 0 ||
                        !left_valid[split] ||
                        !right_valid[split + 1]) {
                        continue;
                    }

                    const double cost = 1.0 + (
                        left_bounds[split].surface_area() * left_count[split] +
                        right_bounds[split + 1].surface_area() * right_count[split + 1]
                    ) / parent_area;

                    if (cost < best_cost) {
                        best_cost = cost;
                        best_axis = axis;
                        best_split = split;
                    }
                }
            }
        }

        // A small leaf is cheaper when the estimated split cost is not lower
        // than testing the primitives directly. Larger ranges are still split
        // to prevent unbounded leaf sizes in degenerate centroid layouts.
        if (count <= kMaxLeafSize &&
            (best_axis < 0 || best_cost >= static_cast<double>(count))) {
            make_leaf(objects, begin, end, bounds);
            return;
        }

        std::size_t mid = begin;
        if (best_axis >= 0) {
            const double axis_min = component(centroid_min, best_axis);
            const double axis_extent = component(centroid_extent, best_axis);
            auto first = objects.begin() + begin;
            auto last = objects.begin() + end;
            const auto split_point = std::partition(
                first,
                last,
                [&](const std::shared_ptr<const Hittable>& object) {
                    const double centroid = component(
                        object->bounding_box().centroid(), best_axis
                    );
                    return bucket_index(centroid, axis_min, axis_extent)
                        <= best_split;
                }
            );
            mid = static_cast<std::size_t>(split_point - objects.begin());
        }

        // Degenerate or empty SAH partitions fall back to a balanced median
        // split along the largest centroid extent.
        if (mid <= begin || mid >= end) {
            int axis = 0;
            if (centroid_extent.y > centroid_extent.x) axis = 1;
            if (centroid_extent.z > component(centroid_extent, axis)) axis = 2;

            std::sort(
                objects.begin() + begin,
                objects.begin() + end,
                [axis](
                    const std::shared_ptr<const Hittable>& a,
                    const std::shared_ptr<const Hittable>& b
                ) {
                    return component(a->bounding_box().centroid(), axis)
                        < component(b->bounding_box().centroid(), axis);
                }
            );
            mid = begin + count / 2;
        }

        is_leaf_ = false;
        leaf_objects_.clear();
        left_ = std::shared_ptr<const Hittable>(
            new BVHNode(objects, begin, mid)
        );
        right_ = std::shared_ptr<const Hittable>(
            new BVHNode(objects, mid, end)
        );
        box_ = surrounding_box(
            left_->bounding_box(),
            right_->bounding_box()
        );
    }

    BVHNode(
        std::vector<std::shared_ptr<const Hittable>>& objects,
        std::size_t begin,
        std::size_t end
    ) {
        build_node(objects, begin, end);
    }
};
