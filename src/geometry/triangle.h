#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

#include "geometry/hittable.h"

class Triangle : public Hittable {
public:
    Triangle(
        const Vec3& a,
        const Vec3& b,
        const Vec3& c,
        std::shared_ptr<const Material> material = nullptr,
        const Vec3& uv_a = Vec3(0.0, 0.0, 0.0),
        const Vec3& uv_b = Vec3(1.0, 0.0, 0.0),
        const Vec3& uv_c = Vec3(0.0, 1.0, 0.0),
        bool has_uv = false
    )
        : a_(a),
          b_(b),
          c_(c),
          edge1_(b - a),
          edge2_(c - a),
          uv_a_(uv_a),
          uv_b_(uv_b),
          uv_c_(uv_c),
          has_uv_(has_uv),
          material_(std::move(material)) {
        const Vec3 normal = cross(edge1_, edge2_);
        const double normal_length = normal.length();

        if (normal_length <= 1e-12) {
            throw std::invalid_argument(
                "Triangle vertices must not be collinear."
            );
        }

        area_ = 0.5 * normal_length;
        normal_ = normal / normal_length;
    }

    bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override {
        constexpr double EPSILON = 1e-12;

        // Moller-Trumbore ray-triangle intersection.
        const Vec3 pvec = cross(ray.direction, edge2_);
        const double determinant = dot(edge1_, pvec);

        if (std::abs(determinant) < EPSILON) {
            return false;
        }

        const double inv_determinant = 1.0 / determinant;
        const Vec3 tvec = ray.origin - a_;

        const double beta = dot(tvec, pvec) * inv_determinant;

        if (beta < 0.0 || beta > 1.0) {
            return false;
        }

        const Vec3 qvec = cross(tvec, edge1_);
        const double gamma =
            dot(ray.direction, qvec) * inv_determinant;

        if (gamma < 0.0 || beta + gamma > 1.0) {
            return false;
        }

        const double t =
            dot(edge2_, qvec) * inv_determinant;

        if (!ray_t.surrounds(t)) {
            return false;
        }

        // Barycentric coordinates:
        // alpha + beta + gamma = 1.
        const double alpha = 1.0 - beta - gamma;

        record.t = t;
        record.point = ray.at(t);
        record.set_face_normal(ray, normal_);

        if (has_uv_) {
            record.u =
                alpha * uv_a_.x +
                beta * uv_b_.x +
                gamma * uv_c_.x;

            record.v =
                alpha * uv_a_.y +
                beta * uv_b_.y +
                gamma * uv_c_.y;
        } else {
            record.u = beta;
            record.v = gamma;
        }

        record.material = material_;

        (void)alpha;

        return true;
    }

    AABB bounding_box() const override {
        constexpr double PADDING = 1e-4;

        const Vec3 minimum(
            std::min({a_.x, b_.x, c_.x}) - PADDING,
            std::min({a_.y, b_.y, c_.y}) - PADDING,
            std::min({a_.z, b_.z, c_.z}) - PADDING
        );

        const Vec3 maximum(
            std::max({a_.x, b_.x, c_.x}) + PADDING,
            std::max({a_.y, b_.y, c_.y}) + PADDING,
            std::max({a_.z, b_.z, c_.z}) + PADDING
        );

        return AABB(minimum, maximum);
    }

    // Uniformly sample the triangle surface.
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

        const double sqrt_u = std::sqrt(u);

        const double alpha = 1.0 - sqrt_u;
        const double beta = sqrt_u * (1.0 - v);
        const double gamma = sqrt_u * v;

        sample.point =
            alpha * a_ +
            beta * b_ +
            gamma * c_;

        sample.normal = normal_;
        sample.front_face = true;
        
        if (has_uv_) {
            sample.u =
                alpha * uv_a_.x +
                beta * uv_b_.x +
                gamma * uv_c_.x;

            sample.v =
                alpha * uv_a_.y +
                beta * uv_b_.y +
                gamma * uv_c_.y;
        } else {
            sample.u = beta;
            sample.v = gamma;
        }

        sample.material = material_;

        pdf_area = 1.0 / area_;

        return true;
    }

    double area() const {
        return area_;
    }

private:
    Vec3 a_;
    Vec3 b_;
    Vec3 c_;
    
    Vec3 uv_a_;
    Vec3 uv_b_;
    Vec3 uv_c_;

    bool has_uv_ = false;

    Vec3 edge1_;
    Vec3 edge2_;

    Vec3 normal_;

    double area_ = 0.0;

    std::shared_ptr<const Material> material_;
};
