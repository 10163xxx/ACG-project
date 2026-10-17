#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

#include "geometry/hittable.h"

class Quad : public Hittable {
public:
    // The quad is defined by one corner q and two edge vectors u and v.
    // Its four corners are q, q + u, q + v, and q + u + v.
    Quad(
        const Vec3& q,
        const Vec3& u,
        const Vec3& v,
        std::shared_ptr<const Material> material = nullptr
    )
        : q_(q),
          u_(u),
          v_(v),
          material_(std::move(material)) {
        const Vec3 normal = cross(u_, v_);
        const double normal_length = normal.length();

        if (normal_length <= 1e-12) {
            throw std::invalid_argument(
                "Quad edge vectors must not be parallel."
            );
        }

        area_ = normal_length;
        normal_ = normal / normal_length;

        // The plane equation is dot(normal_, p) = plane_d_.
        plane_d_ = dot(normal_, q_);

        // Used to map a point on the plane to its (alpha, beta)
        // coordinates in q + alpha * u + beta * v.
        const double uu = dot(u_, u_);
        const double uv = dot(u_, v_);
        const double vv = dot(v_, v_);

        coordinate_determinant_ = uu * vv - uv * uv;

        if (coordinate_determinant_ <= 1e-24) {
            throw std::invalid_argument(
                "Quad edge vectors are numerically degenerate."
            );
        }
    }

    bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override {
        constexpr double EPSILON = 1e-12;

        const double denominator = dot(normal_, ray.direction);

        if (std::abs(denominator) < EPSILON) {
            return false;
        }

        const double t =
            (plane_d_ - dot(normal_, ray.origin)) / denominator;

        if (!ray_t.surrounds(t)) {
            return false;
        }

        const Vec3 point = ray.at(t);
        const Vec3 relative = point - q_;

        const double relative_u = dot(relative, u_);
        const double relative_v = dot(relative, v_);

        const double alpha =
            (relative_u * dot(v_, v_) -
             relative_v * dot(u_, v_)) /
            coordinate_determinant_;

        const double beta =
            (relative_v * dot(u_, u_) -
             relative_u * dot(u_, v_)) /
            coordinate_determinant_;

        constexpr double EDGE_EPSILON = 1e-10;

        if (alpha < -EDGE_EPSILON ||
            alpha > 1.0 + EDGE_EPSILON ||
            beta < -EDGE_EPSILON ||
            beta > 1.0 + EDGE_EPSILON) {
            return false;
        }

        record.t = t;
        record.point = point;
        record.set_face_normal(ray, normal_);

        record.u = std::clamp(alpha, 0.0, 1.0);
        record.v = std::clamp(beta, 0.0, 1.0);

        record.material = material_;

        return true;
    }

    AABB bounding_box() const override {
        constexpr double PADDING = 1e-4;

        const Vec3 p0 = q_;
        const Vec3 p1 = q_ + u_;
        const Vec3 p2 = q_ + v_;
        const Vec3 p3 = q_ + u_ + v_;

        const Vec3 minimum(
            std::min({p0.x, p1.x, p2.x, p3.x}) - PADDING,
            std::min({p0.y, p1.y, p2.y, p3.y}) - PADDING,
            std::min({p0.z, p1.z, p2.z, p3.z}) - PADDING
        );

        const Vec3 maximum(
            std::max({p0.x, p1.x, p2.x, p3.x}) + PADDING,
            std::max({p0.y, p1.y, p2.y, p3.y}) + PADDING,
            std::max({p0.z, p1.z, p2.z, p3.z}) + PADDING
        );

        return AABB(minimum, maximum);
    }

    // Uniformly sample the quad surface.
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

        sample.point = q_ + u * u_ + v * v_;
        sample.normal = normal_;
        sample.front_face = true;
        sample.u = u;
        sample.v = v;
        sample.material = material_;

        pdf_area = 1.0 / area_;

        return true;
    }

    double area() const {
        return area_;
    }

private:
    Vec3 q_;
    Vec3 u_;
    Vec3 v_;

    Vec3 normal_;

    double plane_d_ = 0.0;
    double area_ = 0.0;
    double coordinate_determinant_ = 0.0;

    std::shared_ptr<const Material> material_;
};
