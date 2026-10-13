
#pragma once

#include "core/ray.h"
#include "core/vec3.h"
#include "geometry/hit_record.h"

class Material {
public:
    virtual ~Material() = default;

    // Returns true if the incoming ray scatters.
    // attenuation stores the throughput color multiplier.
    // scattered stores the outgoing ray.
    virtual bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered
    ) const = 0;
};
