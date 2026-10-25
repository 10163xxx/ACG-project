#pragma once

#include "core/vec3.h"

struct Ray {
    Vec3 origin;
    Vec3 direction;

    // Ray differentials approximate the rays through neighboring
    // screen positions. They are optional and may be absent after
    // a scattering event.
    Vec3 rx_origin;
    Vec3 ry_origin;
    Vec3 rx_direction;
    Vec3 ry_direction;

    bool has_differentials = false;

    // Approximate ray-cone footprint in world space.
    // cone_width is the radius at the ray origin.
    // cone_spread is the radius growth per unit world distance.
    double cone_width = 0.0;
    double cone_spread = 0.0;

    Ray() = default;

    Ray(
        const Vec3& origin_,
        const Vec3& direction_
    )
        : origin(origin_),
          direction(direction_) {}

    Ray(
        const Vec3& origin_,
        const Vec3& direction_,
        const Vec3& rx_origin_,
        const Vec3& ry_origin_,
        const Vec3& rx_direction_,
        const Vec3& ry_direction_
    )
        : origin(origin_),
          direction(direction_),
          rx_origin(rx_origin_),
          ry_origin(ry_origin_),
          rx_direction(rx_direction_),
          ry_direction(ry_direction_),
          has_differentials(true) {}

    Vec3 at(double t) const {
        return origin + t * direction;
    }
};
