
#pragma once

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>

#include "core/vec3.h"

class Image {
public:
    Image(int width, int height)
        : width_(width),
          height_(height),
          pixels_(checked_pixel_count(width, height), Vec3{}) {}

    int width() const { return width_; }
    int height() const { return height_; }

    // Set the color of a pixel.
    // Coordinates start at the top-left corner.
    void set_pixel(int x, int y, const Vec3& color) {
        check_coordinates(x, y);
        pixels_[static_cast<std::size_t>(y) * width_ + x] = color;
    }

    // Read the color of a pixel.
    const Vec3& get_pixel(int x, int y) const {
        check_coordinates(x, y);
        return pixels_[static_cast<std::size_t>(y) * width_ + x];
    }

    // Write an ASCII PPM image (P3 format).
    // Automatically create the parent directory if needed.
    void write_ppm(const std::string& filename) const {
        const std::filesystem::path output_path(filename);
        const std::filesystem::path parent =
            output_path.parent_path();

        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);

            if (ec) {
                throw std::runtime_error(
                    "Failed to create output directory: "
                    + parent.string()
                    + " (" + ec.message() + ")"
                );
            }
        }

        std::ofstream out(output_path);
        if (!out) {
            throw std::runtime_error(
                "Failed to open output image: " + filename
            );
        }

        out << "P3\n";
        out << width_ << ' ' << height_ << '\n';
        out << "255\n";

        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                const Vec3& color = get_pixel(x, y);

                out << to_byte(color.x) << ' '
                    << to_byte(color.y) << ' '
                    << to_byte(color.z) << '\n';
            }
        }

        if (!out) {
            throw std::runtime_error(
                "Failed while writing output image: " + filename
            );
        }
    }

private:
    int width_;
    int height_;
    std::vector<Vec3> pixels_;

    static std::size_t checked_pixel_count(int width, int height) {
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument(
                "Image dimensions must be positive."
            );
        }

        return static_cast<std::size_t>(width)
             * static_cast<std::size_t>(height);
    }

    void check_coordinates(int x, int y) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) {
            throw std::out_of_range("Pixel coordinates out of range.");
        }
    }

    static int to_byte(double value) {
        // Clamp to the valid display range, then convert to 8-bit color.
        value = std::clamp(value, 0.0, 1.0);
        return static_cast<int>(255.999 * value);
    }
};
