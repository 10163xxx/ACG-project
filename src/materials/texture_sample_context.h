#pragma once

#include "materials/texture.h"
#include "geometry/hit_record.h"

inline TextureSampleContext make_texture_sample_context(
    const HitRecord& hit
) {
    TextureSampleContext context;

    context.dudx = hit.dudx;
    context.dvdx = hit.dvdx;
    context.dudy = hit.dudy;
    context.dvdy = hit.dvdy;

    context.has_uv_differentials =
        hit.has_uv_differentials;

    context.dpdx = hit.dpdx;
    context.dpdy = hit.dpdy;

    context.has_surface_differentials =
        hit.has_surface_differentials;

    context.cone_width = hit.ray_cone_width;
    context.has_ray_cone = hit.has_ray_cone;

    context.dpdu = hit.dpdu;
    context.dpdv = hit.dpdv;

    context.has_uv_parameterization =
        hit.has_uv_parameterization;

    return context;
}