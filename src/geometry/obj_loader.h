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
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

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

    struct Point2D {
        double x;
        double y;
    };

    static double cross_2d(
        const Point2D& a,
        const Point2D& b,
        const Point2D& c
    ) {
        return
            (b.x - a.x) * (c.y - a.y) -
            (b.y - a.y) * (c.x - a.x);
    }

    static std::vector<std::array<std::size_t, 3>>
    triangulate_polygon(
        const std::vector<int>& vertex_indices,
        const std::vector<Vec3>& vertices
    ) {
        if (vertex_indices.size() < 3) {
            throw std::runtime_error(
                "OBJ polygon must contain at least three vertices."
            );
        }

        // Keep indices into the original polygon corners.
        // This is important because UVs and normals are per-corner.
        std::vector<std::size_t> polygon;

        polygon.reserve(vertex_indices.size());

        for (std::size_t i = 0;
             i < vertex_indices.size();
             ++i) {
            polygon.push_back(i);
        }

        // OBJ files sometimes repeat the first vertex at the end.
        if (vertex_indices.front() == vertex_indices.back()) {
            polygon.pop_back();
        }

        if (polygon.size() < 3) {
            throw std::runtime_error(
                "OBJ polygon is degenerate."
            );
        }

        // Reject consecutive duplicate vertex positions.
        for (std::size_t i = 0; i < polygon.size(); ++i) {
            const std::size_t current =
                static_cast<std::size_t>(
                    vertex_indices[polygon[i]]
                );

            const std::size_t next =
                static_cast<std::size_t>(
                    vertex_indices[
                        polygon[(i + 1) % polygon.size()]
                    ]
                );

            const Vec3& a = vertices[current];
            const Vec3& b = vertices[next];

            if (a.x == b.x &&
                a.y == b.y &&
                a.z == b.z) {
                throw std::runtime_error(
                    "OBJ polygon contains consecutive duplicate vertices."
                );
            }
        }

        // Calculate the Newell normal and the 3D bounding extent.
        Vec3 newell_normal(0.0, 0.0, 0.0);

        Vec3 minimum(
            std::numeric_limits<double>::max(),
            std::numeric_limits<double>::max(),
            std::numeric_limits<double>::max()
        );

        Vec3 maximum(
            -std::numeric_limits<double>::max(),
            -std::numeric_limits<double>::max(),
            -std::numeric_limits<double>::max()
        );

        for (std::size_t i = 0; i < polygon.size(); ++i) {
            const std::size_t current =
                static_cast<std::size_t>(
                    vertex_indices[polygon[i]]
                );

            const std::size_t next =
                static_cast<std::size_t>(
                    vertex_indices[
                        polygon[(i + 1) % polygon.size()]
                    ]
                );

            const Vec3& a = vertices[current];
            const Vec3& b = vertices[next];

            if (!std::isfinite(a.x) ||
                !std::isfinite(a.y) ||
                !std::isfinite(a.z)) {
                throw std::runtime_error(
                    "OBJ polygon contains a non-finite vertex."
                );
            }

            newell_normal.x +=
                (a.y - b.y) * (a.z + b.z);

            newell_normal.y +=
                (a.z - b.z) * (a.x + b.x);

            newell_normal.z +=
                (a.x - b.x) * (a.y + b.y);

            minimum.x = std::min(minimum.x, a.x);
            minimum.y = std::min(minimum.y, a.y);
            minimum.z = std::min(minimum.z, a.z);

            maximum.x = std::max(maximum.x, a.x);
            maximum.y = std::max(maximum.y, a.y);
            maximum.z = std::max(maximum.z, a.z);
        }

        const double extent_x = maximum.x - minimum.x;
        const double extent_y = maximum.y - minimum.y;
        const double extent_z = maximum.z - minimum.z;

        const double extent = std::max(
            extent_x,
            std::max(extent_y, extent_z)
        );

        const double normal_length_squared =
            newell_normal.length_squared();

        if (!std::isfinite(extent) ||
            extent <= 0.0 ||
            !std::isfinite(normal_length_squared) ||
            normal_length_squared <=
                extent * extent * extent * extent * 1e-24) {
            throw std::runtime_error(
                "OBJ polygon has a degenerate or invalid normal."
            );
        }

        // Drop the axis corresponding to the largest normal component.
        const double abs_x = std::abs(newell_normal.x);
        const double abs_y = std::abs(newell_normal.y);
        const double abs_z = std::abs(newell_normal.z);

        int dropped_axis = 0;

        if (abs_y > abs_x && abs_y >= abs_z) {
            dropped_axis = 1;
        } else if (abs_z > abs_x && abs_z > abs_y) {
            dropped_axis = 2;
        }

        std::vector<Point2D> projected;
        projected.reserve(polygon.size());

        for (const std::size_t corner : polygon) {
            const Vec3& vertex =
                vertices[
                    static_cast<std::size_t>(
                        vertex_indices[corner]
                    )
                ];

            if (dropped_axis == 0) {
                projected.push_back({
                    vertex.y,
                    vertex.z
                });
            } else if (dropped_axis == 1) {
                projected.push_back({
                    vertex.x,
                    vertex.z
                });
            } else {
                projected.push_back({
                    vertex.x,
                    vertex.y
                });
            }
        }

        double minimum_x = projected[0].x;
        double maximum_x = projected[0].x;
        double minimum_y = projected[0].y;
        double maximum_y = projected[0].y;

        for (const Point2D& point : projected) {
            minimum_x = std::min(minimum_x, point.x);
            maximum_x = std::max(maximum_x, point.x);
            minimum_y = std::min(minimum_y, point.y);
            maximum_y = std::max(maximum_y, point.y);
        }

        const double projected_extent = std::max(
            maximum_x - minimum_x,
            maximum_y - minimum_y
        );

        const double epsilon =
            projected_extent * projected_extent * 1e-12;

        if (!std::isfinite(epsilon) || epsilon <= 0.0) {
            throw std::runtime_error(
                "OBJ polygon projection is degenerate."
            );
        }

        auto sign_with_epsilon = [&](
            double value
        ) {
            if (value > epsilon) {
                return 1;
            }

            if (value < -epsilon) {
                return -1;
            }

            return 0;
        };

        auto on_segment = [&](
            const Point2D& a,
            const Point2D& b,
            const Point2D& point
        ) {
            return
                point.x >= std::min(a.x, b.x) - epsilon &&
                point.x <= std::max(a.x, b.x) + epsilon &&
                point.y >= std::min(a.y, b.y) - epsilon &&
                point.y <= std::max(a.y, b.y) + epsilon;
        };

        auto segments_intersect = [&](
            const Point2D& a,
            const Point2D& b,
            const Point2D& c,
            const Point2D& d
        ) {
            const double o1 = cross_2d(a, b, c);
            const double o2 = cross_2d(a, b, d);
            const double o3 = cross_2d(c, d, a);
            const double o4 = cross_2d(c, d, b);

            const int s1 = sign_with_epsilon(o1);
            const int s2 = sign_with_epsilon(o2);
            const int s3 = sign_with_epsilon(o3);
            const int s4 = sign_with_epsilon(o4);

            if (s1 * s2 < 0 && s3 * s4 < 0) {
                return true;
            }

            if (s1 == 0 && on_segment(a, b, c)) {
                return true;
            }

            if (s2 == 0 && on_segment(a, b, d)) {
                return true;
            }

            if (s3 == 0 && on_segment(c, d, a)) {
                return true;
            }

            if (s4 == 0 && on_segment(c, d, b)) {
                return true;
            }

            return false;
        };

        for (std::size_t i = 0; i < projected.size(); ++i) {
            const std::size_t i_next =
                (i + 1) % projected.size();

            for (std::size_t j = i + 1;
                 j < projected.size();
                 ++j) {
                const std::size_t j_next =
                    (j + 1) % projected.size();

                // Adjacent polygon edges are allowed to share
                // their common endpoint.
                if (i == j ||
                    i_next == j ||
                    j_next == i) {
                    continue;
                }

                if (segments_intersect(
                        projected[i],
                        projected[i_next],
                        projected[j],
                        projected[j_next])) {
                    throw std::runtime_error(
                        "OBJ polygon is self-intersecting."
                    );
                }
            }
        }

        // Signed area determines the original polygon winding.
        double signed_area = 0.0;

        for (std::size_t i = 0; i < projected.size(); ++i) {
            const Point2D& a = projected[i];
            const Point2D& b =
                projected[(i + 1) % projected.size()];

            signed_area +=
                a.x * b.y - b.x * a.y;
        }

        if (!std::isfinite(signed_area) ||
            std::abs(signed_area) <= epsilon) {
            throw std::runtime_error(
                "OBJ polygon has zero projected area."
            );
        }

        const double orientation =
            signed_area > 0.0 ? 1.0 : -1.0;

        auto point_inside_triangle = [&](
            const Point2D& point,
            const Point2D& a,
            const Point2D& b,
            const Point2D& c
        ) {
            const double ab =
                orientation * cross_2d(a, b, point);

            const double bc =
                orientation * cross_2d(b, c, point);

            const double ca =
                orientation * cross_2d(c, a, point);

            return ab >= -epsilon &&
                   bc >= -epsilon &&
                   ca >= -epsilon;
        };

        std::vector<std::array<std::size_t, 3>> triangles;
        triangles.reserve(polygon.size() - 2);

        // Ear clipping.
        while (polygon.size() > 3) {
            bool ear_found = false;

            for (std::size_t i = 0;
                 i < polygon.size();
                 ++i) {
                const std::size_t previous =
                    (i + polygon.size() - 1) % polygon.size();

                const std::size_t next =
                    (i + 1) % polygon.size();

                const std::size_t corner_a =
                    polygon[previous];

                const std::size_t corner_b =
                    polygon[i];

                const std::size_t corner_c =
                    polygon[next];

                const Point2D& a = projected[corner_a];
                const Point2D& b = projected[corner_b];
                const Point2D& c = projected[corner_c];

                // An ear must be strictly convex in the original winding.
                const double corner_cross =
                    orientation * cross_2d(a, b, c);

                if (corner_cross <= epsilon) {
                    continue;
                }

                // No other remaining vertex may lie inside or on
                // the candidate ear.
                bool contains_vertex = false;

                for (std::size_t j = 0;
                     j < polygon.size();
                     ++j) {
                    if (j == previous ||
                        j == i ||
                        j == next) {
                        continue;
                    }

                    const Point2D& point =
                        projected[polygon[j]];

                    if (point_inside_triangle(
                            point,
                            a,
                            b,
                            c)) {
                        contains_vertex = true;
                        break;
                    }
                }

                if (contains_vertex) {
                    continue;
                }

                // Preserve the original polygon winding.
                triangles.push_back({
                    corner_a,
                    corner_b,
                    corner_c
                });

                polygon.erase(
                    polygon.begin() +
                    static_cast<std::ptrdiff_t>(i)
                );

                ear_found = true;
                break;
            }

            if (!ear_found) {
                throw std::runtime_error(
                    "OBJ polygon cannot be triangulated. "
                    "It may be self-intersecting or degenerate."
                );
            }
        }

        // The final three vertices must also form a valid triangle.
        const double final_cross =
            orientation * cross_2d(
                projected[polygon[0]],
                projected[polygon[1]],
                projected[polygon[2]]
            );

        if (final_cross <= epsilon) {
            throw std::runtime_error(
                "OBJ polygon produced a degenerate final triangle."
            );
        }

        triangles.push_back({
            polygon[0],
            polygon[1],
            polygon[2]
        });

        return triangles;
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

                const auto triangles =
                    triangulate_polygon(
                        vertex_indices,
                        parsed.vertices
                    );

                for (const auto& triangle : triangles) {
                    const std::size_t corner_a = triangle[0];
                    const std::size_t corner_b = triangle[1];
                    const std::size_t corner_c = triangle[2];

                    parsed.faces.push_back({
                        static_cast<std::size_t>(
                            vertex_indices[corner_a]
                        ),
                        static_cast<std::size_t>(
                            vertex_indices[corner_b]
                        ),
                        static_cast<std::size_t>(
                            vertex_indices[corner_c]
                        )
                    });

                    const bool has_uv =
                        vertex_has_uv[corner_a] &&
                        vertex_has_uv[corner_b] &&
                        vertex_has_uv[corner_c];

                    parsed.face_has_uv.push_back(has_uv);

                    if (has_uv) {
                        parsed.face_texcoords.push_back({
                            static_cast<std::size_t>(
                                texcoord_indices[corner_a]
                            ),
                            static_cast<std::size_t>(
                                texcoord_indices[corner_b]
                            ),
                            static_cast<std::size_t>(
                                texcoord_indices[corner_c]
                            )
                        });
                    } else {
                        parsed.face_texcoords.push_back({
                            0, 0, 0
                        });
                    }

                    const bool has_normals =
                        vertex_has_normal[corner_a] &&
                        vertex_has_normal[corner_b] &&
                        vertex_has_normal[corner_c];

                    parsed.face_has_normals.push_back(has_normals);

                    parsed.face_corner_has_normals.push_back({
                        vertex_has_normal[corner_a],
                        vertex_has_normal[corner_b],
                        vertex_has_normal[corner_c]
                    });

                    parsed.face_normals.push_back({
                        vertex_has_normal[corner_a]
                            ? static_cast<std::size_t>(
                                normal_indices[corner_a]
                            )
                            : 0,
                        vertex_has_normal[corner_b]
                            ? static_cast<std::size_t>(
                                normal_indices[corner_b]
                            )
                            : 0,
                        vertex_has_normal[corner_c]
                            ? static_cast<std::size_t>(
                                normal_indices[corner_c]
                            )
                            : 0
                    });

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

                accumulated_normals[key] =
                    accumulated_normals[key] + weighted_normal;
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

        // Second pass: resolve every triangle corner independently.
        for (std::size_t face_index = 0;
             face_index < parsed.faces.size();
             ++face_index) {
            const std::string& group =
                parsed.face_smoothing_groups[face_index];

            const TriangleMesh::Face& face =
                parsed.faces[face_index];

            const auto& explicit_normals =
                parsed.face_corner_has_normals[face_index];

            const Vec3& a = parsed.vertices[face[0]];
            const Vec3& b = parsed.vertices[face[1]];
            const Vec3& c = parsed.vertices[face[2]];

            const Vec3 cross_normal =
                cross(b - a, c - a);

            const double cross_length_squared =
                cross_normal.length_squared();

            if (!std::isfinite(cross_length_squared) ||
                cross_length_squared < 1e-20) {
                throw std::runtime_error(
                    "Cannot generate normals for a degenerate OBJ face."
                );
            }

            const Vec3 geometric_normal =
                cross_normal / std::sqrt(cross_length_squared);

            TriangleMesh::FaceNormal resolved_indices = {
                0, 0, 0
            };

            bool has_any_shading_normal = false;

            for (std::size_t corner = 0;
                 corner < 3;
                 ++corner) {
                if (explicit_normals[corner]) {
                    resolved_indices[corner] =
                        parsed.face_normals[face_index][corner];

                    has_any_shading_normal = true;
                    continue;
                }

                Vec3 resolved_normal = geometric_normal;

                if (!group.empty()) {
                    const SmoothingKey key(
                        face[corner],
                        group
                    );

                    const auto it =
                        accumulated_normals.find(key);

                    if (it != accumulated_normals.end() &&
                        it->second.length_squared() >= 1e-20) {
                        resolved_normal = it->second;
                    }

                    has_any_shading_normal = true;
                }

                resolved_indices[corner] =
                    parsed.normals.size();

                parsed.normals.push_back(resolved_normal);
            }

            if (has_any_shading_normal) {
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