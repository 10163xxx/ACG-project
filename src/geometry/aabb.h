#pragma once

#include <algorithm>
#include <cmath>

#include "core/interval.h"
#include "core/ray.h"
#include "core/vec3.h"

class AABB {
public:
    Vec3 minimum;
    Vec3 maximum;

    AABB()
        : minimum(0.0, 0.0, 0.0),
          maximum(0.0, 0.0, 0.0) {}

    AABB(const Vec3& min_point, const Vec3& max_point)
        : minimum(min_point), maximum(max_point) {}

    bool hit(const Ray& ray, Interval ray_t) const {
        constexpr double epsilon = 1e-15;

        for (int axis = 0; axis < 3; ++axis) {
            double origin;
            double direction;
            double box_min;
            double box_max;

            if (axis == 0) {
                origin = ray.origin.x;
                direction = ray.direction.x;
                box_min = minimum.x;
                box_max = maximum.x;
            } else if (axis == 1) {
                origin = ray.origin.y;
                direction = ray.direction.y;
                box_min = minimum.y;
                box_max = maximum.y;
            } else {
                origin = ray.origin.z;
                direction = ray.direction.z;
                box_min = minimum.z;
                box_max = maximum.z;
            }

            // 射线平行于当前轴对应的包围盒平面
            if (std::abs(direction) < epsilon) {
                if (origin < box_min || origin > box_max) {
                    return false;
                }
                continue;
            }

            double t0 = (box_min - origin) / direction;
            double t1 = (box_max - origin) / direction;

            if (t0 > t1) {
                std::swap(t0, t1);
            }

            ray_t.min = std::max(ray_t.min, t0);
            ray_t.max = std::min(ray_t.max, t1);

            if (ray_t.max < ray_t.min) {
                return false;
            }
        }

        return true;
    }

    Vec3 centroid() const {
        return 0.5 * (minimum + maximum);
    }

    double surface_area() const {
        const Vec3 extent = maximum - minimum;

        return 2.0 * (
            extent.x * extent.y +
            extent.y * extent.z +
            extent.z * extent.x
        );
    }
};

inline AABB surrounding_box(const AABB& a, const AABB& b) {
    const Vec3 small(
        std::min(a.minimum.x, b.minimum.x),
        std::min(a.minimum.y, b.minimum.y),
        std::min(a.minimum.z, b.minimum.z)
    );

    const Vec3 big(
        std::max(a.maximum.x, b.maximum.x),
        std::max(a.maximum.y, b.maximum.y),
        std::max(a.maximum.z, b.maximum.z)
    );

    return AABB(small, big);
}
