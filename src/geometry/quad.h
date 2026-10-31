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

        set_tangent_frame(
            record,
            record.front_face
        );

        record.u = std::clamp(alpha, 0.0, 1.0);
        record.v = std::clamp(beta, 0.0, 1.0);

        record.dpdu = u_;
        record.dpdv = v_;
        record.has_uv_parameterization = true;

        record.has_surface_differentials = false;
        record.has_uv_differentials = false;

        if (ray.has_differentials) {
            const auto intersect_plane = [&](
                const Vec3& ray_origin,
                const Vec3& ray_direction,
                Vec3& differential_point
            ) {
                const double differential_denominator =
                    dot(normal_, ray_direction);

                if (!std::isfinite(differential_denominator) ||
                    std::abs(differential_denominator) < 1e-12) {
                    return false;
                }

                const double differential_t =
                    (plane_d_ - dot(normal_, ray_origin))
                    / differential_denominator;

                if (!std::isfinite(differential_t) ||
                    differential_t <= 0.0) {
                    return false;
                }

                differential_point =
                    ray_origin + differential_t * ray_direction;

                return std::isfinite(differential_point.x) &&
                       std::isfinite(differential_point.y) &&
                       std::isfinite(differential_point.z);
            };

            const auto get_uv = [&](
                const Vec3& differential_point,
                double& differential_u,
                double& differential_v
            ) {
                const Vec3 differential_relative =
                    differential_point - q_;

                const double relative_u =
                    dot(differential_relative, u_);

                const double relative_v =
                    dot(differential_relative, v_);

                differential_u =
                    (
                        relative_u * dot(v_, v_) -
                        relative_v * dot(u_, v_)
                    ) / coordinate_determinant_;

                differential_v =
                    (
                        relative_v * dot(u_, u_) -
                        relative_u * dot(u_, v_)
                    ) / coordinate_determinant_;

                return std::isfinite(differential_u) &&
                       std::isfinite(differential_v);
            };

            Vec3 point_x;
            Vec3 point_y;

            if (intersect_plane(
                    ray.rx_origin,
                    ray.rx_direction,
                    point_x) &&
                intersect_plane(
                    ray.ry_origin,
                    ray.ry_direction,
                    point_y)) {

                record.dpdx = point_x - record.point;
                record.dpdy = point_y - record.point;

                record.dndx = Vec3(0.0, 0.0, 0.0);
                record.dndy = Vec3(0.0, 0.0, 0.0);

                record.has_surface_differentials =
                    std::isfinite(record.dpdx.x) &&
                    std::isfinite(record.dpdx.y) &&
                    std::isfinite(record.dpdx.z) &&
                    std::isfinite(record.dpdy.x) &&
                    std::isfinite(record.dpdy.y) &&
                    std::isfinite(record.dpdy.z);

                double u_x = 0.0;
                double v_x = 0.0;
                double u_y = 0.0;
                double v_y = 0.0;

                if (record.has_surface_differentials &&
                    get_uv(point_x, u_x, v_x) &&
                    get_uv(point_y, u_y, v_y)) {

                    record.dudx = u_x - record.u;
                    record.dvdx = v_x - record.v;
                    record.dudy = u_y - record.u;
                    record.dvdy = v_y - record.v;

                    record.has_uv_differentials =
                        std::isfinite(record.dudx) &&
                        std::isfinite(record.dvdx) &&
                        std::isfinite(record.dudy) &&
                        std::isfinite(record.dvdy);
                }
            }
        }

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

        set_tangent_frame(sample, true);

        sample.dpdu = u_;
        sample.dpdv = v_;
        sample.has_uv_parameterization = true;

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

    void set_tangent_frame(
        HitRecord& record,
        bool front_face
    ) const {
        Vec3 tangent =
            u_ - dot(u_, normal_) * normal_;

        const double tangent_length_squared =
            tangent.length_squared();

        if (!std::isfinite(tangent_length_squared) ||
            tangent_length_squared <= 1e-20) {
            throw std::runtime_error(
                "Quad has an invalid tangent direction."
            );
        }

        tangent = tangent / std::sqrt(tangent_length_squared);

        const double handedness =
            dot(cross(normal_, tangent), v_) < 0.0
                ? -1.0
                : 1.0;

        Vec3 bitangent =
            handedness * cross(normal_, tangent);

        if (!front_face) {
            bitangent = -1.0 * bitangent;
        }

        record.tangent = tangent;
        record.bitangent = bitangent;
    }
};
