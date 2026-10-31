#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/random.h"
#include "core/vec3.h"

class EnvironmentMap {
public:
    EnvironmentMap() = default;

    explicit EnvironmentMap(const std::string& filename) {
        load(filename);
    }

    void load(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);

        if (!file) {
            throw std::runtime_error(
                "Cannot open HDR environment: " + filename
            );
        }

        std::string line;
        bool format_supported = false;

        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line == "FORMAT=32-bit_rle_rgbe") {
                format_supported = true;
            }

            if (line.empty()) {
                break;
            }
        }

        if (!format_supported) {
            throw std::runtime_error(
                "Unsupported HDR format. Expected RGBE."
            );
        }

        // This implementation supports the common -Y +X orientation.
        // Other orientation layouts are deliberately rejected.
        std::string y_axis;
        std::string x_axis;
        int height = 0;
        int width = 0;

        file >> y_axis >> height >> x_axis >> width;

        if (y_axis != "-Y" || x_axis != "+X"
            || width <= 0 || height <= 0) {
            throw std::runtime_error(
                "Unsupported HDR resolution or orientation."
            );
        }

        file.ignore(
            std::numeric_limits<std::streamsize>::max(),
            '\n'
        );

        width_ = width;
        height_ = height;

        pixels_.assign(
            static_cast<std::size_t>(width_) * height_,
            Vec3(0.0, 0.0, 0.0)
        );

        std::vector<unsigned char> scanline(
            static_cast<std::size_t>(width_) * 4
        );

        for (int y = 0; y < height_; ++y) {
            read_scanline(file, scanline);

            for (int x = 0; x < width_; ++x) {
                const std::size_t offset =
                    static_cast<std::size_t>(x) * 4;

                const unsigned char r = scanline[offset + 0];
                const unsigned char g = scanline[offset + 1];
                const unsigned char b = scanline[offset + 2];
                const unsigned char e = scanline[offset + 3];

                Vec3 color(0.0, 0.0, 0.0);

                if (e != 0) {
                    const double scale =
                        std::ldexp(1.0, static_cast<int>(e) - 136);

                    color = Vec3(
                        static_cast<double>(r) * scale,
                        static_cast<double>(g) * scale,
                        static_cast<double>(b) * scale
                    );
                }

                pixels_[
                    static_cast<std::size_t>(y) * width_ + x
                ] = color;
            }
        }

        build_importance_distribution();
    }

    bool empty() const {
        return width_ <= 0
            || height_ <= 0
            || pixels_.empty();
    }

    int width() const {
        return width_;
    }

    int height() const {
        return height_;
    }

    Vec3 evaluate(const Vec3& direction) const {
        if (empty()) {
            return Vec3(0.0, 0.0, 0.0);
        }

        const Vec3 d = normalize(direction);

        if (d.near_zero()) {
            return Vec3(0.0, 0.0, 0.0);
        }

        constexpr double TWO_PI =
            6.28318530717958647692;

        const double u = std::atan2(d.z, d.x) / TWO_PI + 0.5;

        const double v =
            std::acos(std::clamp(d.y, -1.0, 1.0))
            / 3.14159265358979323846;

        const double wrapped_u = u - std::floor(u);

        const int x = std::clamp(
            static_cast<int>(wrapped_u * width_),
            0,
            width_ - 1
        );

        const int y = std::clamp(
            static_cast<int>(v * height_),
            0,
            height_ - 1
        );

        return pixels_[
            static_cast<std::size_t>(y) * width_ + x
        ];
    }

    // Sample a direction according to luminance * sin(theta).
    // The returned PDF is measured per unit solid angle.
    Vec3 sample_direction(double& pdf) const {
        if (empty()) {
            pdf = 0.0;
            return Vec3(0.0, 1.0, 0.0);
        }

        constexpr double PI =
            3.14159265358979323846;

        constexpr double INV_FOUR_PI =
            0.07957747154594766788;

        if (uniform_fallback_) {
            const double y =
                1.0 - 2.0 * Random::random_double();

            const double phi =
                2.0 * PI * Random::random_double();

            const double radius =
                std::sqrt(std::max(0.0, 1.0 - y * y));

            pdf = INV_FOUR_PI;

            return Vec3(
                radius * std::cos(phi),
                y,
                radius * std::sin(phi)
            );
        }

        if (cdf_.empty() || total_weight_ <= 0.0) {
            pdf = 0.0;
            return Vec3(0.0, 1.0, 0.0);
        }

        const double target =
            Random::random_double() * total_weight_;

        auto it = std::lower_bound(
            cdf_.begin(),
            cdf_.end(),
            target
        );

        std::size_t index =
            static_cast<std::size_t>(
                std::distance(cdf_.begin(), it)
            );

        if (index >= cdf_.size()) {
            index = cdf_.size() - 1;
        }

        const int x =
            static_cast<int>(index % width_);

        const int y =
            static_cast<int>(index / width_);

        const double u =
            (x + Random::random_double()) / width_;

        const double v =
            (y + Random::random_double()) / height_;

        constexpr double TWO_PI =
            6.28318530717958647692;

        const double phi = TWO_PI * (u - 0.5);
        const double theta = PI * v;
        const double sin_theta = std::sin(theta);

        const Vec3 direction(
            sin_theta * std::cos(phi),
            std::cos(theta),
            sin_theta * std::sin(phi)
        );

        pdf = pdf_for_pixel(index, sin_theta);

        return normalize(direction);
    }

    double pdf(const Vec3& direction) const {
        if (empty()) {
            return 0.0;
        }

        if (uniform_fallback_) {
            const Vec3 d = normalize(direction);

            if (d.near_zero()) {
                return 0.0;
            }

            return 0.07957747154594766788;
        }

        if (total_weight_ <= 0.0) {
            return 0.0;
        }

        const Vec3 d = normalize(direction);

        if (d.near_zero()) {
            return 0.0;
        }

        constexpr double TWO_PI =
            6.28318530717958647692;

        const double u =
            std::atan2(d.z, d.x) / TWO_PI + 0.5;

        const double v =
            std::acos(std::clamp(d.y, -1.0, 1.0))
            / 3.14159265358979323846;

        const double wrapped_u = u - std::floor(u);

        const int x = std::clamp(
            static_cast<int>(wrapped_u * width_),
            0,
            width_ - 1
        );

        const int y = std::clamp(
            static_cast<int>(v * height_),
            0,
            height_ - 1
        );

        const std::size_t index =
            static_cast<std::size_t>(y) * width_ + x;

        const double sin_theta =
            std::sqrt(std::max(0.0, 1.0 - d.y * d.y));

        return pdf_for_pixel(index, sin_theta);
    }

private:
    int width_ = 0;
    int height_ = 0;

    std::vector<Vec3> pixels_;
    std::vector<double> cdf_;
    std::vector<double> weights_;

    double total_weight_ = 0.0;
    bool uniform_fallback_ = false;

    static double luminance(const Vec3& color) {
        return 0.2126 * color.x
            + 0.7152 * color.y
            + 0.0722 * color.z;
    }

    static unsigned char read_byte(std::ifstream& file) {
        char value = 0;

        if (!file.get(value)) {
            throw std::runtime_error(
                "Unexpected end of HDR file."
            );
        }

        return static_cast<unsigned char>(value);
    }

    static void read_scanline(
        std::ifstream& file,
        std::vector<unsigned char>& scanline
    ) {
        const int width =
            static_cast<int>(scanline.size() / 4);

        const unsigned char b0 = read_byte(file);
        const unsigned char b1 = read_byte(file);
        const unsigned char b2 = read_byte(file);
        const unsigned char b3 = read_byte(file);

        const bool is_new_rle =
            width >= 8
            && width <= 32767
            && b0 == 2
            && b1 == 2
            && (b2 & 0x80) == 0
            && ((static_cast<int>(b2) << 8) | b3) == width;

        if (!is_new_rle) {
            // Old-style / flat RGBE scanline.
            scanline[0] = b0;
            scanline[1] = b1;
            scanline[2] = b2;
            scanline[3] = b3;

            for (std::size_t i = 4; i < scanline.size(); ++i) {
                scanline[i] = read_byte(file);
            }

            return;
        }

        // Radiance RLE stores each channel independently.
        for (int channel = 0; channel < 4; ++channel) {
            int x = 0;

            while (x < width) {
                const unsigned char code = read_byte(file);

                if (code > 128) {
                    const int count =
                        static_cast<int>(code) - 128;

                    if (count <= 0 || x + count > width) {
                        throw std::runtime_error(
                            "Invalid HDR RLE run."
                        );
                    }

                    const unsigned char value = read_byte(file);

                    for (int i = 0; i < count; ++i) {
                        scanline[
                            static_cast<std::size_t>(x++) * 4
                            + channel
                        ] = value;
                    }
                } else {
                    const int count = static_cast<int>(code);

                    if (count <= 0 || x + count > width) {
                        throw std::runtime_error(
                            "Invalid HDR RLE literal."
                        );
                    }

                    for (int i = 0; i < count; ++i) {
                        scanline[
                            static_cast<std::size_t>(x++) * 4
                            + channel
                        ] = read_byte(file);
                    }
                }
            }
        }
    }

    void build_importance_distribution() {
        uniform_fallback_ = false;

        const std::size_t count = pixels_.size();

        weights_.assign(count, 0.0);
        cdf_.assign(count, 0.0);

        total_weight_ = 0.0;

        constexpr double PI =
            3.14159265358979323846;

        for (int y = 0; y < height_; ++y) {
            const double theta =
                PI * (y + 0.5) / height_;

            const double sin_theta = std::sin(theta);

            for (int x = 0; x < width_; ++x) {
                const std::size_t index =
                    static_cast<std::size_t>(y) * width_ + x;

                const double value = std::max(
                    0.0,
                    luminance(pixels_[index])
                );

                // The sin(theta) factor accounts for the
                // changing solid angle of equirectangular texels.
                const double weight = value * sin_theta;

                weights_[index] = weight;
                total_weight_ += weight;
                cdf_[index] = total_weight_;
            }
        }

        // A completely black map has no luminance-based
        // distribution. Use uniform sampling over the sphere.
        if (total_weight_ <= 0.0) {
            uniform_fallback_ = true;
            total_weight_ = 0.0;
        }
    }

    double pdf_for_pixel(
        std::size_t index,
        double sin_theta
    ) const {
        if (total_weight_ <= 0.0
            || sin_theta <= 1e-10) {
            return 0.0;
        }

        constexpr double TWO_PI_SQUARED =
            19.739208802178716;

        const double probability =
            weights_[index] / total_weight_;

        return probability
            * static_cast<double>(width_) * height_
            / (TWO_PI_SQUARED * sin_theta);
    }
};