#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include <map>

#include "acceleration/bvh.h"
#include "geometry/hittable.h"
#include "geometry/triangle.h"

class TriangleMesh : public Hittable {
public:
    using Face = std::array<std::size_t, 3>;
    using FaceUV = std::array<std::size_t, 3>;
    using FaceNormal = std::array<std::size_t, 3>;

    static bool has_closed_topology(
        const std::vector<Face>& faces
    ) {
        struct EdgeUse {
            std::size_t count = 0;
            int orientation_sum = 0;
        };

        std::map<std::pair<std::size_t, std::size_t>, EdgeUse>
            edges;

        for (const Face& face : faces) {
            for (std::size_t i = 0; i < 3; ++i) {
                const std::size_t from = face[i];
                const std::size_t to = face[(i + 1) % 3];

                if (from == to) {
                    return false;
                }

                const bool forward = from < to;

                const auto key = forward
                    ? std::make_pair(from, to)
                    : std::make_pair(to, from);

                EdgeUse& use = edges[key];
                ++use.count;
                use.orientation_sum += forward ? 1 : -1;

                if (use.count > 2) {
                    return false;
                }
            }
        }

        for (const auto& entry : edges) {
            if (entry.second.count != 2 ||
                entry.second.orientation_sum != 0) {
                return false;
            }
        }

        return !edges.empty();
    }

    TriangleMesh(
        const std::vector<Vec3>& vertices,
        const std::vector<Face>& faces,
        std::shared_ptr<const Material> material = nullptr,
        const std::vector<Vec3>& texcoords = {},
        const std::vector<FaceUV>& face_texcoords = {},
        const std::vector<bool>& face_has_uv = {},
        const std::vector<std::shared_ptr<const Material>>& face_materials = {},
        const std::vector<Vec3>& normals = {},
        const std::vector<FaceNormal>& face_normals = {},
        const std::vector<bool>& face_has_normals = {}
    ) {
        if (vertices.empty()) {
            throw std::invalid_argument(
                "TriangleMesh requires vertices."
            );
        }

        if (faces.empty()) {
            throw std::invalid_argument(
                "TriangleMesh requires faces."
            );
        }

        if (!face_texcoords.empty() &&
            face_texcoords.size() != faces.size()) {
            throw std::invalid_argument(
                "TriangleMesh UV face count does not match face count."
            );
        }

        if (!face_normals.empty() &&
            face_normals.size() != faces.size()) {
            throw std::invalid_argument(
                "TriangleMesh normal face count does not match face count."
            );
        }

        if (!face_has_normals.empty() &&
            face_has_normals.size() != faces.size()) {
            throw std::invalid_argument(
                "TriangleMesh normal presence count does not match face count."
            );
        }

        if (!face_has_uv.empty() &&
            face_has_uv.size() != faces.size()) {
            throw std::invalid_argument(
                "TriangleMesh UV presence count does not match face count."
            );
        }

        if (!face_materials.empty() &&
            face_materials.size() != faces.size()) {
            throw std::invalid_argument(
                "TriangleMesh material face count does not match face count."
            );
        }

        is_closed_volume_ = has_closed_topology(faces);

        const auto get_face_material = [&](
            std::size_t face_index
        ) -> const Material* {
            const std::shared_ptr<const Material>& face_material =
                face_materials.empty()
                    ? material
                    : face_materials[face_index];

            return face_material.get();
        };

        bool has_medium_boundary = false;
        bool has_non_medium_boundary = false;

        double medium_ior = 1.0;
        Vec3 medium_absorption(0.0, 0.0, 0.0);
        bool has_reference_medium = false;

        const auto approximately_equal = [](
            double a,
            double b
        ) {
            const double scale = std::max({
                1.0,
                std::abs(a),
                std::abs(b)
            });

            return std::abs(a - b) <= 1e-10 * scale;
        };

        for (std::size_t i = 0; i < faces.size(); ++i) {
            const Material* face_material =
                get_face_material(i);

            const bool is_medium_boundary =
                face_material != nullptr &&
                face_material->is_medium_boundary();

            if (is_medium_boundary) {
                has_medium_boundary = true;

                const double ior =
                    face_material->index_of_refraction();

                const Vec3 absorption =
                    face_material->shadow_absorption_coefficient();

                if (!std::isfinite(ior) || ior <= 0.0 ||
                    !std::isfinite(absorption.x) ||
                    !std::isfinite(absorption.y) ||
                    !std::isfinite(absorption.z) ||
                    absorption.x < 0.0 ||
                    absorption.y < 0.0 ||
                    absorption.z < 0.0) {
                    throw std::invalid_argument(
                        "TriangleMesh has invalid dielectric medium properties."
                    );
                }

                if (!has_reference_medium) {
                    medium_ior = ior;
                    medium_absorption = absorption;
                    has_reference_medium = true;
                } else if (
                    !approximately_equal(ior, medium_ior) ||
                    !approximately_equal(
                        absorption.x,
                        medium_absorption.x
                    ) ||
                    !approximately_equal(
                        absorption.y,
                        medium_absorption.y
                    ) ||
                    !approximately_equal(
                        absorption.z,
                        medium_absorption.z
                    )
                ) {
                    throw std::invalid_argument(
                        "All dielectric faces in a TriangleMesh must use identical IOR and absorption."
                    );
                }
            } else {
                has_non_medium_boundary = true;
            }
        }

        if (has_medium_boundary && has_non_medium_boundary) {
            throw std::invalid_argument(
                "TriangleMesh cannot mix dielectric medium boundaries with ordinary surface materials."
            );
        }

        if (has_medium_boundary && !is_closed_volume_) {
            throw std::invalid_argument(
                "An open TriangleMesh cannot be used as a dielectric medium boundary."
            );
        }

        triangles_.reserve(faces.size());
        cumulative_areas_.reserve(faces.size());

        emissive_triangle_indices_.reserve(faces.size());
        emissive_cumulative_areas_.reserve(faces.size());

        double accumulated_area = 0.0;

        for (std::size_t i = 0; i < faces.size(); ++i) {
            const Face& face = faces[i];

            for (const std::size_t index : face) {
                if (index >= vertices.size()) {
                    throw std::out_of_range(
                        "TriangleMesh face index is out of range."
                    );
                }
            }

            const bool has_uv = face_has_uv.empty()
                ? !face_texcoords.empty()
                : face_has_uv[i];

            Vec3 uv_a(0.0, 0.0, 0.0);
            Vec3 uv_b(1.0, 0.0, 0.0);
            Vec3 uv_c(0.0, 1.0, 0.0);

            if (has_uv) {
                if (face_texcoords.empty()) {
                    throw std::invalid_argument(
                        "TriangleMesh face has UVs but no UV indices were provided."
                    );
                }

                const FaceUV& uv_face = face_texcoords[i];

                for (const std::size_t index : uv_face) {
                    if (index >= texcoords.size()) {
                        throw std::out_of_range(
                            "TriangleMesh UV index is out of range."
                        );
                    }
                }

                uv_a = texcoords[uv_face[0]];
                uv_b = texcoords[uv_face[1]];
                uv_c = texcoords[uv_face[2]];
            }

            const std::shared_ptr<const Material> triangle_material =
                face_materials.empty()
                    ? material
                    : face_materials[i];

            const bool has_normals = face_has_normals.empty()
                ? !face_normals.empty()
                : face_has_normals[i];

            Vec3 normal_a;
            Vec3 normal_b;
            Vec3 normal_c;

            if (has_normals) {
                if (face_normals.empty()) {
                    throw std::invalid_argument(
                        "TriangleMesh face has normals but no normal indices were provided."
                    );
                }

                const FaceNormal& normal_face = face_normals[i];

                for (const std::size_t index : normal_face) {
                    if (index >= normals.size()) {
                        throw std::out_of_range(
                            "TriangleMesh normal index is out of range."
                        );
                    }
                }

                normal_a = normals[normal_face[0]];
                normal_b = normals[normal_face[1]];
                normal_c = normals[normal_face[2]];
            }

            auto triangle = std::make_shared<Triangle>(
                vertices[face[0]],
                vertices[face[1]],
                vertices[face[2]],
                triangle_material,
                uv_a,
                uv_b,
                uv_c,
                has_uv,
                normal_a,
                normal_b,
                normal_c,
                has_normals
            );

            const double triangle_area = triangle->area();

            if (!std::isfinite(triangle_area) ||
                triangle_area <= 0.0) {
                throw std::invalid_argument(
                    "TriangleMesh contains an invalid triangle."
                );
            }

            if (triangle_material &&
                triangle_material->is_emissive()) {
                emissive_total_area_ += triangle_area;

                if (!std::isfinite(emissive_total_area_)) {
                    throw std::overflow_error(
                        "TriangleMesh emissive area is not finite."
                    );
                }

                emissive_triangle_indices_.push_back(i);

                emissive_cumulative_areas_.push_back(
                    emissive_total_area_
                );
            }

            accumulated_area += triangle_area;

            if (!std::isfinite(accumulated_area)) {
                throw std::overflow_error(
                    "TriangleMesh total area is not finite."
                );
            }

            triangles_.push_back(std::move(triangle));
            cumulative_areas_.push_back(accumulated_area);
        }

        total_area_ = accumulated_area;

        if (is_closed_volume_) {
            Vec3 bounds_min = vertices.front();
            Vec3 bounds_max = vertices.front();

            for (const Vec3& vertex : vertices) {
                bounds_min = Vec3(
                    std::min(bounds_min.x, vertex.x),
                    std::min(bounds_min.y, vertex.y),
                    std::min(bounds_min.z, vertex.z)
                );

                bounds_max = Vec3(
                    std::max(bounds_max.x, vertex.x),
                    std::max(bounds_max.y, vertex.y),
                    std::max(bounds_max.z, vertex.z)
                );
            }

            const Vec3 extent = bounds_max - bounds_min;
            const double scale = std::max({
                extent.x,
                extent.y,
                extent.z
            });

            if (!std::isfinite(scale) || scale <= 0.0) {
                throw std::invalid_argument(
                    "Closed TriangleMesh has invalid bounds."
                );
            }

            double signed_volume_six = 0.0;

            for (const Face& face : faces) {
                const Vec3& v0 = vertices[face[0]];
                const Vec3& v1 = vertices[face[1]];
                const Vec3& v2 = vertices[face[2]];

                signed_volume_six +=
                    dot(v0, cross(v1, v2));
            }

            const double volume_tolerance =
                1e-12 * scale * scale * scale * 6.0;

            if (!std::isfinite(signed_volume_six) ||
                signed_volume_six <= volume_tolerance) {
                throw std::invalid_argument(
                    "Closed TriangleMesh must have outward-facing winding."
                );
            }
        }

        // Reuse the project's existing SAH BVH implementation.
        std::vector<std::shared_ptr<const Hittable>> objects;
        objects.reserve(triangles_.size());

        for (const auto& triangle : triangles_) {
            objects.push_back(triangle);
        }

        bvh_ = std::make_shared<BVHNode>(std::move(objects));
    }

    bool hit(
        const Ray& ray,
        const Interval& ray_t,
        HitRecord& record
    ) const override {
        HitRecord mesh_record;

        if (!bvh_->hit(ray, ray_t, mesh_record)) {
            return false;
        }

        if (is_closed_volume_) {
            mesh_record.medium_boundary = this;
        }

        record = mesh_record;
        return true;
    }

    void all_intersections(
        const Ray& ray,
        const Interval& ray_t,
        std::vector<HitRecord>& intersections
    ) const override {
        std::vector<HitRecord> mesh_intersections;

        bvh_->all_intersections(
            ray,
            ray_t,
            mesh_intersections
        );

        if (is_closed_volume_) {
            for (HitRecord& record : mesh_intersections) {
                record.medium_boundary = this;
            }
        }

        std::sort(
            mesh_intersections.begin(),
            mesh_intersections.end(),
            [](const HitRecord& a, const HitRecord& b) {
                return a.t < b.t;
            }
        );

        std::vector<HitRecord> unique_intersections;
        unique_intersections.reserve(mesh_intersections.size());

        for (const HitRecord& record : mesh_intersections) {
            if (unique_intersections.empty()) {
                unique_intersections.push_back(record);
                continue;
            }

            const double previous_t =
                unique_intersections.back().t;

            const double tolerance =
                1e-7 * std::max(1.0, std::abs(record.t));

            if (std::abs(record.t - previous_t) > tolerance) {
                unique_intersections.push_back(record);
            }
        }

        intersections.insert(
            intersections.end(),
            unique_intersections.begin(),
            unique_intersections.end()
        );
    }

    bool is_closed_volume() const override {
        return is_closed_volume_;
    }

    AABB bounding_box() const override {
        return bvh_->bounding_box();
    }

    // Uniformly sample the entire mesh surface by area.
    bool sample_surface(
        double u,
        double v,
        HitRecord& sample,
        double& pdf_area
    ) const override {
        pdf_area = 0.0;

        if (!std::isfinite(u) ||
            !std::isfinite(v) ||
            u < 0.0 || u >= 1.0 ||
            v < 0.0 || v >= 1.0) {
            return false;
        }

        // Select a triangle proportionally to its area.
        const double target = u * total_area_;

        auto it = std::upper_bound(
            cumulative_areas_.begin(),
            cumulative_areas_.end(),
            target
        );

        std::size_t index = static_cast<std::size_t>(
            it - cumulative_areas_.begin()
        );

        if (index >= triangles_.size()) {
            index = triangles_.size() - 1;
        }

        const double previous_area =
            index == 0 ? 0.0 : cumulative_areas_[index - 1];

        const double triangle_area =
            cumulative_areas_[index] - previous_area;

        double local_u = (target - previous_area) / triangle_area;

        local_u = std::clamp(
            local_u,
            0.0,
            std::nextafter(1.0, 0.0)
        );

        if (!triangles_[index]->sample_surface(
                local_u,
                v,
                sample,
                pdf_area
            )) {
            pdf_area = 0.0;
            return false;
        }

        // The density is with respect to the whole mesh area.
        pdf_area = 1.0 / total_area_;

        return true;
    }

    bool sample_light_surface(
        double u,
        double v,
        HitRecord& sample,
        double& pdf_area
    ) const override {
        pdf_area = 0.0;

        if (!std::isfinite(u) ||
            !std::isfinite(v) ||
            u < 0.0 || u >= 1.0 ||
            v < 0.0 || v >= 1.0 ||
            emissive_triangle_indices_.empty() ||
            emissive_total_area_ <= 0.0) {
            return false;
        }

        const double target =
            u * emissive_total_area_;

        const auto it = std::upper_bound(
            emissive_cumulative_areas_.begin(),
            emissive_cumulative_areas_.end(),
            target
        );

        std::size_t emissive_index =
            static_cast<std::size_t>(
                it - emissive_cumulative_areas_.begin()
            );

        if (emissive_index >= emissive_triangle_indices_.size()) {
            emissive_index =
                emissive_triangle_indices_.size() - 1;
        }

        const double previous_area =
            emissive_index == 0
                ? 0.0
                : emissive_cumulative_areas_[emissive_index - 1];

        const double triangle_area =
            emissive_cumulative_areas_[emissive_index]
            - previous_area;

        double local_u =
            (target - previous_area) / triangle_area;

        local_u = std::clamp(
            local_u,
            0.0,
            std::nextafter(1.0, 0.0)
        );

        const std::size_t triangle_index =
            emissive_triangle_indices_[emissive_index];

        if (!triangles_[triangle_index]->sample_surface(
                local_u,
                v,
                sample,
                pdf_area)) {
            pdf_area = 0.0;
            return false;
        }

        pdf_area = 1.0 / emissive_total_area_;

        return true;
    }

    bool has_emissive_surface() const override {
        return !emissive_triangle_indices_.empty();
    }

    double light_surface_pdf(
        const HitRecord& record
    ) const override {
        if (emissive_total_area_ <= 0.0 ||
            !record.material ||
            !record.material->is_emissive()) {
            return 0.0;
        }

        return 1.0 / emissive_total_area_;
    }

    std::size_t triangle_count() const {
        return triangles_.size();
    }

    double area() const {
        return total_area_;
    }

private:
    std::vector<std::shared_ptr<const Triangle>> triangles_;
    std::vector<double> cumulative_areas_;

    std::vector<std::size_t> emissive_triangle_indices_;
    std::vector<double> emissive_cumulative_areas_;

    double total_area_ = 0.0;
    double emissive_total_area_ = 0.0;
    bool is_closed_volume_ = false;

    std::shared_ptr<const Hittable> bvh_;
};
