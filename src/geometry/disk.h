#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

#include "geometry/hittable.h"

class Disk : public Hittable {
public:
    Disk(
        const Vec3& center,
        const Vec3& normal,
        double radius,
        std::shared_ptr<const Material> material = nullptr
    )
        : center_(center),
          radius_(radius),
          radius_squared_(radius * radius),
          material_(std::move(material)) {
        constexpr double EPSILON = 1e-12;

        const double normal_length = normal.length();

        if (normal_length <= EPSILON) {
            throw std::invalid_argument(
                "Disk normal must not be zero."
            );
        }

        if (!std::isfinite(radius_) || radius_ <= 0.0) {
            throw std::invalid_argument(
                "Disk radius must be positive and finite."
            );
        }

        normal_ = normal / normal_length;

        // Construct an orthonormal basis on the disk plane.
        const Vec3 helper =
            std::abs(normal_.x) > 0.9
                ? Vec3(0.0, 1.0, 0.0)
                : Vec3(1.0, 0.0, 0.0);

        tangent_ = normalize(cross(helper, normal_));
        bitangent_ = cross(normal_, tangent_);

        area_ = PI * radius_squared_;
    }

    bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override {
        constexpr double EPSILON = 1e-12;

        const double denominator =
            dot(normal_, ray.direction);

        // The ray is parallel to the disk plane.
        if (std::abs(denominator) < EPSILON) {
            return false;
        }

        const double t =
            dot(center_ - ray.origin, normal_) / denominator;

        if (!ray_t.surrounds(t)) {
            return false;
        }

        const Vec3 point = ray.at(t);
        const Vec3 relative = point - center_;

        if (relative.length_squared() > radius_squared_) {
            return false;
        }

        record.t = t;
        record.point = point;
        record.set_face_normal(ray, normal_);

        // Map the disk into the [0, 1] x [0, 1] UV range.
        record.u = 0.5 +
            dot(relative, tangent_) / (2.0 * radius_);

        record.v = 0.5 +
            dot(relative, bitangent_) / (2.0 * radius_);

        record.material = material_;

        return true;
    }

    AABB bounding_box() const override {
        constexpr double PADDING = 1e-4;

        // Projected radius along each world-space axis.
        const double extent_x =
            radius_ * std::sqrt(
                std::max(0.0, 1.0 - normal_.x * normal_.x)
            );

        const double extent_y =
            radius_ * std::sqrt(
                std::max(0.0, 1.0 - normal_.y * normal_.y)
            );

        const double extent_z =
            radius_ * std::sqrt(
                std::max(0.0, 1.0 - normal_.z * normal_.z)
            );

        const Vec3 minimum(
            center_.x - extent_x - PADDING,
            center_.y - extent_y - PADDING,
            center_.z - extent_z - PADDING
        );

        const Vec3 maximum(
            center_.x + extent_x + PADDING,
            center_.y + extent_y + PADDING,
            center_.z + extent_z + PADDING
        );

        return AABB(minimum, maximum);
    }

    // Uniformly sample the disk surface.
    // The returned area PDF is 1 / area.
    bool sample_surface(
        double u,
        double v,
        HitRecord& sample,
        double& pdf_area
    ) const override {
        pdf_area = 0.0;

        if (u < 0.0 || u >= 1.0 ||
            v < 0.0 || v >= 1.0) {
            return false;
        }

        // sqrt(u) ensures uniform sampling over disk area.
        const double r = radius_ * std::sqrt(u);
        const double phi = 2.0 * PI * v;

        const double x = r * std::cos(phi);
        const double y = r * std::sin(phi);

        sample.point =
            center_ +
            x * tangent_ +
            y * bitangent_;

        sample.normal = normal_;
        sample.front_face = true;

        sample.u = 0.5 + x / (2.0 * radius_);
        sample.v = 0.5 + y / (2.0 * radius_);

        sample.material = material_;

        pdf_area = 1.0 / area_;

        return true;
    }

    double area() const {
        return area_;
    }

private:
    static constexpr double PI =
        3.14159265358979323846;

    Vec3 center_;
    Vec3 normal_;
    Vec3 tangent_;
    Vec3 bitangent_;

    double radius_ = 0.0;
    double radius_squared_ = 0.0;
    double area_ = 0.0;

    std::shared_ptr<const Material> material_;
};
