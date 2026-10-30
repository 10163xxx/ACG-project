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

                    parsed.face_material_names.push_back(
                        current_material
                    );
                }
            }
        }

        return parsed;
    }

    static TriangleMesh build_mesh(
        ParsedObj parsed,
        std::shared_ptr<const Material> fallback_material,
        const MtlLoader::MaterialMap* materials
    ) {
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