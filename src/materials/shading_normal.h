#pragma once

#include <cmath>

#include "core/vec3.h"

namespace shading_normal {

inline bool valid_reflection_directions(
    const Vec3& geometric_normal,
    const Vec3& shading_normal,
    const Vec3& outgoing_direction,
    const Vec3& incoming_direction
) {
    const double wo_geometric =
        dot(geometric_normal, outgoing_direction);

    const double wi_geometric =
        dot(geometric_normal, incoming_direction);

    const double wo_shading =
        dot(shading_normal, outgoing_direction);

    const double wi_shading =
        dot(shading_normal, incoming_direction);

    return
        std::isfinite(wo_geometric) &&
        std::isfinite(wi_geometric) &&
        std::isfinite(wo_shading) &&
        std::isfinite(wi_shading) &&
        wo_geometric > 0.0 &&
        wi_geometric > 0.0 &&
        wo_shading > 0.0 &&
        wi_shading > 0.0;
}

// Radiance-mode shading-normal correction.
// Apply this factor to the BSDF value, never to the PDF.
inline double radiance_correction(
    const Vec3& geometric_normal,
    const Vec3& shading_normal,
    const Vec3& outgoing_direction,
    const Vec3& incoming_direction
) {
    const double wi_shading =
        dot(incoming_direction, shading_normal);

    const double wo_geometric =
        dot(outgoing_direction, geometric_normal);

    const double wi_geometric =
        dot(incoming_direction, geometric_normal);

    const double wo_shading =
        dot(outgoing_direction, shading_normal);

    constexpr double EPSILON = 1e-8;

    const double denominator =
        wi_geometric * wo_shading;

    if (!std::isfinite(denominator) ||
        std::abs(denominator) < EPSILON) {
        return 0.0;
    }

    const double correction =
        std::abs(
            (wi_shading * wo_geometric) /
            denominator
        );

    return std::isfinite(correction)
        ? correction
        : 0.0;
}

} // namespace shading_normal
