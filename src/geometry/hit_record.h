#pragma once

#include <memory>

#include "core/vec3.h"
#include "core/ray.h"

class Material;

struct HitRecord {
    Vec3 point;
    Vec3 normal;

    // Tangent-space basis derived from the hit triangle's UVs.
    Vec3 tangent;
    Vec3 bitangent;

    double t = 0.0;
    bool front_face = true;
    double u = 0.0;
    double v = 0.0;

    // UV derivatives with respect to screen-space pixel steps.
    double dudx = 0.0;
    double dvdx = 0.0;
    double dudy = 0.0;
    double dvdy = 0.0;

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
