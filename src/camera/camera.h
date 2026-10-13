
#pragma once

#include <cmath>
#include <stdexcept>

#include "core/ray.h"
#include "core/vec3.h"

class Camera {
public:
    Camera(
        const Vec3& look_from,
        const Vec3& look_at,
        const Vec3& vup,
        double vertical_fov_degrees,
        double aspect_ratio
    ) {
        if (aspect_ratio <= 0.0) {
            throw std::invalid_argument(
                "Camera aspect ratio must be positive."
            );
        }

        if (vertical_fov_degrees <= 0.0 ||
            vertical_fov_degrees >= 180.0) {
            throw std::invalid_argument(
                "Camera FOV must be between 0 and 180 degrees."
            );
        }

        // Camera coordinate system.
        const Vec3 view_direction = look_from - look_at;

        if (view_direction.near_zero()) {
            throw std::invalid_argument(
                "Camera position and target must differ."
            );
        }

        w_ = normalize(view_direction);
        u_ = normalize(cross(vup, w_));

        if (u_.near_zero()) {
            throw std::invalid_argument(
                "Camera up direction cannot be parallel to view direction."
            );
        }

        v_ = cross(w_, u_);

        const double pi = std::acos(-1.0);
        const double theta = vertical_fov_degrees * pi / 180.0;
        const double half_height = std::tan(theta / 2.0);
        const double half_width = aspect_ratio * half_height;

        origin_ = look_from;

        horizontal_ = 2.0 * half_width * u_;
        vertical_ = 2.0 * half_height * v_;

        lower_left_corner_ =
            origin_
            - half_width * u_
            - half_height * v_
            - w_;
    }
    Ray get_ray(double s, double t) const {
        const Vec3 target =
            lower_left_corner_
            + s * horizontal_
            + t * vertical_;

        return Ray(origin_, target - origin_);
    }

private:
    Vec3 origin_;
    Vec3 lower_left_corner_;
    Vec3 horizontal_;
    Vec3 vertical_;

    Vec3 u_;
    Vec3 v_;
    Vec3 w_;
};
