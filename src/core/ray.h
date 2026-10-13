
#pragma once

#include "core/vec3.h"

struct Ray {
    Vec3 origin;
    Vec3 direction;

    Ray() = default;

    Ray(const Vec3& origin_, const Vec3& direction_)
        : origin(origin_), direction(direction_) {}

    Vec3 at(double t) const {
        return origin + t * direction;
    }
};
