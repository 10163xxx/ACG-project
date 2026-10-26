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
        const Vec3& p
    ) const override {
        return texture_->value(
            transform_u(u),
            transform_v(v),
            p
        );
    }

    Vec3 value(
        double u,
        double v,
        const Vec3& p,
        double dudx,
        double dvdx,
        double dudy,
        double dvdy
    ) const override {
        return texture_->value(
            transform_u(u),
            transform_v(v),
            p,
            scale_u_ * dudx,
            scale_v_ * dvdx,
            scale_u_ * dudy,
            scale_v_ * dvdy
        );
    }

    Vec3 value_with_cone(
        double u,
        double v,
        const Vec3& p,
        double dudx,
        double dvdx,
        double dudy,
        double dvdy,
        bool has_uv_differentials,
        double cone_width,
        bool has_ray_cone,
        const Vec3& dpdu,
        const Vec3& dpdv,
        bool has_uv_parameterization
    ) const override {
        const Vec3 transformed_dpdu =
            dpdu / scale_u_;

        const Vec3 transformed_dpdv =
            dpdv / scale_v_;

        return texture_->value_with_cone(
            transform_u(u),
            transform_v(v),
            p,
            scale_u_ * dudx,
            scale_v_ * dvdx,
            scale_u_ * dudy,
            scale_v_ * dvdy,
            has_uv_differentials,
            cone_width,
            has_ray_cone,
            transformed_dpdu,
            transformed_dpdv,
            has_uv_parameterization
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
