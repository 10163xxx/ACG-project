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

        compute_tangent_frame();
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

        record.tangent = tangent_;
        record.bitangent = record.front_face
            ? bitangent_
            : -1.0 * bitangent_;

        record.material = material_;

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

        if (!std::isfinite(u) ||
            !std::isfinite(v) ||
            u < 0.0 || u >= 1.0 ||
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
        sample.tangent = tangent_;
        sample.bitangent = bitangent_;

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
    Vec3 tangent_;
    Vec3 bitangent_;

    double area_ = 0.0;

    std::shared_ptr<const Material> material_;

    static Vec3 normalize_or(
        const Vec3& value,
        const Vec3& fallback
    ) {
        const double length_squared = value.length_squared();

        if (!std::isfinite(length_squared) ||
            length_squared < 1e-20) {
            return fallback;
        }

        return value / std::sqrt(length_squared);
    }

    void compute_tangent_frame() {
        constexpr double UV_EPSILON = 1e-12;

        bool valid_uv_frame = false;
        Vec3 tangent;
        double handedness = 1.0;

        if (has_uv_) {
            const Vec3 duv1(
                uv_b_.x - uv_a_.x,
                uv_b_.y - uv_a_.y,
                0.0
            );

            const Vec3 duv2(
                uv_c_.x - uv_a_.x,
                uv_c_.y - uv_a_.y,
                0.0
            );

            const double determinant =
                duv1.x * duv2.y -
                duv1.y * duv2.x;

            if (std::isfinite(determinant) &&
                std::abs(determinant) > UV_EPSILON) {
                tangent =
                    (duv2.y * edge1_ -
                     duv1.y * edge2_) / determinant;

                // Preserve mirrored UV orientation.
                handedness = determinant < 0.0
                    ? -1.0
                    : 1.0;

                valid_uv_frame =
                    std::isfinite(tangent.length_squared()) &&
                    tangent.length_squared() > 1e-20;
            }
        }

        if (valid_uv_frame) {
            // Gram-Schmidt: make tangent orthogonal to the normal.
            tangent =
                tangent - dot(tangent, normal_) * normal_;

            valid_uv_frame =
                std::isfinite(tangent.length_squared()) &&
                tangent.length_squared() > 1e-20;
        }

        if (!valid_uv_frame) {
            // Stable fallback for missing or degenerate UVs.
            const Vec3 helper =
                std::abs(normal_.x) < 0.8
                    ? Vec3(1.0, 0.0, 0.0)
                    : Vec3(0.0, 1.0, 0.0);

            tangent = cross(helper, normal_);
            handedness = 1.0;
        }

        tangent_ = normalize_or(
            tangent,
            Vec3(1.0, 0.0, 0.0)
        );

        bitangent_ = handedness * normalize_or(
            cross(normal_, tangent_),
            Vec3(0.0, 0.0, 1.0)
        );
    }
};
