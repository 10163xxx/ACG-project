#pragma once

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "materials/texture.h"

class ImageTexture final : public Texture {
public:

    explicit ImageTexture(
        const std::string& filename,
        bool use_mipmaps = true,
        bool decode_srgb = false
    )
        : use_mipmaps_(use_mipmaps) {
        load(filename);

        if (decode_srgb) {
            decode_srgb_pixels();
        }

        build_mipmaps();
    }


    // Default sampling uses bilinear filtering at full resolution.
    // The existing Texture interface does not provide UV derivatives,
    // so automatic screen-space LOD selection is not possible here.
    Vec3 value(
        double u,
        double v,
        const Vec3&
    ) const override {
        return sample(u, v, 0.0);
    }

    // Automatically select the mip level from screen-space UV derivatives.
    Vec3 value(
        double u,
        double v,
        const Vec3& p,
        double dudx,
        double dvdx,
        double dudy,
        double dvdy
    ) const override {
        (void)p;

        if (!use_mipmaps_) {
            return sample(u, v, 0.0);
        }

        if (!std::isfinite(dudx) ||
            !std::isfinite(dvdx) ||
            !std::isfinite(dudy) ||
            !std::isfinite(dvdy)) {
            return sample(u, v, 0.0);
        }

        // Convert UV derivatives into texture-space pixel distances.
        const double dx_u =
            dudx * static_cast<double>(width_);

        const double dx_v =
            dvdx * static_cast<double>(height_);

        const double dy_u =
            dudy * static_cast<double>(width_);

        const double dy_v =
            dvdy * static_cast<double>(height_);

        const double footprint_x =
            std::sqrt(dx_u * dx_u + dx_v * dx_v);

        const double footprint_y =
            std::sqrt(dy_u * dy_u + dy_v * dy_v);

        const double footprint =
            std::max(footprint_x, footprint_y);

        if (!std::isfinite(footprint)) {
            return sample(u, v, 0.0);
        }

        const double lod =
            std::max(0.0, std::log2(
                std::max(footprint, 1e-8)
            ));

        return sample(u, v, lod);
    }

    // Use the ray-cone footprint when screen-space UV
    // differentials are unavailable.
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
        // Exact screen-space derivatives take priority.
        if (has_uv_differentials) {
            return value(
                u, v, p,
                dudx, dvdx, dudy, dvdy
            );
        }

        if (!use_mipmaps_ ||
            !has_ray_cone ||
            !has_uv_parameterization ||
            !std::isfinite(cone_width) ||
            cone_width < 0.0) {
            return sample(u, v, 0.0);
        }

        // Build the first fundamental form of the UV mapping.
        const double g00 = dot(dpdu, dpdu);
        const double g01 = dot(dpdu, dpdv);
        const double g11 = dot(dpdv, dpdv);

        const double determinant =
            g00 * g11 - g01 * g01;

        if (!std::isfinite(g00) ||
            !std::isfinite(g01) ||
            !std::isfinite(g11) ||
            !std::isfinite(determinant) ||
            g00 <= 0.0 ||
            g11 <= 0.0 ||
            determinant <= 1e-20) {
            return sample(u, v, 0.0);
        }

        // Convert a world-space radius into the maximum local
        // UV displacement, then convert UV units into texels.
        const double uv_u_per_world =
            std::sqrt(g11 / determinant);

        const double uv_v_per_world =
            std::sqrt(g00 / determinant);

        const double footprint_u =
            cone_width *
            uv_u_per_world *
            static_cast<double>(width_);

        const double footprint_v =
            cone_width *
            uv_v_per_world *
            static_cast<double>(height_);

        const double footprint =
            std::max(footprint_u, footprint_v);

        if (!std::isfinite(footprint)) {
            return sample(u, v, 0.0);
        }

        const double lod =
            std::max(
                0.0,
                std::log2(std::max(footprint, 1e-8))
            );

        return sample(u, v, lod);
    }

    // Explicit-LOD sampling:
    // bilinear filtering within each mip level, then trilinear
    // interpolation between adjacent levels.
    Vec3 sample(
        double u,
        double v,
        double lod
    ) const {
        if (mipmaps_.empty() ||
            !std::isfinite(u) ||
            !std::isfinite(v) ||
            !std::isfinite(lod)) {
            return Vec3(1.0, 0.0, 1.0);
        }

        u = std::clamp(u, 0.0, 1.0);
        v = std::clamp(v, 0.0, 1.0);

        const double max_lod =
            static_cast<double>(mipmaps_.size() - 1);

        lod = std::clamp(lod, 0.0, max_lod);

        const std::size_t level0 =
            static_cast<std::size_t>(std::floor(lod));

        const std::size_t level1 =
            std::min(level0 + 1, mipmaps_.size() - 1);

        const double blend =
            lod - static_cast<double>(level0);

        const Vec3 color0 =
            bilinear_sample(mipmaps_[level0], u, v);

        if (level0 == level1) {
            return color0;
        }

        const Vec3 color1 =
            bilinear_sample(mipmaps_[level1], u, v);

        return (1.0 - blend) * color0 + blend * color1;
    }

    std::size_t width() const {
        return width_;
    }

    std::size_t height() const {
        return height_;
    }

    std::size_t mip_level_count() const {
        return mipmaps_.size();
    }

private:
    struct MipLevel {
        std::size_t width = 0;
        std::size_t height = 0;
        std::vector<Vec3> pixels;
    };

    std::size_t width_ = 0;
    std::size_t height_ = 0;
    bool use_mipmaps_ = true;

    // Original pixels are retained for compatibility with existing code.
    std::vector<Vec3> pixels_;

    // Level 0 is the original image; subsequent levels are progressively
    // smaller box-filtered images.
    std::vector<MipLevel> mipmaps_;

    static std::string read_token(std::istream& input) {
        std::string token;
        char ch = '\0';

        while (input.get(ch)) {
            if (std::isspace(
                    static_cast<unsigned char>(ch))) {
                continue;
            }

            if (ch == '#') {
                input.ignore(
                    std::numeric_limits<std::streamsize>::max(),
                    '\n'
                );
                continue;
            }

            token.push_back(ch);
            break;
        }

        while (input.get(ch)) {
            if (std::isspace(
                    static_cast<unsigned char>(ch))) {
                break;
            }

            if (ch == '#') {
                input.ignore(
                    std::numeric_limits<std::streamsize>::max(),
                    '\n'
                );
                break;
            }

            token.push_back(ch);
        }

        if (token.empty()) {
            throw std::runtime_error(
                "Unexpected end of PPM header."
            );
        }

        return token;
    }

    static std::size_t parse_positive_size(
        const std::string& token,
        const char* field_name
    ) {
        std::size_t consumed = 0;
        const unsigned long long value =
            std::stoull(token, &consumed);

        if (consumed != token.size() ||
            value == 0 ||
            value > std::numeric_limits<std::size_t>::max()) {
            throw std::runtime_error(
                std::string("Invalid PPM ") + field_name + "."
            );
        }

        return static_cast<std::size_t>(value);
    }

    static Vec3 bilinear_sample(
        const MipLevel& level,
        double u,
        double v
    ) {
        const double x =
            u * static_cast<double>(level.width - 1);

        // Image rows run from top to bottom; OBJ v runs bottom to top.
        const double y =
            (1.0 - v) * static_cast<double>(level.height - 1);

        const std::size_t x0 =
            static_cast<std::size_t>(std::floor(x));

        const std::size_t y0 =
            static_cast<std::size_t>(std::floor(y));

        const std::size_t x1 =
            std::min(x0 + 1, level.width - 1);

        const std::size_t y1 =
            std::min(y0 + 1, level.height - 1);

        const double tx = x - static_cast<double>(x0);
        const double ty = y - static_cast<double>(y0);

        const Vec3& c00 =
            level.pixels[y0 * level.width + x0];

        const Vec3& c10 =
            level.pixels[y0 * level.width + x1];

        const Vec3& c01 =
            level.pixels[y1 * level.width + x0];

        const Vec3& c11 =
            level.pixels[y1 * level.width + x1];

        const Vec3 top =
            (1.0 - tx) * c00 + tx * c10;

        const Vec3 bottom =
            (1.0 - tx) * c01 + tx * c11;

        return (1.0 - ty) * top + ty * bottom;
    }

    void build_mipmaps() {
        mipmaps_.clear();

        if (pixels_.empty() || width_ == 0 || height_ == 0) {
            return;
        }

        MipLevel base;
        base.width = width_;
        base.height = height_;
        base.pixels = pixels_;

        mipmaps_.push_back(std::move(base));

        while (mipmaps_.back().width > 1 ||
               mipmaps_.back().height > 1) {
            const MipLevel& source = mipmaps_.back();

            MipLevel next;
            next.width = std::max<std::size_t>(
                1,
                (source.width + 1) / 2
            );

            next.height = std::max<std::size_t>(
                1,
                (source.height + 1) / 2
            );

            next.pixels.reserve(next.width * next.height);

            for (std::size_t y = 0; y < next.height; ++y) {
                for (std::size_t x = 0; x < next.width; ++x) {
                    Vec3 sum(0.0, 0.0, 0.0);
                    std::size_t count = 0;

                    for (std::size_t dy = 0; dy < 2; ++dy) {
                        for (std::size_t dx = 0; dx < 2; ++dx) {
                            const std::size_t sx = 2 * x + dx;
                            const std::size_t sy = 2 * y + dy;

                            if (sx >= source.width ||
                                sy >= source.height) {
                                continue;
                            }

                            sum = sum + source.pixels[
                                sy * source.width + sx
                            ];

                            ++count;
                        }
                    }

                    next.pixels.push_back(
                        sum / static_cast<double>(count)
                    );
                }
            }

            mipmaps_.push_back(std::move(next));
        }
    }

    void load(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);

        if (!file) {
            throw std::runtime_error(
                "Failed to open image texture: " + filename
            );
        }

        const std::string magic = read_token(file);

        if (magic != "P3" && magic != "P6") {
            throw std::runtime_error(
                "Unsupported image format for '" + filename +
                "'. Only PPM P3 and P6 are supported."
            );
        }

        width_ = parse_positive_size(
            read_token(file),
            "width"
        );

        height_ = parse_positive_size(
            read_token(file),
            "height"
        );

        const std::size_t max_value =
            parse_positive_size(
                read_token(file),
                "maximum channel value"
            );

        if (max_value > 65535) {
            throw std::runtime_error(
                "PPM channel maximum cannot exceed 65535."
            );
        }

        if (width_ >
            std::numeric_limits<std::size_t>::max() / height_) {
            throw std::runtime_error(
                "PPM image dimensions overflow."
            );
        }

        const std::size_t pixel_count = width_ * height_;

        if (pixel_count >
            std::numeric_limits<std::size_t>::max() /
                sizeof(Vec3)) {
            throw std::runtime_error(
                "PPM image is too large."
            );
        }

        pixels_.clear();
        pixels_.reserve(pixel_count);

        auto read_channel = [&]() -> unsigned int {
            if (magic == "P3") {
                const std::string token = read_token(file);
                std::size_t consumed = 0;

                const unsigned long value =
                    std::stoul(token, &consumed);

                if (consumed != token.size() ||
                    value > max_value) {
                    throw std::runtime_error(
                        "Invalid P3 pixel channel."
                    );
                }

                return static_cast<unsigned int>(value);
            }

            if (max_value <= 255) {
                char byte = '\0';

                if (!file.get(byte)) {
                    throw std::runtime_error(
                        "Unexpected end of P6 pixel data."
                    );
                }

                const unsigned int value =
                    static_cast<unsigned char>(byte);

                if (value > max_value) {
                    throw std::runtime_error(
                        "P6 pixel channel exceeds maximum value."
                    );
                }

                return value;
            }

            char high = '\0';
            char low = '\0';

            if (!file.get(high) || !file.get(low)) {
                throw std::runtime_error(
                    "Unexpected end of 16-bit P6 pixel data."
                );
            }

            const unsigned int value =
                (static_cast<unsigned int>(
                    static_cast<unsigned char>(high)
                ) << 8) |
                static_cast<unsigned int>(
                    static_cast<unsigned char>(low)
                );

            if (value > max_value) {
                throw std::runtime_error(
                    "P6 pixel channel exceeds maximum value."
                );
            }

            return value;
        };

        for (std::size_t i = 0; i < pixel_count; ++i) {
            const double r =
                static_cast<double>(read_channel()) / max_value;

            const double g =
                static_cast<double>(read_channel()) / max_value;

            const double b =
                static_cast<double>(read_channel()) / max_value;

            pixels_.emplace_back(r, g, b);
        }
    }

    static double srgb_to_linear(double value) {
        value = std::clamp(value, 0.0, 1.0);

        if (value <= 0.04045) {
            return value / 12.92;
        }

        return std::pow(
            (value + 0.055) / 1.055,
            2.4
        );
    }

    void decode_srgb_pixels() {
        for (Vec3& pixel : pixels_) {
            pixel.x = srgb_to_linear(pixel.x);
            pixel.y = srgb_to_linear(pixel.y);
            pixel.z = srgb_to_linear(pixel.z);
        }
    }
};
