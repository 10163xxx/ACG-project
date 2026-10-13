#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

#include "geometry/aabb.h"
#include "geometry/hittable.h"

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

        if (left_.get() == right_.get()) {
            return left_->hit(ray, ray_t, record);
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

    AABB bounding_box() const override {
        return box_;
    }

private:
    std::shared_ptr<const Hittable> left_;
    std::shared_ptr<const Hittable> right_;
    AABB box_;

    void build_node(
        std::vector<std::shared_ptr<const Hittable>>& objects,
        std::size_t begin,
        std::size_t end
    ) {
        const std::size_t count = end - begin;

        AABB bounds = objects[begin]->bounding_box();

        for (std::size_t i = begin + 1; i < end; ++i) {
            bounds = surrounding_box(
                bounds,
                objects[i]->bounding_box()
            );
        }

        const Vec3 extent = bounds.maximum - bounds.minimum;

        int axis = 0;
        if (extent.y > extent.x) {
            axis = 1;
        }

        const double current_extent =
            axis == 0 ? extent.x : extent.y;

        if (extent.z > current_extent) {
            axis = 2;
        }

        const auto centroid_axis = [axis](
            const std::shared_ptr<const Hittable>& object
        ) {
            const Vec3 c = object->bounding_box().centroid();

            if (axis == 0) return c.x;
            if (axis == 1) return c.y;
            return c.z;
        };

        std::sort(
            objects.begin() + begin,
            objects.begin() + end,
            [&](const auto& a, const auto& b) {
                return centroid_axis(a) < centroid_axis(b);
            }
        );

        if (count == 1) {
            left_ = objects[begin];
            right_ = objects[begin];
        } else if (count == 2) {
            left_ = objects[begin];
            right_ = objects[begin + 1];
        } else {
            const std::size_t mid = begin + count / 2;

            left_ = std::shared_ptr<const Hittable>(
                new BVHNode(objects, begin, mid)
            );

            right_ = std::shared_ptr<const Hittable>(
                new BVHNode(objects, mid, end)
            );
        }

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
