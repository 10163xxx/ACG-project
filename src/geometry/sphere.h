#pragma once

#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>
#include <algorithm>

#include "geometry/hittable.h"

class Sphere : public Hittable {
public:
    Sphere(
        const Vec3& center,
        double radius,
        std::shared_ptr<const Material> material = nullptr
    )
        : center_(center),
          radius_(radius),
          material_(std::move(material)) {
        if (radius <= 0.0) {
            throw std::invalid_argument(
                "Sphere radius must be positive."
            );
        }
    }

    bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override {
        const Vec3 oc = ray.origin - center_;

        const double a = dot(ray.direction, ray.direction);
        if (a <= 0.0) {
            return false;
        }

        const double half_b = dot(oc, ray.direction);
        const double c =
            dot(oc, oc) - radius_ * radius_;

        const double discriminant =
            half_b * half_b - a * c;

        if (discriminant < 0.0) {
            return false;
        }

        const double sqrt_d = std::sqrt(discriminant);

        double root = (-half_b - sqrt_d) / a;

        if (!ray_t.surrounds(root)) {
            root = (-half_b + sqrt_d) / a;

            if (!ray_t.surrounds(root)) {
                return false;
            }
        }

        record.t = root;
        record.point = ray.at(root);

        const Vec3 outward_normal =
            (record.point - center_) / radius_;

        record.set_face_normal(ray, outward_normal);
        get_sphere_uv(outward_normal, record.u, record.v);

        // Compute the sphere's world-space UV derivatives.
        constexpr double PI = 3.14159265358979323846;

        const double theta =
            std::acos(std::clamp(
                -outward_normal.y,
                -1.0,
                1.0
            ));

        const double phi =
            std::atan2(-outward_normal.z, outward_normal.x);

        const double sin_theta = std::sin(theta);
        const double cos_theta = std::cos(theta);
        const double sin_phi = std::sin(phi);
        const double cos_phi = std::cos(phi);

        record.dpdu = Vec3(
            -2.0 * PI * radius_ * sin_theta * sin_phi,
            0.0,
            -2.0 * PI * radius_ * sin_theta * cos_phi
        );

        record.dpdv = Vec3(
            PI * radius_ * cos_theta * cos_phi,
            PI * radius_ * sin_theta,
            -PI * radius_ * cos_theta * sin_phi
        );

        const double dpdu_length_squared =
            record.dpdu.length_squared();

        const double dpdv_length_squared =
            record.dpdv.length_squared();

        // U is singular at the sphere poles.
        record.has_uv_parameterization =
            std::isfinite(dpdu_length_squared) &&
            std::isfinite(dpdv_length_squared) &&
            dpdu_length_squared > 1e-20 &&
            dpdv_length_squared > 1e-20;

        compute_uv_differentials(ray, record);

        record.material = material_;
        record.medium_boundary = this;

        return true;
    }

    // Uniformly sample the sphere surface using two values
    // in [0, 1). The resulting area PDF is 1 / (4 * pi * r^2).
    bool sample_surface(
        double u1,
        double u2,
        HitRecord& sample,
        double& pdf_area
    ) const override {
        constexpr double PI = 3.14159265358979323846;

        if (u1 < 0.0 || u1 >= 1.0 ||
            u2 < 0.0 || u2 >= 1.0) {
            return false;
        }

        const double y = 1.0 - 2.0 * u1;
        const double phi = 2.0 * PI * u2;
        const double radial =
            std::sqrt(std::max(0.0, 1.0 - y * y));

        const Vec3 outward_normal(
            radial * std::cos(phi),
            y,
            radial * std::sin(phi)
        );

        sample.point = center_ + radius_ * outward_normal;
        sample.normal = outward_normal;
        sample.front_face = true;
        sample.material = material_;

        pdf_area = 1.0 / (4.0 * PI * radius_ * radius_);

        return true;
    }

    bool is_closed_volume() const override {
        return true;
    }

    AABB bounding_box() const override {
        const Vec3 radius_vec(
            radius_,
            radius_,
            radius_
        );

        return AABB(
            center_ - radius_vec,
            center_ + radius_vec
        );
    }

private:
    Vec3 center_;
    double radius_;
    std::shared_ptr<const Material> material_;

    static void get_sphere_uv(
        const Vec3& p,
        double& u,
        double& v
    ) {
        constexpr double PI = 3.14159265358979323846;

        const double theta = std::acos(-p.y);
        const double phi =
            std::atan2(-p.z, p.x) + PI;

        u = phi / (2.0 * PI);
        v = theta / PI;
    }

    bool intersect_differential_ray(
        const Vec3& ray_origin,
        const Vec3& ray_direction,
        Vec3& point,
        Vec3& outward_normal
    ) const {
        const Vec3 oc = ray_origin - center_;

        const double a =
            dot(ray_direction, ray_direction);

        if (!std::isfinite(a) || a <= 1e-20) {
            return false;
        }

        const double half_b =
            dot(oc, ray_direction);

        const double c =
            dot(oc, oc) - radius_ * radius_;

        const double discriminant =
            half_b * half_b - a * c;

        if (!std::isfinite(discriminant) ||
            discriminant < 0.0) {
            return false;
        }

        const double sqrt_d =
            std::sqrt(discriminant);

        double t =
            (-half_b - sqrt_d) / a;

        if (!std::isfinite(t) || t <= 1e-8) {
            t = (-half_b + sqrt_d) / a;
        }

        if (!std::isfinite(t) || t <= 1e-8) {
            return false;
        }

        point = ray_origin + t * ray_direction;
        outward_normal = (point - center_) / radius_;

        return std::isfinite(point.x) &&
               std::isfinite(point.y) &&
               std::isfinite(point.z) &&
               std::isfinite(outward_normal.x) &&
               std::isfinite(outward_normal.y) &&
               std::isfinite(outward_normal.z);
    }

    void compute_uv_differentials(
        const Ray& ray,
        HitRecord& record
    ) const {
        record.has_surface_differentials = false;
        record.has_uv_differentials = false;

        if (!ray.has_differentials) {
            return;
        }

        Vec3 point_x;
        Vec3 point_y;
        Vec3 normal_x;
        Vec3 normal_y;

        if (!intersect_differential_ray(
                ray.rx_origin,
                ray.rx_direction,
                point_x,
                normal_x) ||
            !intersect_differential_ray(
                ray.ry_origin,
                ray.ry_direction,
                point_y,
                normal_y)) {
            return;
        }

        record.dpdx = point_x - record.point;
        record.dpdy = point_y - record.point;

        const Vec3 outward_normal =
            record.front_face
                ? record.normal
                : -1.0 * record.normal;

        record.dndx =
            normalize(normal_x) - outward_normal;

        record.dndy =
            normalize(normal_y) - outward_normal;

        record.has_surface_differentials =
            std::isfinite(record.dpdx.x) &&
            std::isfinite(record.dpdx.y) &&
            std::isfinite(record.dpdx.z) &&
            std::isfinite(record.dpdy.x) &&
            std::isfinite(record.dpdy.y) &&
            std::isfinite(record.dpdy.z) &&
            std::isfinite(record.dndx.x) &&
            std::isfinite(record.dndx.y) &&
            std::isfinite(record.dndx.z) &&
            std::isfinite(record.dndy.x) &&
            std::isfinite(record.dndy.y) &&
            std::isfinite(record.dndy.z);

        if (!record.has_surface_differentials) {
            return;
        }

        double u_x = 0.0;
        double v_x = 0.0;
        double u_y = 0.0;
        double v_y = 0.0;

        get_sphere_uv(normalize(normal_x), u_x, v_x);
        get_sphere_uv(normalize(normal_y), u_y, v_y);

        // Handle the wraparound at the spherical UV seam.
        double delta_u_x = u_x - record.u;
        double delta_u_y = u_y - record.u;

        if (delta_u_x > 0.5) {
            delta_u_x -= 1.0;
        } else if (delta_u_x < -0.5) {
            delta_u_x += 1.0;
        }

        if (delta_u_y > 0.5) {
            delta_u_y -= 1.0;
        } else if (delta_u_y < -0.5) {
            delta_u_y += 1.0;
        }

        record.dudx = delta_u_x;
        record.dvdx = v_x - record.v;
        record.dudy = delta_u_y;
        record.dvdy = v_y - record.v;

        record.has_uv_differentials =
            std::isfinite(record.dudx) &&
            std::isfinite(record.dvdx) &&
            std::isfinite(record.dudy) &&
            std::isfinite(record.dvdy);
    }
};
