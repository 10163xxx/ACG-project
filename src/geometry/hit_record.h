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
