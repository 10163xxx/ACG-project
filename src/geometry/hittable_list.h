#pragma once

#include <memory>
#include <stdexcept>
#include <vector>
#include <utility>

#include "geometry/hittable.h"

class HittableList : public Hittable {
public:
    HittableList() = default;

    void clear() {
        objects_.clear();
    }

    void add(std::shared_ptr<const Hittable> object) {
        if (!object) {
            throw std::invalid_argument(
                "Cannot add a null object to HittableList."
            );
        }

        objects_.push_back(std::move(object));
    }

    std::size_t size() const {
        return objects_.size();
    }

    bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override {
        HitRecord temp_record;
        bool hit_anything = false;

        double closest_so_far = ray_t.max;

        for (const auto& object : objects_) {
            const Interval current_interval(
                ray_t.min,
                closest_so_far
            );

            if (object->hit(
                    ray,
                    current_interval,
                    temp_record
                )) {
                hit_anything = true;
                closest_so_far = temp_record.t;
                record = temp_record;
            }
        }

        return hit_anything;
    }

private:
    std::vector<std::shared_ptr<const Hittable>> objects_;
};
