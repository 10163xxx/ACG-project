
#pragma once

#include <memory>

#include "core/vec3.h"
#include "core/ray.h"

class Material;

struct HitRecord {
    Vec3 point;
    Vec3 normal;
    double t = 0.0;
    bool front_face = true;

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
