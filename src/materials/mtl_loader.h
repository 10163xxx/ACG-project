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

struct MtlProperties {
    Vec3 diffuse = Vec3(0.8, 0.8, 0.8);
    Vec3 specular = Vec3(0.04, 0.04, 0.04);
    double shininess = 10.0;
    double index_of_refraction = 1.0;
    double opacity = 1.0;
    int illumination_model = 2;
    std::string diffuse_map;
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

            if (!properties.diffuse_map.empty()) {
                std::filesystem::path texture_path =
                    std::filesystem::path(
                        properties.diffuse_map
                    );

                if (texture_path.is_relative()) {
                    texture_path =
                        base_directory / texture_path;
                }

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
                // This implementation supports a direct image path.
                // Advanced MTL map options (such as -s and -o) are
                // intentionally not interpreted in this version.
                std::string texture_path;

                std::getline(stream, texture_path);

                const std::size_t first =
                    texture_path.find_first_not_of(" \t\r\n");

                if (first != std::string::npos) {
                    const std::size_t last =
                        texture_path.find_last_not_of(" \t\r\n");

                    properties.diffuse_map =
                        texture_path.substr(
                            first,
                            last - first + 1
                        );
                }
            }
        }

        save_material();
        return materials;
    }
};
