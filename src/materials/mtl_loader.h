#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "materials/principled.h"
#include "materials/dielectric.h"
#include "materials/opacity_material.h"
#include "materials/image_texture.h"
#include "materials/textured_material.h"
#include "materials/normal_mapped_lambertian.h"
#include "materials/bump_mapped_material.h"

struct MtlProperties {
    Vec3 diffuse = Vec3(0.8, 0.8, 0.8);
    Vec3 specular = Vec3(0.04, 0.04, 0.04);
    double shininess = 10.0;
    double index_of_refraction = 1.0;
    double opacity = 1.0;
    int illumination_model = 2;

    std::string diffuse_map;
    std::string normal_map;
    std::string bump_map;
    double bump_strength = 1.0;
};

class MtlLoader {
public:
    using MaterialMap =
        std::map<std::string, std::shared_ptr<const Material>>;

    static MaterialMap load_file(const std::string& filename) {
        std::ifstream file(filename);

        if (!file) {
            throw std::runtime_error(
                "Failed to open MTL file: " + filename
            );
        }

        const std::filesystem::path mtl_path(filename);

        return load(file, mtl_path.parent_path());
    }

    static MaterialMap load(std::istream& input) {
        return load(input, std::filesystem::path{});
    }

    static MaterialMap load(
        std::istream& input,
        const std::filesystem::path& base_directory
    ) {
        MaterialMap materials;
        MtlProperties properties;
        std::string current_name;
        std::string line;

        auto resolve_path = [&](
            const std::string& filename
        ) -> std::filesystem::path {
            std::filesystem::path path(filename);

            if (path.is_relative()) {
                path = base_directory / path;
            }

            return path;
        };

        auto trim = [](const std::string& value) {
            const std::size_t first =
                value.find_first_not_of(" \t\r\n");

            if (first == std::string::npos) {
                return std::string{};
            }

            const std::size_t last =
                value.find_last_not_of(" \t\r\n");

            return value.substr(first, last - first + 1);
        };

        auto read_map_path = [&](
            std::istringstream& stream,
            double* bump_strength = nullptr
        ) -> std::string {
            std::string value;
            std::getline(stream, value);
            value = trim(value);

            if (value.rfind("-bm", 0) == 0 &&
                value.size() > 3 &&
                (value[3] == ' ' || value[3] == '\t')) {
                std::istringstream option_stream(value);
                std::string option;
                double strength = 1.0;

                option_stream >> option >> strength;
                std::getline(option_stream, value);
                value = trim(value);

                if (bump_strength != nullptr &&
                    std::isfinite(strength)) {
                    *bump_strength = strength;
                }
            }

            return value;
        };

        auto save_material = [&]() {
            if (current_name.empty()) {
                return;
            }

            const double ior =
                properties.index_of_refraction;

            const bool has_valid_ior =
                std::isfinite(ior) && ior > 1.001;

            const bool indicates_glass =
                properties.illumination_model == 4 ||
                properties.illumination_model == 6 ||
                properties.illumination_model == 7 ||
                properties.illumination_model == 9;

            std::shared_ptr<const Material> base_material;

            if (has_valid_ior || indicates_glass) {
                const double glass_ior =
                    has_valid_ior ? ior : 1.5;

                const Vec3 glass_tint(
                    std::clamp(properties.diffuse.x, 0.0, 1.0),
                    std::clamp(properties.diffuse.y, 0.0, 1.0),
                    std::clamp(properties.diffuse.z, 0.0, 1.0)
                );

                base_material =
                    std::make_shared<Dielectric>(
                        glass_ior,
                        glass_tint
                    );
            } else {
                const Vec3 dielectric_f0(
                    std::clamp(properties.specular.x, 0.0, 1.0),
                    std::clamp(properties.specular.y, 0.0, 1.0),
                    std::clamp(properties.specular.z, 0.0, 1.0)
                );

                const double roughness =
                    std::clamp(
                        std::sqrt(
                            2.0 /
                            (
                                std::max(
                                    0.0,
                                    properties.shininess
                                ) + 2.0
                            )
                        ),
                        0.045,
                        1.0
                    );

                base_material =
                    std::make_shared<PrincipledBSDF>(
                        properties.diffuse,
                        0.0,
                        roughness,
                        dielectric_f0
                    );
            }

            // Apply the diffuse/color texture first.
            if (!properties.diffuse_map.empty()) {
                const auto texture_path =
                    resolve_path(properties.diffuse_map);

                auto texture =
                    std::make_shared<ImageTexture>(
                        texture_path.string()
                    );

                base_material =
                    std::make_shared<TexturedMaterial>(
                        base_material,
                        texture
                    );
            }

            // Apply a scalar height map as bump mapping.
            if (!properties.bump_map.empty()) {
                const auto bump_path =
                    resolve_path(properties.bump_map);

                auto height_texture =
                    std::make_shared<ImageTexture>(
                        bump_path.string()
                    );

                base_material =
                    std::make_shared<BumpMappedMaterial>(
                        base_material,
                        height_texture,
                        properties.bump_strength
                    );
            }

            // Then decorate the existing material with a normal map.
            if (!properties.normal_map.empty()) {
                const auto normal_path =
                    resolve_path(properties.normal_map);

                auto normal_texture =
                    std::make_shared<ImageTexture>(
                        normal_path.string()
                    );

                base_material =
                    std::make_shared<NormalMappedMaterial>(
                        base_material,
                        normal_texture
                    );
            }

            // Preserve the existing opacity handling.
            const double opacity =
                std::clamp(
                    properties.opacity,
                    0.0,
                    1.0
                );

            if (opacity < 1.0) {
                materials[current_name] =
                    std::make_shared<OpacityMaterial>(
                        base_material,
                        opacity
                    );
            } else {
                materials[current_name] = base_material;
            }
        };

        while (std::getline(input, line)) {
            const std::size_t comment = line.find('#');

            if (comment != std::string::npos) {
                line.erase(comment);
            }

            std::istringstream stream(line);
            std::string command;

            if (!(stream >> command)) {
                continue;
            }

            if (command == "newmtl") {
                save_material();

                stream >> current_name;
                properties = MtlProperties{};
            } else if (command == "Kd") {
                stream >>
                    properties.diffuse.x >>
                    properties.diffuse.y >>
                    properties.diffuse.z;

                properties.diffuse.x =
                    std::max(0.0, properties.diffuse.x);
                properties.diffuse.y =
                    std::max(0.0, properties.diffuse.y);
                properties.diffuse.z =
                    std::max(0.0, properties.diffuse.z);
            } else if (command == "Ks") {
                stream >>
                    properties.specular.x >>
                    properties.specular.y >>
                    properties.specular.z;
            } else if (command == "Ns") {
                stream >> properties.shininess;
            } else if (command == "Ni") {
                stream >> properties.index_of_refraction;
            } else if (command == "d") {
                stream >> properties.opacity;

                properties.opacity =
                    std::clamp(
                        properties.opacity,
                        0.0,
                        1.0
                    );
            } else if (command == "Tr") {
                double transparency = 0.0;
                stream >> transparency;

                properties.opacity =
                    1.0 - std::clamp(
                        transparency,
                        0.0,
                        1.0
                    );
            } else if (command == "illum") {
                stream >> properties.illumination_model;
            } else if (command == "map_Kd") {
                properties.diffuse_map = read_map_path(stream);
            } else if (
                command == "map_Bump" ||
                command == "map_bump" ||
                command == "bump"
            ) {
                properties.bump_map =
                    read_map_path(
                        stream,
                        &properties.bump_strength
                    );
            } else if (
                command == "norm" ||
                command == "map_Kn"
            ) {
                properties.normal_map =
                    read_map_path(stream);
            }
        }

        save_material();
        return materials;
    }
};
