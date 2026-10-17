#pragma once

#include "core/ray.h"
#include "core/vec3.h"
#include "geometry/hit_record.h"

class Material {
public:
    virtual ~Material() = default;

    // Emitted radiance from the surface.
    // Non-emissive materials return black by default.
    virtual Vec3 emitted(const HitRecord&) const {
        return Vec3(0.0, 0.0, 0.0);
    }

    // Evaluate the BSDF for directions pointing away from the surface.
    // Non-BSDF materials return black by default.
    virtual Vec3 evaluate_bsdf(
        const HitRecord&,
        const Vec3&,
        const Vec3&
    ) const {
        return Vec3(0.0, 0.0, 0.0);
    }

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
