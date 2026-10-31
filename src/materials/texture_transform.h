#pragma once

#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

#include "materials/texture.h"

class TextureTransform final : public Texture {
public:
    TextureTransform(
        std::shared_ptr<const Texture> texture,
        double scale_u = 1.0,
        double scale_v = 1.0,
        double offset_u = 0.0,
        double offset_v = 0.0
    )
        : texture_(std::move(texture)),
          scale_u_(scale_u),
          scale_v_(scale_v),
          offset_u_(offset_u),
          offset_v_(offset_v) {
        if (!texture_) {
            throw std::invalid_argument(
                "TextureTransform requires a texture."
            );
        }

        if (!std::isfinite(scale_u_) ||
            !std::isfinite(scale_v_) ||
            !std::isfinite(offset_u_) ||
            !std::isfinite(offset_v_) ||
            std::abs(scale_u_) < 1e-12 ||
            std::abs(scale_v_) < 1e-12) {
            throw std::invalid_argument(
                "TextureTransform requires finite, non-zero scales "
                "and finite offsets."
            );
        }
    }

    Vec3 value(
        double u,
        double v,
        const Vec3& p,
        const TextureSampleContext& context
    ) const override {
        TextureSampleContext transformed_context = context;

        // Transform screen-space UV derivatives.
        transformed_context.dudx *= scale_u_;
        transformed_context.dvdx *= scale_v_;
        transformed_context.dudy *= scale_u_;
        transformed_context.dvdy *= scale_v_;

        // Transform the UV parameterization using the chain rule:
        //
        // u' = scale_u * u + offset_u
        // v' = scale_v * v + offset_v
        //
        // Therefore:
        // dP/du' = dP/du / scale_u
        // dP/dv' = dP/dv / scale_v
        transformed_context.dpdu =
            context.dpdu / scale_u_;

        transformed_context.dpdv =
            context.dpdv / scale_v_;

        return texture_->value(
            transform_u(u),
            transform_v(v),
            p,
            transformed_context
        );
    }

private:
    std::shared_ptr<const Texture> texture_;
    double scale_u_;
    double scale_v_;
    double offset_u_;
    double offset_v_;

    double transform_u(double u) const {
        return scale_u_ * u + offset_u_;
    }

    double transform_v(double v) const {
        return scale_v_ * v + offset_v_;
    }
};
