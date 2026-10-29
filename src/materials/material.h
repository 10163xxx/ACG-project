#pragma once

#include "core/ray.h"
#include "core/vec3.h"
#include "geometry/hit_record.h"

struct ScatterSampleInfo {
    // True when the ray passes straight through a surface
    // without sampling the underlying BSDF.
    bool is_straight_through = false;
};

class Material {
public:
    virtual ~Material() = default;

    // Emitted radiance from the surface.
    // Non-emissive materials return black by default.
    virtual Vec3 emitted(const HitRecord&) const {
        return Vec3(0.0, 0.0, 0.0);
    }

    virtual bool is_emissive() const {
        return false;
    }

    // Whether this surface blocks a shadow ray completely.
    // Ordinary materials are opaque by default.
    virtual bool blocks_shadow(const HitRecord& hit) const {
        (void)hit;
        return true;
    }

    // Fraction of shadow-ray energy transmitted through this surface.
    // Opaque materials return 1 here because blocking is handled
    // separately by blocks_shadow().
    virtual double shadow_transmittance(
        const HitRecord& hit,
        const Vec3& ray_direction
    ) const {
        (void)hit;
        (void)ray_direction;
        return 1.0;
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

    // Probability density of sampling incoming_direction
    // given outgoing_direction at the surface.
    // Both directions point away from the surface.
    // The PDF is measured per unit solid angle (sr^-1).
    virtual double scattering_pdf(
        const HitRecord&,
        const Vec3&,
        const Vec3&
    ) const {
        return 0.0;
    }
    
    // Returns the shading normal used for light transport.
    // By default, materials use the geometric surface normal.
    virtual Vec3 shading_normal(
        const Ray& incoming,
        const HitRecord& hit
    ) const {
        (void)incoming;
        return hit.normal;
    }

    virtual bool supports_direct_lighting() const {
        return false;
    }

    // Samples the next ray and reports whether it passes
    // straight through without sampling the underlying BSDF.
    virtual bool scatter(
        const Ray& incoming,
        const HitRecord& hit,
        Vec3& attenuation,
        Ray& scattered,
        ScatterSampleInfo& info
    ) const = 0;
};
