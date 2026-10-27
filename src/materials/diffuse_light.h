#pragma once

#include <stdexcept>

#include "materials/material.h"

class DiffuseLight final : public Material {
public:
    explicit DiffuseLight(const Vec3& intensity)
        : intensity_(intensity) {
        if (intensity.x < 0.0 ||
            intensity.y < 0.0 ||
            intensity.z < 0.0) {
            throw std::invalid_argument(
                "Light intensity must be non-negative."
            );
        }
    }

    Vec3 emitted(const HitRecord& hit) const override {
        if (!hit.front_face) {
            return Vec3(0.0, 0.0, 0.0);
        }

        return intensity_;
    }

    bool is_emissive() const override {
        return true;
    }

    bool scatter(
        const Ray&,
        const HitRecord&,
        Vec3&,
        Ray&,
        ScatterSampleInfo& info
    ) const override {
        info.is_straight_through = false;
        return false;
    }

private:
    Vec3 intensity_;
};
