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
        const std::vector<std::shared_ptr<const Material>>& face_materials = {}
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

        triangles_.reserve(faces.size());
        cumulative_areas_.reserve(faces.size());

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

            auto triangle = std::make_shared<Triangle>(
                vertices[face[0]],
                vertices[face[1]],
                vertices[face[2]],
                triangle_material,
                uv_a,
                uv_b,
                uv_c,
                has_uv
            );

            const double triangle_area = triangle->area();

            if (!std::isfinite(triangle_area) ||
                triangle_area <= 0.0) {
                throw std::invalid_argument(
                    "TriangleMesh contains an invalid triangle."
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
        return bvh_->hit(ray, ray_t, record);
    }

    void all_intersections(
        const Ray& ray,
        const Interval& ray_t,
        std::vector<HitRecord>& intersections
    ) const override {
        for (const auto& triangle : triangles_) {
            HitRecord record;

            if (triangle->hit(ray, ray_t, record)) {
                intersections.push_back(record);
            }
        }

        std::sort(
            intersections.begin(),
            intersections.end(),
            [](const HitRecord& a, const HitRecord& b) {
                return a.t < b.t;
            }
        );

        std::vector<HitRecord> unique_intersections;
        unique_intersections.reserve(intersections.size());

        for (const HitRecord& record : intersections) {
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

        intersections = std::move(unique_intersections);
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

    std::size_t triangle_count() const {
        return triangles_.size();
    }

    double area() const {
        return total_area_;
    }

private:
    std::vector<std::shared_ptr<const Triangle>> triangles_;
    std::vector<double> cumulative_areas_;

    double total_area_ = 0.0;
    bool is_closed_volume_ = false;

    std::shared_ptr<const Hittable> bvh_;
};
