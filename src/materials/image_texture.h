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

        // Anisotropic filtering requires screen-space UV derivatives.
        // Ray-cone footprints do not provide an orientation, so they
        // continue to use isotropic filtering.
        if (context.has_valid_uv_differentials()) {
            const double dx_u =
                context.dudx * static_cast<double>(width_);

            const double dx_v =
                context.dvdx * static_cast<double>(height_);

            const double dy_u =
                context.dudy * static_cast<double>(width_);

            const double dy_v =
                context.dvdy * static_cast<double>(height_);

            if (!std::isfinite(dx_u) ||
                !std::isfinite(dx_v) ||
                !std::isfinite(dy_u) ||
                !std::isfinite(dy_v)) {
                return sample(u, v, 0.0);
            }

            // Construct the covariance matrix of the texture-space
            // screen footprint:
            //
            // C = J * transpose(J)
            //
            // Its eigenvectors give the footprint directions, and
            // its eigenvalues give the squared principal-axis lengths.
            const double c00 =
                dx_u * dx_u + dy_u * dy_u;

            const double c01 =
                dx_u * dx_v + dy_u * dy_v;

            const double c11 =
                dx_v * dx_v + dy_v * dy_v;

            const double discriminant =
                std::hypot(
                    c00 - c11,
                    2.0 * c01
                );

            const double major_squared =
                0.5 * (
                    c00 + c11 + discriminant
                );

            if (!std::isfinite(major_squared) ||
                major_squared < 0.0) {
                return sample(u, v, 0.0);
            }

            const double major_length =
                std::sqrt(major_squared);

            if (major_length <= 1e-8) {
                return sample(u, v, 0.0);
            }

            // The determinant of J * transpose(J) is det(J)^2.
            // This form is more stable than subtracting two nearly
            // equal eigenvalues for highly anisotropic footprints.
            const double jacobian_determinant =
                dx_u * dy_v - dx_v * dy_u;

            const double covariance_determinant =
                jacobian_determinant *
                jacobian_determinant;

            const double minor_squared =
                major_squared > 1e-20
                    ? covariance_determinant / major_squared
                    : 0.0;

            const double minor_length =
                std::sqrt(std::max(0.0, minor_squared));

            if (!std::isfinite(minor_length)) {
                return sample(u, v, 0.0);
            }

            constexpr double MAX_ANISOTROPY = 8.0;
            constexpr std::size_t MAX_ANISOTROPIC_SAMPLES = 8;

            // Clamp the anisotropy ratio. Beyond this limit, the
            // remaining footprint is handled by a coarser mip level.
            const double filtered_minor_length =
                std::max(
                    minor_length,
                    major_length / MAX_ANISOTROPY
                );

            const double anisotropy =
                major_length /
                std::max(filtered_minor_length, 1e-8);

            const std::size_t sample_count =
                std::clamp(
                    static_cast<std::size_t>(
                        std::ceil(anisotropy)
                    ),
                    static_cast<std::size_t>(1),
                    MAX_ANISOTROPIC_SAMPLES
                );

            // The minor axis determines the mip level.
            const double lod =
                std::max(
                    0.0,
                    std::log2(
                        std::max(
                            filtered_minor_length,
                            1e-8
                        )
                    )
                );

            // Eigenvector corresponding to the major eigenvalue.
            const double angle =
                0.5 * std::atan2(
                    2.0 * c01,
                    c00 - c11
                );

            const double axis_u = std::cos(angle);
            const double axis_v = std::sin(angle);

            Vec3 accumulated_color(0.0, 0.0, 0.0);

            for (std::size_t i = 0; i < sample_count; ++i) {
                // Uniformly distribute samples over the major axis.
                const double offset =
                    (
                        (static_cast<double>(i) + 0.5) /
                        static_cast<double>(sample_count)
                        - 0.5
                    ) * major_length;

                const double sample_u =
                    u + axis_u * offset /
                        static_cast<double>(width_);

                const double sample_v =
                    v + axis_v * offset /
                        static_cast<double>(height_);

                accumulated_color =
                    accumulated_color +
                    sample(
                        sample_u,
                        sample_v,
                        lod
                    );
            }

            return accumulated_color /
                static_cast<double>(sample_count);
        }

        // Ray-cone footprint: no screen-space orientation is
        // available, so retain isotropic trilinear filtering.
        double width_u = 0.0;
        double width_v = 0.0;

        if (!context.get_uv_footprint(width_u, width_v)) {
            return sample(u, v, 0.0);
        }

        const double footprint_u =
            width_u * static_cast<double>(width_);

        const double footprint_v =
            width_v * static_cast<double>(height_);

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
                if (ch == '\r' && input.peek() == '\n') {
                    input.get(ch);
                }

                break;
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

            const double scale_x =
                static_cast<double>(source.width) /
                static_cast<double>(next.width);

            const double scale_y =
                static_cast<double>(source.height) /
                static_cast<double>(next.height);

            for (std::size_t y = 0; y < next.height; ++y) {
                for (std::size_t x = 0; x < next.width; ++x) {
                    // The destination texel covers a continuous
                    // rectangular footprint in source pixel space.
                    const double x_begin =
                        static_cast<double>(x) * scale_x;

                    const double x_end =
                        static_cast<double>(x + 1) * scale_x;

                    const double y_begin =
                        static_cast<double>(y) * scale_y;

                    const double y_end =
                        static_cast<double>(y + 1) * scale_y;

                    const std::size_t source_x_begin =
                        static_cast<std::size_t>(
                            std::floor(x_begin)
                        );

                    const std::size_t source_x_end =
                        std::min(
                            source.width,
                            static_cast<std::size_t>(
                                std::ceil(x_end)
                            )
                        );

                    const std::size_t source_y_begin =
                        static_cast<std::size_t>(
                            std::floor(y_begin)
                        );

                    const std::size_t source_y_end =
                        std::min(
                            source.height,
                            static_cast<std::size_t>(
                                std::ceil(y_end)
                            )
                        );

                    Vec3 weighted_sum(0.0, 0.0, 0.0);
                    double total_weight = 0.0;

                    for (std::size_t sy = source_y_begin;
                         sy < source_y_end;
                         ++sy) {
                        const double overlap_y =
                            std::max(
                                0.0,
                                std::min(
                                    y_end,
                                    static_cast<double>(sy + 1)
                                ) -
                                std::max(
                                    y_begin,
                                    static_cast<double>(sy)
                                )
                            );

                        if (overlap_y <= 0.0) {
                            continue;
                        }

                        for (std::size_t sx = source_x_begin;
                             sx < source_x_end;
                             ++sx) {
                            const double overlap_x =
                                std::max(
                                    0.0,
                                    std::min(
                                        x_end,
                                        static_cast<double>(sx + 1)
                                    ) -
                                    std::max(
                                        x_begin,
                                        static_cast<double>(sx)
                                    )
                                );

                            const double weight =
                                overlap_x * overlap_y;

                            if (weight <= 0.0) {
                                continue;
                            }

                            weighted_sum =
                                weighted_sum +
                                weight * source.pixels[
                                    sy * source.width + sx
                                ];

                            total_weight += weight;
                        }
                    }

                    if (total_weight <= 0.0 ||
                        !std::isfinite(total_weight)) {
                        throw std::runtime_error(
                            "Mipmap generation produced an invalid filter footprint."
                        );
                    }

                    next.pixels.push_back(
                        weighted_sum / total_weight
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
