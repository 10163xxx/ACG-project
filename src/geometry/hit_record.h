#pragma once

#include <memory>

#include "core/vec3.h"
#include "core/ray.h"

class Material;

struct HitRecord {
    Vec3 point;
    Vec3 normal;

    // Shading normal is independent from the geometric surface normal.
    Vec3 shading_normal;
    bool has_shading_normal = false;

    Vec3 get_shading_normal() const {
        return has_shading_normal
            ? shading_normal
            : normal;
    }

    // Tangent-space basis derived from the hit triangle's UVs.
    Vec3 tangent;
    Vec3 bitangent;

    // World-space surface derivatives with respect to UV.
    Vec3 dpdu;
    Vec3 dpdv;

    // True when dpdu and dpdv form a valid UV parameterization.
    bool has_uv_parameterization = false;

    double t = 0.0;
    bool front_face = true;
    double u = 0.0;
    double v = 0.0;

    // Ray-cone radius evaluated at this hit point.
    double ray_cone_width = 0.0;
    bool has_ray_cone = false;

    // Surface-point derivatives with respect to screen-space pixel steps.
    Vec3 dpdx;
    Vec3 dpdy;

    // Surface-normal derivatives with respect to screen-space pixel steps.
    Vec3 dndx;
    Vec3 dndy;

    // UV derivatives with respect to screen-space pixel steps.
    double dudx = 0.0;
    double dvdx = 0.0;
    double dudy = 0.0;
    double dvdy = 0.0;

    // True when the incoming ray supplied valid surface differentials.
    bool has_surface_differentials = false;
    bool has_uv_differentials = false;

    // True only for a sampled event inside a participating medium.
    bool is_volume = false;

    std::shared_ptr<const Material> material;

    void set_face_normal(
        const Ray& ray,
        const Vec3& outward_normal
    ) {
        front_face = dot(ray.direction, outward_normal) < 0.0;

        normal = front_face
            ? outward_normal
            : -1.0 * outward_normal;
    }
};
