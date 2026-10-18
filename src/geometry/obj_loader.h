#pragma once

#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "geometry/mesh.h"

class ObjLoader {
public:
    // Load OBJ geometry from a stream.
    static TriangleMesh load(
        std::istream& input,
        std::shared_ptr<const Material> material = nullptr
    ) {
        std::vector<Vec3> vertices;
        std::vector<Vec3> texcoords;
        std::vector<TriangleMesh::Face> faces;
        std::vector<TriangleMesh::FaceUV> face_texcoords;
        std::vector<bool> face_has_uv;

        std::string line;
        std::size_t line_number = 0;

        while (std::getline(input, line)) {
            ++line_number;

            const std::size_t comment_pos = line.find('#');

            if (comment_pos != std::string::npos) {
                line.erase(comment_pos);
            }

            std::istringstream line_stream(line);
            std::string keyword;

            if (!(line_stream >> keyword)) {
                continue;
            }

            if (keyword == "v") {
                double x = 0.0;
                double y = 0.0;
                double z = 0.0;

                if (!(line_stream >> x >> y >> z)) {
                    throw std::runtime_error(
                        make_error(line_number, "Invalid vertex position.")
                    );
                }

                if (!std::isfinite(x) ||
                    !std::isfinite(y) ||
                    !std::isfinite(z)) {
                    throw std::runtime_error(
                        make_error(
                            line_number,
                            "Vertex coordinates must be finite."
                        )
                    );
                }

                vertices.emplace_back(x, y, z);
            } else if (keyword == "vt") {
                double u = 0.0;
                double v = 0.0;

                if (!(line_stream >> u >> v)) {
                    throw std::runtime_error(
                        make_error(line_number, "Invalid texture coordinate.")
                    );
                }

                if (!std::isfinite(u) || !std::isfinite(v)) {
                    throw std::runtime_error(
                        make_error(
                            line_number,
                            "Texture coordinates must be finite."
                        )
                    );
                }

                texcoords.emplace_back(u, v, 0.0);
            } else if (keyword == "f") {
                struct FaceVertex {
                    std::size_t vertex_index;
                    std::size_t texcoord_index;
                    bool has_texcoord;
                };

                std::vector<FaceVertex> polygon;
                std::string token;

                while (line_stream >> token) {
                    const std::size_t first_slash = token.find('/');

                    const std::string vertex_text =
                        token.substr(0, first_slash);

                    const std::size_t vertex_index = parse_obj_index(
                        vertex_text,
                        vertices.size(),
                        line_number,
                        "vertex"
                    );

                    bool has_texcoord = false;
                    std::size_t texcoord_index = 0;

                    if (first_slash != std::string::npos) {
                        const std::size_t second_slash =
                            token.find('/', first_slash + 1);

                        const std::string texcoord_text =
                            token.substr(
                                first_slash + 1,
                                second_slash == std::string::npos
                                    ? std::string::npos
                                    : second_slash - first_slash - 1
                            );

                        if (!texcoord_text.empty()) {
                            texcoord_index = parse_obj_index(
                                texcoord_text,
                                texcoords.size(),
                                line_number,
                                "texture coordinate"
                            );

                            has_texcoord = true;
                        }
                    }

                    polygon.push_back({
                        vertex_index,
                        texcoord_index,
                        has_texcoord
                    });
                }

                if (polygon.size() < 3) {
                    throw std::runtime_error(
                        make_error(
                            line_number,
                            "A face must contain at least 3 vertices."
                        )
                    );
                }

                const bool polygon_has_uv =
                    polygon.front().has_texcoord;

                for (const FaceVertex& vertex : polygon) {
                    if (vertex.has_texcoord != polygon_has_uv) {
                        throw std::runtime_error(
                            make_error(
                                line_number,
                                "A face cannot mix vertices with and without UV coordinates."
                            )
                        );
                    }
                }

                for (std::size_t i = 1;
                     i + 1 < polygon.size();
                     ++i) {
                    faces.push_back({
                        polygon[0].vertex_index,
                        polygon[i].vertex_index,
                        polygon[i + 1].vertex_index
                    });

                    face_texcoords.push_back({
                        polygon[0].texcoord_index,
                        polygon[i].texcoord_index,
                        polygon[i + 1].texcoord_index
                    });

                    face_has_uv.push_back(polygon_has_uv);
                }
            }
        }

        if (input.bad()) {
            throw std::runtime_error(
                "Failed while reading OBJ input."
            );
        }

        if (vertices.empty()) {
            throw std::runtime_error(
                "OBJ file contains no vertices."
            );
        }

        if (faces.empty()) {
            throw std::runtime_error(
                "OBJ file contains no faces."
            );
        }

        try {
            return TriangleMesh(
                vertices,
                faces,
                std::move(material),
                texcoords,
                face_texcoords,
                face_has_uv
            );
        } catch (const std::exception& exception) {
            throw std::runtime_error(
                std::string("Invalid OBJ mesh: ") + exception.what()
            );
        }
    }

    // Load OBJ geometry from a file.
    static TriangleMesh load_file(
        const std::string& filename,
        std::shared_ptr<const Material> material = nullptr
    ) {
        std::ifstream file(filename);

        if (!file.is_open()) {
            throw std::runtime_error(
                "Cannot open OBJ file: " + filename
            );
        }

        return load(file, std::move(material));
    }

private:
    static std::string make_error(
        std::size_t line_number,
        const std::string& message
    ) {
        return "OBJ line " +
            std::to_string(line_number) +
            ": " + message;
    }

    static std::size_t parse_obj_index(
        const std::string& index_text,
        std::size_t item_count,
        std::size_t line_number,
        const std::string& index_type
    ) {
        if (index_text.empty()) {
            throw std::runtime_error(
                make_error(
                    line_number,
                    "Missing " + index_type + " index."
                )
            );
        }

        long long obj_index = 0;

        try {
            std::size_t parsed_chars = 0;

            obj_index = std::stoll(index_text, &parsed_chars);

            if (parsed_chars != index_text.size()) {
                throw std::runtime_error("Invalid index.");
            }
        } catch (const std::exception&) {
            throw std::runtime_error(
                make_error(
                    line_number,
                    "Invalid " + index_type + " index: " + index_text
                )
            );
        }

        if (obj_index == 0) {
            throw std::runtime_error(
                make_error(
                    line_number,
                    "OBJ indices cannot be zero."
                )
            );
        }

        if (item_count >
            static_cast<std::size_t>(
                std::numeric_limits<long long>::max()
            )) {
            throw std::runtime_error(
                make_error(
                    line_number,
                    "Too many elements to resolve OBJ index."
                )
            );
        }

        const long long resolved_index =
            obj_index > 0
                ? obj_index - 1
                : static_cast<long long>(item_count) + obj_index;

        if (resolved_index < 0 ||
            static_cast<unsigned long long>(resolved_index) >= item_count) {
            throw std::runtime_error(
                make_error(
                    line_number,
                    index_type + " index is out of range: " + index_text
                )
            );
        }

        return static_cast<std::size_t>(resolved_index);
    }
};
