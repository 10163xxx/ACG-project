#pragma once

#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <map>

#include "geometry/mesh.h"
#include "materials/mtl_loader.h"

class ObjLoader {
private:
    struct ParsedObj {
        std::vector<Vec3> vertices;
        std::vector<Vec3> texcoords;
        std::vector<Vec3> normals;

        std::vector<TriangleMesh::Face> faces;
        std::vector<TriangleMesh::FaceUV> face_texcoords;
        std::vector<TriangleMesh::FaceNormal> face_normals;

        std::vector<bool> face_has_uv;
        std::vector<bool> face_has_normals;

        // Per-corner explicit normal availability.
        std::vector<std::array<bool, 3>> face_corner_has_normals;

        // Empty string means smoothing is disabled.
        std::vector<std::string> face_smoothing_groups;

        std::vector<std::string> face_material_names;
        std::vector<std::string> material_libraries;
    };

    static int parse_index(
        const std::string& token,
        std::size_t count
    ) {
        std::size_t consumed = 0;
        const long long index =
            std::stoll(token, &consumed);

        if (consumed != token.size()) {
            throw std::runtime_error(
                "Invalid OBJ index: " + token
            );
        }

        if (index == 0) {
            throw std::runtime_error(
                "OBJ index cannot be zero."
            );
        }

        if (count >
            static_cast<std::size_t>(
                std::numeric_limits<int>::max()
            )) {
            throw std::runtime_error(
                "OBJ attribute count exceeds supported index range."
            );
        }

        const long long attribute_count =
            static_cast<long long>(count);

        if (index > 0) {
            if (index > attribute_count) {
                throw std::out_of_range(
                    "OBJ index is out of range: " + token
                );
            }

            return static_cast<int>(index - 1);
        }

        if (index < -attribute_count) {
            throw std::out_of_range(
                "OBJ relative index is out of range: " + token
            );
        }

        return static_cast<int>(
            attribute_count + index
        );
    }

    static void parse_face_vertex(
        const std::string& token,
        std::size_t vertex_count,
        std::size_t texcoord_count,
        std::size_t normal_count,
        int& vertex_index,
        int& texcoord_index,
        int& normal_index,
        bool& has_uv,
        bool& has_normal
    ) {
        const std::size_t first_slash = token.find('/');
        const std::string vertex_token =
            token.substr(0, first_slash);

        vertex_index = parse_index(
            vertex_token,
            vertex_count
        );

        texcoord_index = -1;
        normal_index = -1;
        has_uv = false;
        has_normal = false;

        if (first_slash == std::string::npos) {
            return;
        }

        const std::size_t second_slash =
            token.find('/', first_slash + 1);

        const std::string texcoord_token =
            token.substr(
                first_slash + 1,
                second_slash == std::string::npos
                    ? std::string::npos
                    : second_slash - first_slash - 1
            );

        if (!texcoord_token.empty()) {
            texcoord_index = parse_index(
                texcoord_token,
                texcoord_count
            );

            has_uv = true;
        }

        if (second_slash == std::string::npos) {
            return;
        }

        const std::string normal_token =
            token.substr(second_slash + 1);

        if (normal_token.empty()) {
            throw std::runtime_error(
                "OBJ face vertex has an empty normal index: " + token
            );
        }

        if (normal_token.find('/') != std::string::npos) {
            throw std::runtime_error(
                "Invalid OBJ face vertex: " + token
            );
        }

        normal_index = parse_index(
            normal_token,
            normal_count
        );

        has_normal = true;
    }

    static ParsedObj parse(std::istream& input) {
        ParsedObj parsed;
        std::string current_material;
        std::string current_smoothing_group;
        std::string line;

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

            if (command == "v") {
                Vec3 vertex;
                stream >> vertex.x >> vertex.y >> vertex.z;
                parsed.vertices.push_back(vertex);
            } else if (command == "vt") {
                Vec3 texcoord(0.0, 0.0, 0.0);
                stream >> texcoord.x >> texcoord.y;
                parsed.texcoords.push_back(texcoord);
            } else if (command == "vn") {
                Vec3 normal;
                stream >> normal.x >> normal.y >> normal.z;

                if (!stream) {
                    throw std::runtime_error(
                        "Invalid OBJ vertex normal."
                    );
                }

                parsed.normals.push_back(normal);
            } else if (command == "mtllib") {
                std::string library;

                while (stream >> library) {
                    parsed.material_libraries.push_back(library);
                }
            } else if (command == "usemtl") {
                stream >> current_material;
            } else if (command == "s") {
                std::string group;

                if (!(stream >> group) ||
                    group == "off" ||
                    group == "0") {
                    current_smoothing_group.clear();
                } else {
                    current_smoothing_group = group;
                }
            } else if (command == "f") {
                std::vector<int> vertex_indices;
                std::vector<int> texcoord_indices;
                std::vector<int> normal_indices;

                std::vector<bool> vertex_has_uv;
                std::vector<bool> vertex_has_normal;

                std::string token;

                while (stream >> token) {
                    int vertex_index = -1;
                    int texcoord_index = -1;
                    int normal_index = -1;

                    bool has_uv = false;
                    bool has_normal = false;

                    parse_face_vertex(
                        token,
                        parsed.vertices.size(),
                        parsed.texcoords.size(),
                        parsed.normals.size(),
                        vertex_index,
                        texcoord_index,
                        normal_index,
                        has_uv,
                        has_normal
                    );

                    vertex_indices.push_back(vertex_index);
                    texcoord_indices.push_back(texcoord_index);
                    normal_indices.push_back(normal_index);

                    vertex_has_uv.push_back(has_uv);
                    vertex_has_normal.push_back(has_normal);
                }

                if (vertex_indices.size() < 3) {
                    throw std::runtime_error(
                        "OBJ face must contain at least three vertices."
                    );
                }

                // Triangulate polygons using a triangle fan.
                for (std::size_t i = 1;
                     i + 1 < vertex_indices.size();
                     ++i) {
                    parsed.faces.push_back({
                        static_cast<std::size_t>(vertex_indices[0]),
                        static_cast<std::size_t>(vertex_indices[i]),
                        static_cast<std::size_t>(vertex_indices[i + 1])
                    });

                    const bool has_uv =
                        vertex_has_uv[0] &&
                        vertex_has_uv[i] &&
                        vertex_has_uv[i + 1];

                    parsed.face_has_uv.push_back(has_uv);

                    if (has_uv) {
                        parsed.face_texcoords.push_back({
                            static_cast<std::size_t>(texcoord_indices[0]),
                            static_cast<std::size_t>(texcoord_indices[i]),
                            static_cast<std::size_t>(texcoord_indices[i + 1])
                        });
                    } else {
                        parsed.face_texcoords.push_back({
                            0, 0, 0
                        });
                    }

                    const bool has_normals =
                        vertex_has_normal[0] &&
                        vertex_has_normal[i] &&
                        vertex_has_normal[i + 1];

                    parsed.face_has_normals.push_back(has_normals);

                    parsed.face_corner_has_normals.push_back({
                        vertex_has_normal[0],
                        vertex_has_normal[i],
                        vertex_has_normal[i + 1]
                    });

                    if (has_normals) {
                        parsed.face_normals.push_back({
                            static_cast<std::size_t>(normal_indices[0]),
                            static_cast<std::size_t>(normal_indices[i]),
                            static_cast<std::size_t>(normal_indices[i + 1])
                        });
                    } else {
                        parsed.face_normals.push_back({
                            0, 0, 0
                        });
                    }

                    parsed.face_smoothing_groups.push_back(
                        current_smoothing_group
                    );

                    parsed.face_material_names.push_back(
                        current_material
                    );
                }
            }
        }

        return parsed;
    }

    static void generate_smoothing_normals(
        ParsedObj& parsed
    ) {
        using SmoothingKey =
            std::pair<std::size_t, std::string>;

        std::map<SmoothingKey, Vec3> accumulated_normals;

        // First pass: accumulate area-weighted face normals.
        for (std::size_t face_index = 0;
             face_index < parsed.faces.size();
             ++face_index) {
            const std::string& group =
                parsed.face_smoothing_groups[face_index];

            if (group.empty()) {
                continue;
            }

            const TriangleMesh::Face& face =
                parsed.faces[face_index];

            const Vec3& a = parsed.vertices[face[0]];
            const Vec3& b = parsed.vertices[face[1]];
            const Vec3& c = parsed.vertices[face[2]];

            const Vec3 weighted_normal =
                cross(b - a, c - a);

            for (const std::size_t vertex_index : face) {
                const SmoothingKey key(
                    vertex_index,
                    group
                );

                accumulated_normals[key] = accumulated_normals[key] + weighted_normal;
            }
        }

        // Normalize the accumulated normals.
        for (auto& entry : accumulated_normals) {
            const double length_squared =
                entry.second.length_squared();

            if (!std::isfinite(length_squared) ||
                length_squared < 1e-20) {
                entry.second = Vec3(0.0, 0.0, 0.0);
            } else {
                entry.second =
                    entry.second / std::sqrt(length_squared);
            }
        }

        // Second pass: resolve each triangle corner.
        for (std::size_t face_index = 0;
             face_index < parsed.faces.size();
             ++face_index) {
            const std::string& group =
                parsed.face_smoothing_groups[face_index];

            const TriangleMesh::Face& face =
                parsed.faces[face_index];

            const auto& explicit_normals =
                parsed.face_corner_has_normals[face_index];

            TriangleMesh::FaceNormal resolved_indices = {
                0, 0, 0
            };

            bool has_resolved_normals = true;

            for (std::size_t corner = 0;
                 corner < 3;
                 ++corner) {
                if (explicit_normals[corner]) {
                    resolved_indices[corner] =
                        parsed.face_normals[face_index][corner];

                    continue;
                }

                if (group.empty()) {
                    has_resolved_normals = false;
                    break;
                }

                const SmoothingKey key(
                    face[corner],
                    group
                );

                const auto it =
                    accumulated_normals.find(key);

                if (it == accumulated_normals.end() ||
                    it->second.length_squared() < 1e-20) {
                    has_resolved_normals = false;
                    break;
                }

                resolved_indices[corner] =
                    parsed.normals.size();

                parsed.normals.push_back(it->second);
            }

            if (has_resolved_normals) {
                parsed.face_normals[face_index] =
                    resolved_indices;

                parsed.face_has_normals[face_index] = true;
            } else {
                parsed.face_has_normals[face_index] = false;
            }
        }
    }

    static TriangleMesh build_mesh(
        ParsedObj parsed,
        std::shared_ptr<const Material> fallback_material,
        const MtlLoader::MaterialMap* materials
    ) {
        generate_smoothing_normals(parsed);

        std::vector<std::shared_ptr<const Material>> face_materials;
        bool has_face_material = false;

        if (materials != nullptr) {
            face_materials.reserve(
                parsed.face_material_names.size()
            );

            for (const std::string& name :
                 parsed.face_material_names) {
                const auto it = materials->find(name);

                if (!name.empty() && it != materials->end()) {
                    face_materials.push_back(it->second);
                    has_face_material = true;
                } else {
                    face_materials.push_back(fallback_material);
                }
            }
        }

        if (!has_face_material) {
            face_materials.clear();
        }

        return TriangleMesh(
            std::move(parsed.vertices),
            std::move(parsed.faces),
            std::move(fallback_material),
            std::move(parsed.texcoords),
            std::move(parsed.face_texcoords),
            std::move(parsed.face_has_uv),
            face_materials,
            std::move(parsed.normals),
            std::move(parsed.face_normals),
            std::move(parsed.face_has_normals)
        );
    }

public:
    static TriangleMesh load(
        std::istream& input,
        std::shared_ptr<const Material> material = nullptr
    ) {
        ParsedObj parsed = parse(input);

        return build_mesh(
            std::move(parsed),
            std::move(material),
            nullptr
        );
    }

    static TriangleMesh load_file(
        const std::string& filename,
        std::shared_ptr<const Material> fallback_material = nullptr
    ) {
        std::ifstream file(filename);

        if (!file) {
            throw std::runtime_error(
                "Failed to open OBJ file: " + filename
            );
        }

        ParsedObj parsed = parse(file);
        const std::filesystem::path obj_path(filename);
        const std::filesystem::path directory =
            obj_path.parent_path();

        MtlLoader::MaterialMap materials;

        for (const std::string& library :
             parsed.material_libraries) {
            const std::filesystem::path mtl_path =
                directory / library;

            MtlLoader::MaterialMap loaded =
                MtlLoader::load_file(mtl_path.string());

            materials.insert(loaded.begin(), loaded.end());
        }

        return build_mesh(
            std::move(parsed),
            std::move(fallback_material),
            &materials
        );
    }
};