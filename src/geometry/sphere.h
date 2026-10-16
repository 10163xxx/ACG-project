
#pragma once

#include <cmath>
#include <memory>
#include <stdexcept>

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
        const double c = dot(oc, oc) - radius_ * radius_;

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
        record.material = material_;

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
        const double phi = std::atan2(-p.z, p.x)
                        + PI;
        
        u = phi / (2.0 * PI);
        v = theta / PI;
    }
};
