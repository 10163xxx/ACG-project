#pragma once

#include <cmath>
#include <cstddef>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits>
#include <utility>

#include "geometry/mesh.h"

class ObjLoader {
public:
    // Load OBJ geometry from a stream.
    // OBJ texture coordinates and normals are currently ignored.
    static TriangleMesh load(
        std::istream& input,
        std::shared_ptr<const Material> material = nullptr
    ) {
        std::vector<Vec3> vertices;
        std::vector<TriangleMesh::Face> faces;

        std::string line;
        std::size_t line_number = 0;

        while (std::getline(input, line)) {
            ++line_number;

            // Remove comments.
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
                        make_error(
                            line_number,
                            "Invalid vertex position."
                        )
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
            } else if (keyword == "f") {
                std::vector<std::size_t> polygon;
                std::string token;

                while (line_stream >> token) {
                    const std::size_t vertex_index =
                        parse_vertex_index(
                            token,
                            vertices.size(),
                            line_number
                        );

                    polygon.push_back(vertex_index);
                }

                if (polygon.size() < 3) {
                    throw std::runtime_error(
                        make_error(
                            line_number,
                            "A face must contain at least 3 vertices."
                        )
                    );
                }

                // Triangulate a polygon as a triangle fan:
                // (0, 1, 2), (0, 2, 3), ...
                for (std::size_t i = 1;
                     i + 1 < polygon.size();
                     ++i) {
                    faces.push_back({
                        polygon[0],
                        polygon[i],
                        polygon[i + 1]
                    });
                }
            }

            // Other OBJ statements, including vt, vn, o, g,
            // s, mtllib and usemtl, are currently ignored.
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
                std::move(material)
            );
        } catch (const std::exception& exception) {
            throw std::runtime_error(
                std::string("Invalid OBJ mesh: ") +
                exception.what()
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

    static std::size_t parse_vertex_index(
        const std::string& token,
        std::size_t vertex_count,
        std::size_t line_number
    ) {
        // Face tokens may look like:
        // v, v/vt, v//vn, or v/vt/vn.
        // Only the vertex position index is needed here.
        const std::size_t slash_pos = token.find('/');

        const std::string index_text =
            token.substr(0, slash_pos);

        if (index_text.empty()) {
            throw std::runtime_error(
                make_error(
                    line_number,
                    "Face token has no vertex index: " + token
                )
            );
        }

        long long obj_index = 0;

        try {
            std::size_t parsed_chars = 0;

            obj_index = std::stoll(
                index_text,
                &parsed_chars
            );

            if (parsed_chars != index_text.size()) {
                throw std::runtime_error(
                    "Invalid vertex index."
                );
            }
        } catch (const std::exception&) {
            throw std::runtime_error(
                make_error(
                    line_number,
                    "Invalid vertex index: " + token
                )
            );
        }

        if (obj_index == 0) {
            throw std::runtime_error(
                make_error(
                    line_number,
                    "OBJ vertex indices cannot be zero."
                )
            );
        }

        // OBJ indices are 1-based when positive.
        // Negative indices are relative to the most recently
        // defined vertex.
        long long resolved_index = 0;

        if (obj_index > 0) {
            resolved_index = obj_index - 1;
        } else {
            if (vertex_count >
                static_cast<std::size_t>(
                    std::numeric_limits<long long>::max()
                )) {
                throw std::runtime_error(
                    make_error(
                        line_number,
                        "Too many vertices to resolve OBJ index."
                    )
                );
            }

            resolved_index =
                static_cast<long long>(vertex_count) +
                obj_index;
        }

        if (resolved_index < 0 ||
            static_cast<unsigned long long>(resolved_index) >=
                vertex_count) {
            throw std::runtime_error(
                make_error(
                    line_number,
                    "Vertex index is out of range: " + token
                )
            );
        }

        return static_cast<std::size_t>(resolved_index);
    }
};
