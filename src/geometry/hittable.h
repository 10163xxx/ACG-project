#pragma once

#include "core/ray.h"
#include "core/interval.h"
#include "geometry/hit_record.h"

class Hittable {
public:
    virtual ~Hittable() = default;

    virtual bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const = 0;
};