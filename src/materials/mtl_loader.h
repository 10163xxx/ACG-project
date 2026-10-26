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
#include <cstdlib>
#include <vector>

#include "materials/principled.h"
#include "materials/dielectric.h"
#include "materials/opacity_material.h"
#include "materials/image_texture.h"
#include "materials/textured_material.h"
#include "materials/normal_mapped_lambertian.h"
#include "materials/bump_mapped_material.h"
#include "materials/texture_transform.h"

struct MtlMapOptions {
    std::string filename;

    double scale_u = 1.0;
    double scale_v = 1.0;

    double offset_u = 0.0;
    double offset_v = 0.0;

    TextureAddressMode address_mode =
        TextureAddressMode::Clamp;
};

struct MtlProperties {
    Vec3 diffuse = Vec3(0.8, 0.8, 0.8);
    Vec3 specular = Vec3(0.04, 0.04, 0.04);
    double shininess = 10.0;
    double metallic = 0.0;
    double roughness = 1.0;
    bool has_roughness = false;
    double index_of_refraction = 1.0;
    double opacity = 1.0;
    int illumination_model = 2;

    MtlMapOptions diffuse_map;
    MtlMapOptions metallic_map;
    MtlMapOptions roughness_map;
    MtlMapOptions normal_map;
    MtlMapOptions bump_map;

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

        auto read_map_options = [&](
            std::istringstream& stream,
            double* bump_strength = nullptr
        ) -> MtlMapOptions {
            MtlMapOptions options;

            std::string remainder;
            std::getline(stream, remainder);
            remainder = trim(remainder);

            std::istringstream option_stream(remainder);
            std::vector<std::string> tokens;
            std::string token;

            while (option_stream >> token) {
                tokens.push_back(token);
            }

            auto parse_number = [](
                const std::string& text,
                double& result
            ) -> bool {
                char* end = nullptr;
                result = std::strtod(text.c_str(), &end);

                return end != text.c_str() &&
                    end != nullptr &&
                    *end == '\0' &&
                    std::isfinite(result);
            };

            std::size_t index = 0;

            while (index < tokens.size()) {
                const std::string& current = tokens[index];

                if (current == "-s" || current == "-o") {
                    const bool is_scale = current == "-s";
                    ++index;

                    double values[3] = {
                        is_scale ? 1.0 : 0.0,
                        is_scale ? 1.0 : 0.0,
                        is_scale ? 1.0 : 0.0
                    };

                    std::size_t count = 0;

                    while (index < tokens.size() &&
                           count < 3) {
                        double value = 0.0;

                        if (!parse_number(
                                tokens[index],
                                value
                            )) {
                            break;
                        }

                        values[count++] = value;
                        ++index;
                    }

                    if (count == 0) {
                        throw std::runtime_error(
                            "MTL map option " + current +
                            " requires at least one numeric value."
                        );
                    }

                    if (is_scale) {
                        options.scale_u = values[0];
                        options.scale_v =
                            count >= 2 ? values[1] : 1.0;
                    } else {
                        options.offset_u = values[0];
                        options.offset_v =
                            count >= 2 ? values[1] : 0.0;
                    }

                    continue;
                }

                if (current == "-clamp") {
                    ++index;

                    if (index >= tokens.size()) {
                        throw std::runtime_error(
                            "MTL map option -clamp requires on or off."
                        );
                    }

                    if (tokens[index] == "on") {
                        options.address_mode =
                            TextureAddressMode::Clamp;
                    } else if (tokens[index] == "off") {
                        options.address_mode =
                            TextureAddressMode::Repeat;
                    } else {
                        throw std::runtime_error(
                            "MTL map option -clamp accepts only on or off."
                        );
                    }

                    ++index;
                    continue;
                }

                if (current == "-bm") {
                    ++index;

                    if (index >= tokens.size()) {
                        throw std::runtime_error(
                            "MTL map option -bm requires a numeric value."
                        );
                    }

                    double strength = 1.0;

                    if (!parse_number(
                            tokens[index],
                            strength
                        )) {
                        throw std::runtime_error(
                            "Invalid numeric value for MTL option -bm."
                        );
                    }

                    if (bump_strength != nullptr) {
                        *bump_strength = strength;
                    }

                    ++index;
                    continue;
                }

                // The first non-option token begins the filename.
                // Join the remaining tokens to preserve spaces.
                for (; index < tokens.size(); ++index) {
                    if (!options.filename.empty()) {
                        options.filename += ' ';
                    }

                    options.filename += tokens[index];
                }

                break;
            }

            if (options.filename.empty()) {
                throw std::runtime_error(
                    "MTL map declaration is missing a filename."
                );
            }

            if (!std::isfinite(options.scale_u) ||
                !std::isfinite(options.scale_v) ||
                options.scale_u == 0.0 ||
                options.scale_v == 0.0 ||
                !std::isfinite(options.offset_u) ||
                !std::isfinite(options.offset_v)) {
                throw std::runtime_error(
                    "MTL map has invalid scale or offset values."
                );
            }

            return options;
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

                    auto make_map_texture = [&](
                const MtlMapOptions& map,
                bool decode_srgb
            ) -> std::shared_ptr<const Texture> {
                const auto texture_path =
                    resolve_path(map.filename);

                std::shared_ptr<const Texture> texture =
                    std::make_shared<ImageTexture>(
                        texture_path.string(),
                        true,
                        decode_srgb,
                        map.address_mode
                    );

                if (map.scale_u != 1.0 ||
                    map.scale_v != 1.0 ||
                    map.offset_u != 0.0 ||
                    map.offset_v != 0.0) {
                    texture =
                        std::make_shared<TextureTransform>(
                            texture,
                            map.scale_u,
                            map.scale_v,
                            map.offset_u,
                            map.offset_v
                        );
                }

                return texture;
            };

            std::shared_ptr<const Texture> metallic_texture;
            std::shared_ptr<const Texture> roughness_texture;

            if (!properties.metallic_map.filename.empty()) {
                metallic_texture = make_map_texture(
                    properties.metallic_map,
                    false
                );
            }

            if (!properties.roughness_map.filename.empty()) {
                roughness_texture = make_map_texture(
                    properties.roughness_map,
                    false
                );
            }

            if (indicates_glass) {
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
                    properties.has_roughness
                        ? properties.roughness
                        : std::sqrt(
                            2.0 /
                            (
                                std::max(
                                    0.0,
                                    properties.shininess
                                ) + 2.0
                            )
                        );

                base_material =
                    std::make_shared<PrincipledBSDF>(
                        properties.diffuse,
                        properties.metallic,
                        roughness,
                        dielectric_f0,
                        metallic_texture,
                        roughness_texture
                    );
            }

            // Color textures are decoded from sRGB to linear space.
            if (!properties.diffuse_map.filename.empty()) {
                auto texture = make_map_texture(
                    properties.diffuse_map,
                    true
                );

                base_material =
                    std::make_shared<TexturedMaterial>(
                        base_material,
                        texture
                    );
            }

            // Height maps remain linear data textures.
            if (!properties.bump_map.filename.empty()) {
                auto height_texture = make_map_texture(
                    properties.bump_map,
                    false
                );

                base_material =
                    std::make_shared<BumpMappedMaterial>(
                        base_material,
                        height_texture,
                        properties.bump_strength
                    );
            }

            // Normal maps remain linear data textures.
            if (!properties.normal_map.filename.empty()) {
                auto normal_texture = make_map_texture(
                    properties.normal_map,
                    false
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
                double value = properties.shininess;

                if (stream >> value) {
                    if (std::isfinite(value)) {
                        properties.shininess =
                            std::max(0.0, value);
                    }
                }
            } else if (command == "Pm") {
                double value = properties.metallic;

                if (stream >> value) {
                    if (std::isfinite(value)) {
                        properties.metallic =
                            std::clamp(value, 0.0, 1.0);
                    }
                }
            } else if (command == "Pr") {
                double value = properties.roughness;

                if (stream >> value) {
                    if (std::isfinite(value)) {
                        properties.roughness =
                            std::clamp(value, 0.045, 1.0);
                        properties.has_roughness = true;
                    }
                }
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
                properties.diffuse_map =
                    read_map_options(stream);
            } else if (command == "map_Pm") {
                properties.metallic_map =
                    read_map_options(stream);
            } else if (command == "map_Pr") {
                properties.roughness_map =
                    read_map_options(stream);
            } else if (
                command == "map_Bump" ||
                command == "map_bump" ||
                command == "bump"
            ) {
                properties.bump_map =
                    read_map_options(
                        stream,
                        &properties.bump_strength
                    );
            } else if (
                command == "norm" ||
                command == "map_Kn"
            ) {
                properties.normal_map =
                    read_map_options(stream);
            }
        }

        save_material();
        return materials;
    }
};
