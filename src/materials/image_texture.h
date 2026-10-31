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

enum class TextureAddressMode {
    Clamp,
    Repeat
};

#include "materials/texture.h"

class ImageTexture final : public Texture {
public:

    explicit ImageTexture(
        const std::string& filename,
        bool use_mipmaps = true,
        bool decode_srgb = false,
        TextureAddressMode address_mode =
            TextureAddressMode::Clamp
    )
        : use_mipmaps_(use_mipmaps),
          address_mode_(address_mode) {
        load(filename);

        if (decode_srgb) {
            decode_srgb_pixels();
        }

        build_mipmaps();
    }

    Vec3 value(
        double u,
        double v,
        const Vec3&,
        const TextureSampleContext& context
    ) const override {
        if (!use_mipmaps_) {
            return sample(u, v, 0.0);
        }

        double width_u = 0.0;
        double width_v = 0.0;

        if (!context.get_uv_footprint(width_u, width_v)) {
            return sample(u, v, 0.0);
        }

        double footprint = 0.0;

        if (context.has_uv_differentials) {
            // Preserve the original screen-space derivative
            // footprint calculation.
            const double dx_u =
                context.dudx * static_cast<double>(width_);

            const double dx_v =
                context.dvdx * static_cast<double>(height_);

            const double dy_u =
                context.dudy * static_cast<double>(width_);

            const double dy_v =
                context.dvdy * static_cast<double>(height_);

            const double footprint_x =
                std::sqrt(dx_u * dx_u + dx_v * dx_v);

            const double footprint_y =
                std::sqrt(dy_u * dy_u + dy_v * dy_v);

            footprint =
                std::max(footprint_x, footprint_y);
        } else {
            // Ray-cone footprint: convert UV extents into texels.
            const double footprint_u =
                width_u * static_cast<double>(width_);

            const double footprint_v =
                width_v * static_cast<double>(height_);

            footprint =
                std::max(footprint_u, footprint_v);
        }

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

        if (address_mode_ == TextureAddressMode::Repeat) {
            u -= std::floor(u);
            v -= std::floor(v);
        } else {
            u = std::clamp(u, 0.0, 1.0);
            v = std::clamp(v, 0.0, 1.0);
        }

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
            bilinear_sample(
                mipmaps_[level0],
                u,
                v,
                address_mode_
            );

        if (level0 == level1) {
            return color0;
        }

        const Vec3 color1 =
            bilinear_sample(
                mipmaps_[level1],
                u,
                v,
                address_mode_
            );

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
    TextureAddressMode address_mode_ =
        TextureAddressMode::Clamp;

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
        double v,
        TextureAddressMode address_mode
    ) {
        if (address_mode == TextureAddressMode::Repeat) {
            u -= std::floor(u);
            v -= std::floor(v);
        } else {
            u = std::clamp(u, 0.0, 1.0);
            v = std::clamp(v, 0.0, 1.0);
        }

        const double x =
            address_mode == TextureAddressMode::Repeat
                ? u * static_cast<double>(level.width) - 0.5
                : u * static_cast<double>(level.width - 1);

        const double y =
            address_mode == TextureAddressMode::Repeat
                ? (1.0 - v) *
                    static_cast<double>(level.height) - 0.5
                : (1.0 - v) *
                    static_cast<double>(level.height - 1);

        const long long ix0 =
            static_cast<long long>(std::floor(x));

        const long long iy0 =
            static_cast<long long>(std::floor(y));

        const double tx =
            x - static_cast<double>(ix0);

        const double ty =
            y - static_cast<double>(iy0);

        auto resolve_index = [](
            long long index,
            std::size_t size,
            TextureAddressMode mode
        ) -> std::size_t {
            const long long extent =
                static_cast<long long>(size);

            if (mode == TextureAddressMode::Repeat) {
                const long long wrapped =
                    ((index % extent) + extent) % extent;

                return static_cast<std::size_t>(wrapped);
            }

            return static_cast<std::size_t>(
                std::clamp(
                    index,
                    0LL,
                    extent - 1
                )
            );
        };

        const std::size_t x0 =
            resolve_index(ix0, level.width, address_mode);

        const std::size_t x1 =
            resolve_index(ix0 + 1, level.width, address_mode);

        const std::size_t y0 =
            resolve_index(iy0, level.height, address_mode);

        const std::size_t y1 =
            resolve_index(iy0 + 1, level.height, address_mode);

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
