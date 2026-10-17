#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "acceleration/bvh.h"
#include "geometry/hittable.h"
#include "geometry/triangle.h"

class TriangleMesh : public Hittable {
public:
    using Face = std::array<std::size_t, 3>;

    TriangleMesh(
        const std::vector<Vec3>& vertices,
        const std::vector<Face>& faces,
        std::shared_ptr<const Material> material = nullptr
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

        triangles_.reserve(faces.size());
        cumulative_areas_.reserve(faces.size());

        double accumulated_area = 0.0;

        for (const Face& face : faces) {
            for (const std::size_t index : face) {
                if (index >= vertices.size()) {
                    throw std::out_of_range(
                        "TriangleMesh face index is out of range."
                    );
                }
            }

            auto triangle = std::make_shared<Triangle>(
                vertices[face[0]],
                vertices[face[1]],
                vertices[face[2]],
                material
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

        // Remap the selection interval to [0, 1) for sampling
        // a point uniformly inside the selected triangle.
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

    std::shared_ptr<const Hittable> bvh_;
};
