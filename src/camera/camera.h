#pragma once

#include <cmath>
#include <algorithm>
#include <stdexcept>

#include "core/ray.h"
#include "core/vec3.h"
#include "core/random.h"

class Camera {
public:
    Camera(
        const Vec3& look_from,
        const Vec3& look_at,
        const Vec3& vup,
        double vertical_fov_degrees,
        double aspect_ratio,
        double aperture_radius = 0.0,
        double focus_distance = 1.0
    )
        : aperture_radius_(aperture_radius) {

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

        if (aperture_radius < 0.0) {
            throw std::invalid_argument(
                "Camera aperture radius cannot be negative."
            );
        }

        if (focus_distance <= 0.0) {
            throw std::invalid_argument(
                "Camera focus distance must be positive."
            );
        }

        const Vec3 view_direction =
            look_from - look_at;

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
        const double theta =
            vertical_fov_degrees * pi / 180.0;

        const double half_height =
            std::tan(theta / 2.0);

        const double half_width =
            aspect_ratio * half_height;

        origin_ = look_from;

        // Scale the viewport to the focal plane.
        horizontal_ =
            2.0 * focus_distance * half_width * u_;

        vertical_ =
            2.0 * focus_distance * half_height * v_;

        lower_left_corner_ =
            origin_
            - focus_distance * half_width * u_
            - focus_distance * half_height * v_
            - focus_distance * w_;
    }

    // Original interface: preserves compatibility with existing callers.
    Ray get_ray(double s, double t) const {
        const Vec3 target =
            lower_left_corner_
            + s * horizontal_
            + t * vertical_;

        const Vec3 lens_sample =
            aperture_radius_ * random_in_unit_disk();

        const Vec3 lens_offset =
            lens_sample.x * u_
            + lens_sample.y * v_;

        const Vec3 ray_origin =
            origin_ + lens_offset;

        return Ray(
            ray_origin,
            target - ray_origin
        );
    }

    // Generate a primary ray and its screen-space differentials.
    // ds and dt are the normalized image-space pixel steps.
    Ray get_ray(
        double s,
        double t,
        double ds,
        double dt
    ) const {
        const Vec3 target =
            lower_left_corner_
            + s * horizontal_
            + t * vertical_;

        const Vec3 target_x =
            target + ds * horizontal_;

        const Vec3 target_y =
            target + dt * vertical_;

        // All three rays must use the same lens sample.
        const Vec3 lens_sample =
            aperture_radius_ * random_in_unit_disk();

        const Vec3 lens_offset =
            lens_sample.x * u_
            + lens_sample.y * v_;

        const Vec3 ray_origin =
            origin_ + lens_offset;

        Ray ray(
            ray_origin,
            target - ray_origin,
            ray_origin,
            ray_origin,
            target_x - ray_origin,
            target_y - ray_origin
        );

        const Vec3 unit_direction =
            normalize(ray.direction);

        const Vec3 unit_rx_direction =
            normalize(ray.rx_direction);

        const Vec3 unit_ry_direction =
            normalize(ray.ry_direction);

        const double spread_x =
            (unit_rx_direction - unit_direction).length();

        const double spread_y =
            (unit_ry_direction - unit_direction).length();

        // Approximate the pixel footprint radius using half
        // the larger angular difference to a neighboring ray.
        ray.cone_width = 0.0;
        ray.cone_spread =
            0.5 * std::max(spread_x, spread_y);

        return ray;
    }

private:
    static Vec3 random_in_unit_disk() {
        while (true) {
            const Vec3 point(
                Random::random_double(-1.0, 1.0),
                Random::random_double(-1.0, 1.0),
                0.0
            );

            if (point.length_squared() < 1.0) {
                return point;
            }
        }
    }

    Vec3 origin_;
    Vec3 lower_left_corner_;
    Vec3 horizontal_;
    Vec3 vertical_;

    Vec3 u_;
    Vec3 v_;
    Vec3 w_;

    double aperture_radius_;
};