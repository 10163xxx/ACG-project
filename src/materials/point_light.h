#pragma once

#include <cmath>
#include <stdexcept>

#include "core/vec3.h"

struct PointLight {
    Vec3 position;
    Vec3 intensity;

    PointLight(
        const Vec3& position_,
        const Vec3& intensity_
    )
        : position(position_),
          intensity(intensity_) {
        if (!std::isfinite(position.x) ||
            !std::isfinite(position.y) ||
            !std::isfinite(position.z)) {
            throw std::invalid_argument(
                "Point light position must be finite."
            );
        }

        if (!std::isfinite(intensity.x) ||
            !std::isfinite(intensity.y) ||
            !std::isfinite(intensity.z) ||
            intensity.x < 0.0 ||
            intensity.y < 0.0 ||
            intensity.z < 0.0) {
            throw std::invalid_argument(
                "Point light intensity must be finite and non-negative."
            );
        }
    }
};
