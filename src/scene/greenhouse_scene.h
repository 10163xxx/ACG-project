#pragma once

#include <memory>
#include <array>
#include <cmath>
#include <vector>
#include <string>
#include <stdexcept>

#include "geometry/mesh.h"
#include "geometry/hittable_list.h"
#include "geometry/quad.h"
#include "geometry/sphere.h"
#include "geometry/obj_loader.h"
#include "materials/dielectric.h"
#include "materials/diffuse_light.h"
#include "materials/lambertian.h"
#include "materials/principled.h"
#include "materials/procedural_texture.h"
#include "materials/thin_glass.h"

inline const AABB& greenhouse_plant_bounds() {
    static const AABB bounds(
        Vec3(-3.55, 0.0, -7.65),
        Vec3(3.55, 4.0, 3.65)
    );

    return bounds;
}

inline void greenhouse_validate_plant_bounds(
    const AABB& bounds,
    const std::string& plant_name
) {
    constexpr double EPSILON = 1e-6;

    const AABB& allowed = greenhouse_plant_bounds();

    if (bounds.minimum.x < allowed.minimum.x - EPSILON ||
        bounds.minimum.y < allowed.minimum.y - EPSILON ||
        bounds.minimum.z < allowed.minimum.z - EPSILON ||
        bounds.maximum.x > allowed.maximum.x + EPSILON ||
        bounds.maximum.y > allowed.maximum.y + EPSILON ||
        bounds.maximum.z > allowed.maximum.z + EPSILON) {
        throw std::runtime_error(
            "Greenhouse plant is outside the interior bounds: " +
            plant_name
        );
    }

    const double maximum_abs_x = std::max(
        std::abs(bounds.minimum.x),
        std::abs(bounds.maximum.x)
    );

    const double roof_height =
        5.98 - 0.485 * maximum_abs_x;

    if (bounds.maximum.y > roof_height - 0.12 + EPSILON) {
        throw std::runtime_error(
            "Greenhouse plant intersects the roof clearance: " +
            plant_name
        );
    }
}

inline TriangleMesh greenhouse_load_plant_obj(
    const std::string& filename,
    std::shared_ptr<const Material> fallback_material,
    double target_height,
    const Vec3& base_position,
    const std::vector<std::string>& object_names = {},
    double target_width = 0.0
) {
    TriangleMesh mesh = ObjLoader::load_file(
        filename,
        std::move(fallback_material),
        target_height,
        base_position,
        object_names,
        target_width,
        &greenhouse_plant_bounds()
    );

    return mesh;
}

inline void greenhouse_box(
    HittableList& world,
    const Vec3& lo,
    const Vec3& hi,
    const std::shared_ptr<const Material>& material
) {
    const double x = hi.x - lo.x;
    const double y = hi.y - lo.y;
    const double z = hi.z - lo.z;

    world.add(std::make_shared<Quad>(
        Vec3(lo.x, lo.y, hi.z), Vec3(x, 0, 0), Vec3(0, y, 0), material));
    world.add(std::make_shared<Quad>(
        Vec3(hi.x, lo.y, lo.z), Vec3(-x, 0, 0), Vec3(0, y, 0), material));
    world.add(std::make_shared<Quad>(
        Vec3(lo.x, lo.y, lo.z), Vec3(0, 0, z), Vec3(0, y, 0), material));
    world.add(std::make_shared<Quad>(
        Vec3(hi.x, lo.y, hi.z), Vec3(0, 0, -z), Vec3(0, y, 0), material));
    world.add(std::make_shared<Quad>(
        Vec3(lo.x, hi.y, hi.z), Vec3(x, 0, 0), Vec3(0, 0, -z), material));
    world.add(std::make_shared<Quad>(
        Vec3(lo.x, lo.y, lo.z), Vec3(x, 0, 0), Vec3(0, 0, z), material));
}

inline void greenhouse_add_stone_slab_path(
    HittableList& world,
    const std::vector<std::shared_ptr<const Material>>& stone_materials,
    const std::shared_ptr<const Material>& joint_material,
    double path_center_x,
    double first_row_z,
    int rows,
    int columns,
    double column_offset,
    double row_spacing,
    double half_width,
    double half_length,
    double top_y,
    int seed
) {
    constexpr int SIDES = 6;
    constexpr double BOTTOM_Y = 0.008;
    constexpr double PI = 3.14159265358979323846;

    std::vector<Vec3> vertices;
    std::vector<TriangleMesh::Face> faces;

    const int slab_count = rows * columns;

    vertices.reserve(
        static_cast<std::size_t>(slab_count)
        * (2 * SIDES + 2)
    );

    faces.reserve(
        static_cast<std::size_t>(slab_count)
        * (4 * SIDES)
    );

    const auto variation = [&](
        int row,
        int column,
        int index
    ) {
        const double value =
            std::sin(
                (seed + row) * 12.9898
                + (seed * 2 + column) * 78.233
                + index * 37.719
            ) * 43758.5453;

        return value - std::floor(value);
    };

    // Dark substrate remains visible through the stone joints.
    const double joint_first_z =
        first_row_z + half_length + 0.06;

    const double joint_last_z =
        first_row_z - (rows - 1) * row_spacing
        - half_length - 0.06;

    const double joint_half_width =
        columns > 1
            ? column_offset + half_width + 0.09
            : half_width + 0.09;

    world.add(std::make_shared<Quad>(
        Vec3(
            path_center_x - joint_half_width,
            0.003,
            joint_first_z
        ),
        Vec3(2.0 * joint_half_width, 0.0, 0.0),
        Vec3(0.0, 0.0, joint_last_z - joint_first_z),
        joint_material
    ));

    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const double row_shift =
                (row % 2 == 0)
                    ? 0.0
                    : (variation(row, column, 9) - 0.5) * 0.14;

            const double center_x =
                path_center_x
                + (
                    (columns == 1)
                        ? (variation(row, column, 10) - 0.5) * 0.30
                        : (
                            (column % 2 == 0 ? -1.0 : 1.0)
                            * column_offset
                            * (0.88 + 0.24 * variation(row, column, 11))
                          )
                    )
                + row_shift;

            const double center_z =
                first_row_z
                - static_cast<double>(row) * row_spacing
                + (variation(row, column, 12) - 0.5) * 0.22;

            // Flagstone-like variety: strongly uneven width, length
            // and rotation so no two slabs match.
            const double width_variation =
                0.82 + 0.36 * variation(row, column, 0);

            const double length_variation =
                0.82 + 0.36 * variation(row, column, 1);

            const double rotation =
                (variation(row, column, 2) - 0.5) * 0.48;

            const double cos_rotation = std::cos(rotation);
            const double sin_rotation = std::sin(rotation);

            const std::size_t base =
                vertices.size();

            vertices.emplace_back(
                center_x,
                BOTTOM_Y,
                center_z
            );

            vertices.emplace_back(
                center_x,
                top_y + 0.018 * (variation(row, column, 3) - 0.5),
                center_z
            );

            for (int side = 0; side < SIDES; ++side) {
                const double angle =
                    -2.0 * PI * side / SIDES;

                const double corner_x =
                    std::cos(angle)
                    * half_width
                    * width_variation;

                const double corner_z =
                    std::sin(angle)
                    * half_length
                    * length_variation;

                const double local_x =
                    corner_x * cos_rotation
                    - corner_z * sin_rotation;

                const double local_z =
                    corner_x * sin_rotation
                    + corner_z * cos_rotation;

                const double edge_variation =
                    0.82 + 0.30 * variation(
                        row,
                        column,
                        side + 4
                    );

                const double x =
                    center_x + local_x * edge_variation;

                const double z =
                    center_z + local_z * edge_variation;

                const double top_offset =
                    0.014 * (
                        variation(row, column, side + 12) - 0.5
                    );

                vertices.emplace_back(
                    x,
                    BOTTOM_Y,
                    z
                );

                vertices.emplace_back(
                    x,
                    top_y + top_offset,
                    z
                );
            }

            const std::size_t bottom_center = base;
            const std::size_t top_center = base + 1;

            for (int side = 0; side < SIDES; ++side) {
                const int next = (side + 1) % SIDES;

                const std::size_t bottom_a =
                    base + 2 + static_cast<std::size_t>(side * 2);

                const std::size_t top_a =
                    bottom_a + 1;

                const std::size_t bottom_b =
                    base + 2 + static_cast<std::size_t>(next * 2);

                const std::size_t top_b =
                    bottom_b + 1;

                // Top surface.
                faces.push_back({
                    top_center,
                    top_a,
                    top_b
                });

                // Bottom surface.
                faces.push_back({
                    bottom_center,
                    bottom_b,
                    bottom_a
                });

                // Two triangles close each side wall.
                faces.push_back({
                    top_b,
                    bottom_b,
                    bottom_a
                });

                faces.push_back({
                    top_b,
                    bottom_a,
                    top_a
                });
            }
        }
    }

    world.add(std::make_shared<TriangleMesh>(
        vertices,
        faces,
        stone_materials.empty()
            ? nullptr
            : stone_materials[static_cast<std::size_t>(
                (seed + 7) % static_cast<int>(stone_materials.size())
            )]
    ));
}

inline void greenhouse_add_leaf(
    HittableList& world,
    const Vec3& base,
    const Vec3& direction,
    double width,
    double curvature,
    const std::shared_ptr<const Material>& material,
    bool validate_bounds = true
) {
    constexpr int SEGMENTS = 6;

    const double horizontal_length = std::sqrt(
        direction.x * direction.x +
        direction.z * direction.z
    );

    Vec3 side;

    if (horizontal_length > 1e-8) {
        side = Vec3(
            -direction.z / horizontal_length,
            0.0,
            direction.x / horizontal_length
        );
    } else {
        side = Vec3(1.0, 0.0, 0.0);
    }

    std::vector<Vec3> vertices;
    std::vector<TriangleMesh::Face> faces;

    vertices.reserve((SEGMENTS + 1) * 3);
    faces.reserve(SEGMENTS * 4);

    for (int i = 0; i <= SEGMENTS; ++i) {
        const double t =
            static_cast<double>(i) / SEGMENTS;

        const double profile =
            0.04 + 0.96 * std::pow(
                std::sin(3.14159265358979323846 * t),
                0.85
            );

        const double local_width =
            width * profile;

        const Vec3 center =
            base
            + t * direction
            + Vec3(
                0.0,
                curvature * std::sin(3.14159265358979323846 * t),
                0.0
            );

        vertices.push_back(
            center - local_width * side
        );

        vertices.push_back(
            center + Vec3(
                0.0,
                0.045 * profile,
                0.0
            )
        );

        vertices.push_back(
            center + local_width * side
        );
    }

    for (int i = 0; i < SEGMENTS; ++i) {
        const std::size_t a =
            static_cast<std::size_t>(i * 3);

        const std::size_t b = a + 3;

        faces.push_back({a, b, a + 1});
        faces.push_back({a + 1, b, b + 1});
        faces.push_back({a + 1, b + 1, a + 2});
        faces.push_back({a + 2, b + 1, b + 2});
    }

    Vec3 bounds_min = vertices.front();
    Vec3 bounds_max = vertices.front();

    for (const Vec3& vertex : vertices) {
        bounds_min.x = std::min(bounds_min.x, vertex.x);
        bounds_min.y = std::min(bounds_min.y, vertex.y);
        bounds_min.z = std::min(bounds_min.z, vertex.z);

        bounds_max.x = std::max(bounds_max.x, vertex.x);
        bounds_max.y = std::max(bounds_max.y, vertex.y);
        bounds_max.z = std::max(bounds_max.z, vertex.z);
    }

    if (validate_bounds) {
        greenhouse_validate_plant_bounds(
            AABB(bounds_min, bounds_max),
            "procedural leaf"
        );
    }

    world.add(std::make_shared<TriangleMesh>(
        vertices,
        faces,
        material
    ));
}

inline void greenhouse_add_plant(
    HittableList& world,
    double x,
    double z,
    double radius,
    double height,
    int leaf_count,
    const std::shared_ptr<const Material>& leaf_a,
    const std::shared_ptr<const Material>& leaf_b,
    const std::shared_ptr<const Material>& pot_material
) {
    const double pot_width = radius * 1.35;
    const double pot_height = 0.42;

    const double maximum_leaf_radius =
        radius * 1.18 + radius * 0.22;

    const double maximum_leaf_height =
        pot_height + 0.035 + height * 1.08
        + height * 0.10 + height * 0.06;

    greenhouse_validate_plant_bounds(
        AABB(
            Vec3(
                x - maximum_leaf_radius,
                0.0,
                z - maximum_leaf_radius
            ),
            Vec3(
                x + maximum_leaf_radius,
                maximum_leaf_height,
                z + maximum_leaf_radius
            )
        ),
        "procedural plant"
    );

    greenhouse_box(
        world,
        Vec3(x - pot_width, 0.0, z - pot_width),
        Vec3(x + pot_width, pot_height, z + pot_width),
        pot_material
    );

    auto soil = std::make_shared<Lambertian>(
        Vec3(0.095, 0.065, 0.035)
    );

    world.add(std::make_shared<Quad>(
        Vec3(x - pot_width * 0.92, pot_height + 0.005,
            z - pot_width * 0.92),
        Vec3(pot_width * 1.84, 0.0, 0.0),
        Vec3(0.0, 0.0, pot_width * 1.84),
        soil
    ));

    const Vec3 base(
        x,
        pot_height + 0.035,
        z
    );

    for (int i = 0; i < leaf_count; ++i) {
        const double angle =
            6.28318530717958647692
            * static_cast<double>(i)
            / leaf_count;

        const double horizontal =
            radius * (0.65 + 0.18 * (i % 3));

        const double leaf_height =
            height * (0.68 + 0.08 * (i % 4));

        const Vec3 direction(
            horizontal * std::cos(angle),
            leaf_height,
            horizontal * std::sin(angle)
        );

        greenhouse_add_leaf(
            world,
            base,
            direction,
            radius * (0.34 + 0.035 * (i % 3)),
            height * 0.10,
            (i % 3 == 0) ? leaf_b : leaf_a
        );
    }

    // A few upright leaves give the plant a fuller silhouette.
    for (int i = 0; i < 2; ++i) {
        const double angle =
            3.14159265358979323846 * i + 0.6;

        const Vec3 direction(
            radius * 0.22 * std::cos(angle),
            height * (0.92 + 0.08 * i),
            radius * 0.22 * std::sin(angle)
        );

        greenhouse_add_leaf(
            world,
            base,
            direction,
            radius * 0.22,
            height * 0.06,
            leaf_b
        );
    }
}

inline void greenhouse_add_tapered_pot(
    HittableList& world,
    double x,
    double z,
    double bottom_half_width,
    double top_half_width,
    double height,
    const std::shared_ptr<const Material>& material,
    double base_y = 0.0
) {
    const double y0 = base_y;
    const double y1 = base_y + height;

    const Vec3 p0(
        x - bottom_half_width, y0, z - bottom_half_width);
    const Vec3 p1(
        x + bottom_half_width, y0, z - bottom_half_width);
    const Vec3 p2(
        x + bottom_half_width, y0, z + bottom_half_width);
    const Vec3 p3(
        x - bottom_half_width, y0, z + bottom_half_width);

    const Vec3 q0(
        x - top_half_width, y1, z - top_half_width);
    const Vec3 q1(
        x + top_half_width, y1, z - top_half_width);
    const Vec3 q2(
        x + top_half_width, y1, z + top_half_width);
    const Vec3 q3(
        x - top_half_width, y1, z + top_half_width);

    world.add(std::make_shared<Quad>(
        p0, p1 - p0, q0 - p0, material));

    world.add(std::make_shared<Quad>(
        p1, p2 - p1, q1 - p1, material));

    world.add(std::make_shared<Quad>(
        p2, p3 - p2, q2 - p2, material));

    world.add(std::make_shared<Quad>(
        p3, p0 - p3, q3 - p3, material));

    // Four separate rim bars leave the planter opening unobstructed.
    const double rim_height = 0.055;
    const double rim_width = 0.045;
    const double outer = top_half_width + rim_width;
    const double inner = top_half_width;

    greenhouse_validate_plant_bounds(
        AABB(
            Vec3(
                x - outer,
                y0,
                z - outer
            ),
            Vec3(
                x + outer,
                y1,
                z + outer
            )
        ),
        "procedural planter"
    );

    greenhouse_box(
        world,
        Vec3(x - outer, y1 - rim_height, z - outer),
        Vec3(x + outer, y1, z - inner),
        material
    );

    greenhouse_box(
        world,
        Vec3(x - outer, y1 - rim_height, z + inner),
        Vec3(x + outer, y1, z + outer),
        material
    );

    greenhouse_box(
        world,
        Vec3(x - outer, y1 - rim_height, z - inner),
        Vec3(x - inner, y1, z + inner),
        material
    );

    greenhouse_box(
        world,
        Vec3(x + inner, y1 - rim_height, z - inner),
        Vec3(x + outer, y1, z + inner),
        material
    );
}

inline void greenhouse_add_hanging_basket(
    HittableList& world,
    double x,
    double z,
    double chain_top_y,
    const std::shared_ptr<const Material>& basket_material,
    const std::shared_ptr<const Material>& chain_material,
    const std::shared_ptr<const Material>& soil_material,
    const std::shared_ptr<const Material>& vine_a,
    const std::shared_ptr<const Material>& vine_b,
    int seed
) {
    constexpr double PI = 3.14159265358979323846;

    const double top_half = 0.21;
    const double bottom_half = 0.13;
    const double basket_height = 0.20;

    const double rim_y = chain_top_y - 0.34;
    const double bottom_y = rim_y - basket_height;

    greenhouse_validate_plant_bounds(
        AABB(
            Vec3(
                x - top_half - 0.35,
                bottom_y - 0.60,
                z - top_half - 0.35
            ),
            Vec3(
                x + top_half + 0.35,
                chain_top_y,
                z + top_half + 0.35
            )
        ),
        "hanging basket"
    );

    const auto variation = [seed](int index) {
        const double value = std::sin(
            seed * 12.9898 + index * 78.233
        ) * 43758.5453;

        return value - std::floor(value);
    };

    // Three slender chains hang the basket from the tie above.
    // Two crossed narrow quads per chain keep it visible
    // from every direction.
    for (int chain = 0; chain < 3; ++chain) {
        const double angle =
            PI / 2.0
            + 2.0 * PI * static_cast<double>(chain) / 3.0;

        const Vec3 top(x, chain_top_y, z);

        const Vec3 bottom(
            x + std::cos(angle) * top_half * 0.82,
            rim_y,
            z + std::sin(angle) * top_half * 0.82
        );

        const Vec3 span = bottom - top;

        Vec3 side(-span.z, 0.0, span.x);

        const double side_length = side.length();

        if (side_length > 1e-8) {
            side = side * (0.016 / side_length);
        } else {
            side = Vec3(0.016, 0.0, 0.0);
        }

        world.add(std::make_shared<Quad>(
            top - side,
            span,
            side * 2.0,
            chain_material
        ));

        const Vec3 crossed_side(side.z, 0.0, -side.x);

        world.add(std::make_shared<Quad>(
            top - crossed_side,
            span,
            crossed_side * 2.0,
            chain_material
        ));
    }

    // Hexagonal woven bowl, open at the top and closed below.
    constexpr int SIDES = 6;

    std::vector<Vec3> vertices;
    std::vector<TriangleMesh::Face> faces;

    vertices.reserve(2 * SIDES + 1);
    faces.reserve(4 * SIDES);

    for (int side_index = 0; side_index < SIDES; ++side_index) {
        const double angle =
            2.0 * PI * static_cast<double>(side_index) / SIDES;

        vertices.emplace_back(
            x + std::cos(angle) * top_half,
            rim_y,
            z + std::sin(angle) * top_half
        );
    }

    for (int side_index = 0; side_index < SIDES; ++side_index) {
        const double angle =
            2.0 * PI * static_cast<double>(side_index) / SIDES;

        vertices.emplace_back(
            x + std::cos(angle) * bottom_half,
            bottom_y,
            z + std::sin(angle) * bottom_half
        );
    }

    vertices.emplace_back(x, bottom_y, z);

    for (int side_index = 0; side_index < SIDES; ++side_index) {
        const int next = (side_index + 1) % SIDES;

        const std::size_t top_a =
            static_cast<std::size_t>(side_index);

        const std::size_t top_b =
            static_cast<std::size_t>(next);

        const std::size_t bottom_a =
            static_cast<std::size_t>(SIDES + side_index);

        const std::size_t bottom_b =
            static_cast<std::size_t>(SIDES + next);

        faces.push_back({top_a, top_b, bottom_b});
        faces.push_back({top_a, bottom_b, bottom_a});

        faces.push_back({
            static_cast<std::size_t>(2 * SIDES),
            bottom_b,
            bottom_a
        });
    }

    world.add(std::make_shared<TriangleMesh>(
        vertices,
        faces,
        basket_material
    ));

    // Dark soil just below the rim closes the bowl from above.
    world.add(std::make_shared<Quad>(
        Vec3(
            x + top_half * 0.92,
            rim_y - 0.015,
            z + top_half * 0.92
        ),
        Vec3(-top_half * 1.84, 0.0, 0.0),
        Vec3(0.0, 0.0, -top_half * 1.84),
        soil_material
    ));

    // Trailing vines spill over the rim on all sides.
    for (int vine = 0; vine < 10; ++vine) {
        const double angle =
            2.0 * PI * static_cast<double>(vine) / 10.0
            + 0.35 * (variation(vine) - 0.5);

        const double radial =
            top_half * (0.86 + 0.10 * variation(vine + 8));

        const Vec3 base(
            x + std::cos(angle) * radial,
            rim_y + 0.02,
            z + std::sin(angle) * radial
        );

        const double outward =
            0.14 + 0.10 * variation(vine + 24);

        const double droop =
            0.42 + 0.22 * variation(vine + 16);

        const Vec3 direction(
            std::cos(angle) * outward,
            -droop,
            std::sin(angle) * outward
        );

        greenhouse_add_leaf(
            world,
            base,
            direction,
            0.115 + 0.03 * variation(vine + 32),
            0.10,
            (vine % 2 == 0) ? vine_a : vine_b
        );
    }

    // A few young leaves stand upright in the middle.
    for (int upright = 0; upright < 4; ++upright) {
        const double angle =
            2.0 * PI * static_cast<double>(upright) / 4.0
            + 0.5 * variation(upright + 40);

        const Vec3 base(
            x + std::cos(angle) * 0.05,
            rim_y,
            z + std::sin(angle) * 0.05
        );

        const Vec3 direction(
            std::cos(angle) * 0.09,
            0.20 + 0.06 * variation(upright + 48),
            std::sin(angle) * 0.09
        );

        greenhouse_add_leaf(
            world,
            base,
            direction,
            0.09,
            -0.02,
            vine_a
        );
    }
}

inline void greenhouse_add_octagon_disc(
    HittableList& world,
    double center_x,
    double y,
    double center_z,
    double radius,
    const std::shared_ptr<const Material>& material
) {
    std::vector<Vec3> vertices;
    std::vector<TriangleMesh::Face> faces;

    vertices.emplace_back(center_x, y, center_z);

    for (int side = 0; side < 8; ++side) {
        const double angle =
            2.0 * 3.14159265358979323846 * static_cast<double>(side) / 8.0;

        vertices.emplace_back(
            center_x + std::cos(angle) * radius,
            y,
            center_z + std::sin(angle) * radius
        );
    }

    for (int side = 0; side < 8; ++side) {
        const std::size_t next =
            static_cast<std::size_t>((side + 1) % 8) + 1;

        faces.push_back({
            static_cast<std::size_t>(0),
            static_cast<std::size_t>(side) + 1,
            next
        });
    }

    world.add(std::make_shared<TriangleMesh>(
        vertices,
        faces,
        material
    ));
}

// Closed octagonal prism used as the water body: a real volume so
// refraction, Fresnel reflections and absorption behave correctly.
inline void greenhouse_add_octagon_prism(
    HittableList& world,
    double center_x,
    double bottom_y,
    double top_y,
    double center_z,
    double radius,
    const std::shared_ptr<const Material>& material
) {
    constexpr int SIDES = 8;
    constexpr double PI = 3.14159265358979323846;

    std::vector<Vec3> vertices;
    std::vector<TriangleMesh::Face> faces;

    vertices.reserve(2 * SIDES + 2);

    vertices.emplace_back(center_x, top_y, center_z);
    vertices.emplace_back(center_x, bottom_y, center_z);

    for (int side = 0; side < SIDES; ++side) {
        const double angle =
            2.0 * PI * static_cast<double>(side) / SIDES;

        vertices.emplace_back(
            center_x + std::cos(angle) * radius,
            top_y,
            center_z + std::sin(angle) * radius
        );
    }

    for (int side = 0; side < SIDES; ++side) {
        const double angle =
            2.0 * PI * static_cast<double>(side) / SIDES;

        vertices.emplace_back(
            center_x + std::cos(angle) * radius,
            bottom_y,
            center_z + std::sin(angle) * radius
        );
    }

    const std::size_t top_center = 0;
    const std::size_t bottom_center = 1;

    for (int side = 0; side < SIDES; ++side) {
        const int next = (side + 1) % SIDES;

        const std::size_t top_a =
            2 + static_cast<std::size_t>(side);

        const std::size_t top_b =
            2 + static_cast<std::size_t>(next);

        const std::size_t bottom_a =
            2 + static_cast<std::size_t>(SIDES + side);

        const std::size_t bottom_b =
            2 + static_cast<std::size_t>(SIDES + next);

        faces.push_back({top_center, top_b, top_a});
        faces.push_back({bottom_center, bottom_a, bottom_b});

        faces.push_back({top_a, top_b, bottom_b});
        faces.push_back({top_a, bottom_b, bottom_a});
    }

    world.add(std::make_shared<TriangleMesh>(
        vertices,
        faces,
        material
    ));
}

// A round stone cistern holding a shallow sheet of still water.
// The water surface is a thin-glass sheet so the pebbles on the
// basin floor stay visible through it with a slight refraction.
inline void greenhouse_add_cistern(
    HittableList& world,
    double center_x,
    double center_z,
    const std::shared_ptr<const Material>& stone_material,
    const std::shared_ptr<const Material>& water_material,
    const std::shared_ptr<const Material>& basin_floor_material
) {
    constexpr int SIDES = 8;

    const double outer_radius = 0.58;
    const double inner_radius = 0.42;
    const double wall_top = 0.36;
    const double water_level = 0.34;
    const double basin_floor_y = 0.13;

    std::vector<Vec3> vertices;
    std::vector<TriangleMesh::Face> faces;

    vertices.reserve(4 * SIDES);

    for (int ring = 0; ring < 4; ++ring) {
        const double radius =
            (ring < 2) ? outer_radius : inner_radius;

        const double y =
            (ring % 2 == 0) ? 0.0 : wall_top;

        for (int side = 0; side < SIDES; ++side) {
            const double angle =
                2.0 * 3.14159265358979323846
                * static_cast<double>(side) / SIDES;

            vertices.emplace_back(
                center_x + std::cos(angle) * radius,
                y,
                center_z + std::sin(angle) * radius
            );
        }
    }

    for (int side = 0; side < SIDES; ++side) {
        const int next = (side + 1) % SIDES;

        const std::size_t outer_bottom =
            static_cast<std::size_t>(side);

        const std::size_t outer_top =
            static_cast<std::size_t>(SIDES + side);

        const std::size_t outer_bottom_next =
            static_cast<std::size_t>(next);

        const std::size_t outer_top_next =
            static_cast<std::size_t>(SIDES + next);

        // Outer wall.
        faces.push_back({outer_bottom, outer_bottom_next, outer_top_next});
        faces.push_back({outer_bottom, outer_top_next, outer_top});

        const std::size_t inner_ring = 2 * SIDES;

        const std::size_t inner_bottom =
            inner_ring + static_cast<std::size_t>(side);

        const std::size_t inner_top =
            inner_ring + static_cast<std::size_t>(SIDES + side);

        const std::size_t inner_bottom_next =
            inner_ring + static_cast<std::size_t>(next);

        const std::size_t inner_top_next =
            inner_ring + static_cast<std::size_t>(SIDES + next);

        // Inner wall.
        faces.push_back({inner_bottom, inner_top_next, inner_bottom_next});
        faces.push_back({inner_bottom, inner_top, inner_top_next});

        // Rim top ring.
        faces.push_back({outer_top, inner_top, inner_top_next});
        faces.push_back({outer_top, inner_top_next, outer_top_next});
    }

    world.add(std::make_shared<TriangleMesh>(
        vertices,
        faces,
        stone_material
    ));

    // Dark basin floor just below the water line.
    greenhouse_add_octagon_disc(
        world,
        center_x,
        basin_floor_y,
        center_z,
        inner_radius - 0.02,
        basin_floor_material
    );

    // Closed water body: refraction, Fresnel reflections and
    // absorption all behave like real water. The bottom rim is
    // buried below the opaque basin floor so no ray can travel in
    // the gap between water and floor while inside the medium.
    greenhouse_add_octagon_prism(
        world,
        center_x,
        basin_floor_y - 0.02,
        water_level,
        center_z,
        inner_radius - 0.01,
        water_material
    );

    // Pebbles rest on the basin floor, partly submerged.
    auto pebble_material = std::make_shared<PrincipledBSDF>(
        Vec3(0.30, 0.29, 0.26),
        0.0,
        0.62
    );

    for (int pebble = 0; pebble < 9; ++pebble) {
        const double value = std::sin(
            pebble * 12.9898 + 4.17
        ) * 43758.5453;

        const double fraction = value - std::floor(value);

        const double angle =
            2.0 * 3.14159265358979323846 * fraction
            + 0.7 * pebble;

        const double radial =
            inner_radius * (0.25 + 0.55 * fraction);

        const double radius =
            0.042 + 0.030 * (0.5 + 0.5 * fraction);

        world.add(std::make_shared<Sphere>(
            Vec3(
                center_x + std::cos(angle) * radial,
                basin_floor_y + radius,
                center_z + std::sin(angle) * radial
            ),
            radius,
            pebble_material
        ));
    }
}

// A rectangular stone trough with the same still-water treatment.
inline void greenhouse_add_stone_trough(
    HittableList& world,
    double min_x,
    double min_z,
    double max_x,
    double max_z,
    const std::shared_ptr<const Material>& stone_material,
    const std::shared_ptr<const Material>& water_material,
    const std::shared_ptr<const Material>& basin_floor_material
) {
    const double wall_top = 0.40;
    const double water_level = 0.37;
    const double floor_level = 0.15;
    const double wall = 0.11;
    const double water_bottom = 0.13;
    const double wx0 = min_x + wall + 0.01;
    const double wx1 = max_x - wall - 0.01;
    const double wz0 = min_z + wall + 0.01;
    const double wz1 = max_z - wall - 0.01;

    // Four walls.
    greenhouse_box(
        world,
        Vec3(min_x, 0.0, min_z),
        Vec3(max_x, wall_top, min_z + wall),
        stone_material
    );

    greenhouse_box(
        world,
        Vec3(min_x, 0.0, max_z - wall),
        Vec3(max_x, wall_top, max_z),
        stone_material
    );

    greenhouse_box(
        world,
        Vec3(min_x, 0.0, min_z),
        Vec3(min_x + wall, wall_top, max_z),
        stone_material
    );

    greenhouse_box(
        world,
        Vec3(max_x - wall, 0.0, min_z),
        Vec3(max_x, wall_top, max_z),
        stone_material
    );

    // Stone coping slabs overhang the walls slightly.
    greenhouse_box(
        world,
        Vec3(min_x - 0.02, wall_top, min_z - 0.02),
        Vec3(max_x + 0.02, wall_top + 0.05, min_z + wall + 0.02),
        stone_material
    );

    greenhouse_box(
        world,
        Vec3(min_x - 0.02, wall_top, max_z - wall - 0.02),
        Vec3(max_x + 0.02, wall_top + 0.05, max_z + 0.02),
        stone_material
    );

    greenhouse_box(
        world,
        Vec3(min_x - 0.02, wall_top, min_z),
        Vec3(min_x + wall + 0.02, wall_top + 0.05, max_z),
        stone_material
    );

    greenhouse_box(
        world,
        Vec3(max_x - wall - 0.02, wall_top, min_z),
        Vec3(max_x + 0.02, wall_top + 0.05, max_z),
        stone_material
    );

    // Basin floor below the water body.
    world.add(std::make_shared<Quad>(
        Vec3(wx0 + 0.01, floor_level, wz1 - 0.01),
        Vec3(wx1 - wx0 - 0.02, 0.0, 0.0),
        Vec3(0.0, 0.0, -(wz1 - wz0 - 0.02)),
        basin_floor_material
    ));

    // Closed water volume as a single watertight mesh so refraction
    // and Fresnel reflections behave like real water. The bottom is
    // buried below the basin floor so the medium stays consistent.
    {
        std::vector<Vec3> water_vertices = {
            Vec3(wx0, water_level, wz0),
            Vec3(wx1, water_level, wz0),
            Vec3(wx1, water_level, wz1),
            Vec3(wx0, water_level, wz1),
            Vec3(wx0, water_bottom, wz0),
            Vec3(wx1, water_bottom, wz0),
            Vec3(wx1, water_bottom, wz1),
            Vec3(wx0, water_bottom, wz1)
        };

        const std::vector<TriangleMesh::Face> water_faces = {
            {0, 2, 1}, {0, 3, 2},   // top, outward up
            {4, 5, 6}, {4, 6, 7},   // bottom, outward down
            {0, 1, 5}, {0, 5, 4},   // front z0
            {2, 3, 7}, {2, 7, 6},   // back z1
            {3, 0, 4}, {3, 4, 7},   // left x0
            {1, 2, 6}, {1, 6, 5}    // right x1
        };

        world.add(std::make_shared<TriangleMesh>(
            water_vertices,
            water_faces,
            water_material
        ));
    }

    // A few submerged pebbles.
    auto pebble_material = std::make_shared<PrincipledBSDF>(
        Vec3(0.30, 0.29, 0.26),
        0.0,
        0.62
    );

    for (int pebble = 0; pebble < 5; ++pebble) {
        const double value = std::sin(
            pebble * 37.719 + 2.53
        ) * 43758.5453;

        const double fraction = value - std::floor(value);

        const double x =
            wx0 + 0.10
            + fraction * (wx1 - wx0 - 0.24);

        const double z =
            wz0 + 0.12
            + (1.0 - fraction) * (wz1 - wz0 - 0.28);

        const double radius =
            0.045 + 0.028 * fraction;

        world.add(std::make_shared<Sphere>(
            Vec3(x, floor_level + radius, z),
            radius,
            pebble_material
        ));
    }
}

struct GreenhousePlantPlacement {
    Vec3 position;
    double target_height;
};

inline HittableList make_greenhouse_scene() {
    HittableList world;

    auto floor_texture = std::make_shared<PerlinNoiseTexture>(
        Vec3(0.20, 0.195, 0.175),
        Vec3(0.33, 0.315, 0.275),
        0.72,
        5,
        417
    );

    auto floor = std::make_shared<Lambertian>(
        floor_texture
    );

    // Flagstone palette: three weathered granite variants so the
    // path reads as pieced-together stone rather than stamped tiles.
    auto stone_path_material_a = std::make_shared<Lambertian>(
        std::make_shared<PerlinNoiseTexture>(
            Vec3(0.26, 0.24, 0.21),
            Vec3(0.40, 0.385, 0.35),
            9.0,
            5,
            2026
        )
    );

    auto stone_path_material_b = std::make_shared<Lambertian>(
        std::make_shared<PerlinNoiseTexture>(
            Vec3(0.23, 0.215, 0.19),
            Vec3(0.375, 0.36, 0.33),
            7.0,
            5,
            2027
        )
    );

    auto stone_path_material_c = std::make_shared<Lambertian>(
        std::make_shared<PerlinNoiseTexture>(
            Vec3(0.28, 0.255, 0.215),
            Vec3(0.43, 0.41, 0.37),
            11.0,
            5,
            2028
        )
    );

    const std::vector<std::shared_ptr<const Material>>
        stone_path_materials = {
            stone_path_material_a,
            stone_path_material_b,
            stone_path_material_c
        };

    auto stone_joint_material =
        std::make_shared<Lambertian>(
            Vec3(0.062, 0.052, 0.038)
        );

    auto stone = std::make_shared<PrincipledBSDF>(
        Vec3(0.36, 0.31, 0.23), 0.0, 0.72);

    auto iron = std::make_shared<PrincipledBSDF>(
        Vec3(0.045, 0.095, 0.065), 0.45, 0.48);

    auto copper = std::make_shared<PrincipledBSDF>(
        Vec3(0.20, 0.12, 0.075), 0.0, 0.78);

    // Thin architectural glazing.
    // This material is designed for open Quad surfaces,
    // not closed dielectric volumes.
    auto glass_pane = std::make_shared<ThinGlass>(
        1.58,
        Vec3(0.88, 0.95, 0.92)
    );

    // Weathered panes alternate with clean ones so the glazing
    // reads as individual sheets of glass rather than open air.
    auto glass_pane_weathered = std::make_shared<ThinGlass>(
        1.58,
        Vec3(0.82, 0.92, 0.87)
    );

    world.add(std::make_shared<Quad>(
        Vec3(-4, 0, 4), Vec3(8, 0, 0), Vec3(0, 0, -12), floor));

    greenhouse_add_stone_slab_path(
        world,
        stone_path_materials,
        stone_joint_material,
        0.0,
        3.40,
        8,
        2,
        0.34,
        0.88,
        0.295,
        0.375,
        0.105,
        3
    );

    greenhouse_add_stone_slab_path(
        world,
        stone_path_materials,
        stone_joint_material,
        0.0,
        3.94,
        1,
        1,
        0.0,
        0.88,
        0.30,
        0.30,
        0.090,
        29
    );

    greenhouse_add_stone_slab_path(
        world,
        stone_path_materials,
        stone_joint_material,
        0.0,
        12.60,
        9,
        3,
        0.46,
        1.02,
        0.34,
        0.42,
        0.075,
        17
    );

    // Exterior landscape surrounding the conservatory.
    auto exterior_ground_texture = std::make_shared<PerlinNoiseTexture>(
        Vec3(0.055, 0.082, 0.030),
        Vec3(0.19, 0.24, 0.095),
        0.22,
        6,
        913
    );

    auto exterior_ground = std::make_shared<Lambertian>(
        exterior_ground_texture
    );

    world.add(std::make_shared<Quad>(
        Vec3(-30.0, -0.08, 30.0),
        Vec3(60.0, 0.0, 0.0),
        Vec3(0.0, 0.0, -60.0),
        exterior_ground
    ));

    // Irregular gravel stays outside the greenhouse, along both edges
    // of the approach path (whose z range is 4.0 to 15.0).
    auto gravel_material = std::make_shared<PrincipledBSDF>(
        Vec3(0.29, 0.27, 0.22),
        0.0,
        0.94
    );

    for (int i = 0; i < 72; ++i) {
        const double a = std::sin(i * 12.9898 + 4.17);
        const double b = std::sin(i * 78.233 + 1.91);
        const double c = std::sin(i * 37.719 + 8.31);

        const double side = (i % 2 == 0) ? -1.0 : 1.0;
        const double x = side * (1.38 + 0.30 * std::abs(a));
        const double z = 4.25 + 10.45 * (i / 2) / 35.0
            + 0.12 * b;
        const double radius = 0.022 + 0.020 * (0.5 + 0.5 * c);

        world.add(std::make_shared<Sphere>(
            Vec3(x, -0.025 + 0.006 * b, z),
            radius,
            gravel_material
        ));
    }

    // Exterior planting visible through the glazing, so the view
    // outward reads as garden instead of bare lawn. These plants
    // stand outside the interior bounds and are loaded without the
    // interior clearance validation.
    auto exterior_foliage = std::make_shared<PrincipledBSDF>(
        Vec3(0.085, 0.155, 0.045),
        0.0,
        0.62
    );

    const auto add_exterior_shrub01 = [&](
        const std::string& variant,
        double x,
        double z,
        double height
    ) {
        TriangleMesh plant = ObjLoader::load_file(
            "assets/shrub_01_2k.blend/shrub_01_2k.obj",
            exterior_foliage,
            height,
            Vec3(x, 0.0, z),
            { "shrub_01_" + variant + "_LOD0" }
        );

        world.add(
            std::make_shared<TriangleMesh>(
                std::move(plant)
            )
        );
    };

    const auto add_exterior_model = [&](
        const std::string& path,
        double x,
        double z,
        double height,
        double width
    ) {
        TriangleMesh plant = ObjLoader::load_file(
            path,
            exterior_foliage,
            height,
            Vec3(x, 0.0, z),
            {},
            width
        );

        world.add(
            std::make_shared<TriangleMesh>(
                std::move(plant)
            )
        );
    };

    // Left of the conservatory.
    add_exterior_shrub01("b", -4.60, 0.50, 1.50);
    add_exterior_shrub01("d", -5.00, -2.20, 1.70);
    add_exterior_model(
        "assets/fern_02_2k.blend/fern_02_2k.obj",
        -4.30, -1.20, 0.90, 0.0);
    add_exterior_model(
        "assets/fern_02_2k.blend/fern_02_2k.obj",
        -4.50, -4.00, 1.00, 0.0);
    add_exterior_model(
        "assets/shrub_03_2k.blend/shrub_03_2k.obj",
        -4.90, -5.20, 0.0, 1.60);
    add_exterior_shrub01("f", -4.40, -6.20, 1.40);

    // Right of the conservatory.
    add_exterior_shrub01("c", 4.50, 0.20, 1.60);
    add_exterior_model(
        "assets/fern_02_2k.blend/fern_02_2k.obj",
        4.40, -1.60, 0.90, 0.0);
    add_exterior_shrub01("e", 5.00, -3.40, 1.70);
    add_exterior_model(
        "assets/shrub_04_2k.blend/shrub_04_2k.obj",
        4.60, -5.00, 0.0, 1.50);
    add_exterior_model(
        "assets/fern_02_2k.blend/fern_02_2k.obj",
        4.30, -6.40, 0.85, 0.0);

    // Behind the rear wall.
    add_exterior_shrub01("g", -1.60, -8.90, 1.50);
    add_exterior_shrub01("i", 0.80, -9.20, 1.60);
    add_exterior_shrub01("h", 2.20, -8.80, 1.30);
    add_exterior_model(
        "assets/fern_02_2k.blend/fern_02_2k.obj",
        -0.20, -9.00, 1.00, 0.0);

    greenhouse_box(world, Vec3(-4, 0, -8), Vec3(-3.72, .62, 4), stone);
    greenhouse_box(world, Vec3(3.72, 0, -8), Vec3(4, .62, 4), stone);

    // Side columns and transverse ties.
    for (int z = 3; z >= -6; z -= 3) {
        greenhouse_box(world, Vec3(-3.88, .62, z - .11),
            Vec3(-3.68, 4.05, z + .11), iron);
        greenhouse_box(world, Vec3(3.68, .62, z - .11),
            Vec3(3.88, 4.05, z + .11), iron);
        greenhouse_box(world, Vec3(-3.82, 3.82, z - .075),
            Vec3(3.82, 4.02, z + .075), iron);
    }


    greenhouse_box(world, Vec3(-.10, 5.98, -8),
        Vec3(.10, 6.20, 4), copper);
    greenhouse_box(world, Vec3(-4, 3.91, -8),
        Vec3(-3.82, 4.12, 4), iron);
    greenhouse_box(world, Vec3(3.82, 3.91, -8),
        Vec3(4, 4.12, 4), iron);

    // Roof cross rafters.
    for (int i = 0; i <= 5; ++i) {
        const double z = 3.85 - 2.4 * i;
        world.add(std::make_shared<Quad>(
            Vec3(-4, 4.04, z), Vec3(4, 1.98, 0),
            Vec3(0, 0, -.075), iron));
        world.add(std::make_shared<Quad>(
            Vec3(0, 6.02, z), Vec3(4, -1.98, 0),
            Vec3(0, 0, -.075), iron));
    }

    // Rear glazed wall and mullions.
    for (double x : {-2.4, -1.2, 0.0, 1.2, 2.4}) {
        greenhouse_box(world, Vec3(x - .035, .72, -7.98),
            Vec3(x + .035, 3.95, -7.82), iron);
    }

    // Side-wall glazing: individual panes separated from metal frames.
    // The glass is slightly offset toward the interior to avoid
    // coplanar intersections with the structural mullions.
    const double side_glass_left_x = -3.735;
    const double side_glass_right_x = 3.735;

    const std::array<double, 7> pane_z = {
        3.82, 2.08, 0.08, -1.92, -3.92, -5.92, -7.82
    };

    const std::array<double, 4> pane_y = {
        0.76, 1.70, 2.80, 3.86
    };

    for (std::size_t zi = 0; zi + 1 < pane_z.size(); ++zi) {
        for (std::size_t yi = 0; yi + 1 < pane_y.size(); ++yi) {
            const double z_start = pane_z[zi];
            const double z_end = pane_z[zi + 1];
            const double y_start = pane_y[yi];
            const double pane_height = pane_y[yi + 1] - y_start;

            const Vec3 pane_u(0.0, 0.0, z_end - z_start);
            const Vec3 pane_v(0.0, pane_height, 0.0);

            const auto pane_material =
                ((zi + yi) % 2 == 0) ? glass_pane : glass_pane_weathered;

            world.add(std::make_shared<Quad>(
                Vec3(side_glass_left_x, y_start, z_start),
                pane_u,
                pane_v,
                pane_material
            ));

            world.add(std::make_shared<Quad>(
                Vec3(side_glass_right_x, y_start, z_start),
                pane_u,
                pane_v,
                pane_material
            ));
        }
    }

    // Vertical side-wall mullions.
    for (int z = 2; z >= -6; z -= 2) {
        greenhouse_box(
            world,
            Vec3(-3.86, 0.72, z - 0.035),
            Vec3(-3.78, 3.90, z + 0.035),
            iron
        );

        greenhouse_box(
            world,
            Vec3(3.78, 0.72, z - 0.035),
            Vec3(3.86, 3.90, z + 0.035),
            iron
        );
    }

    // Horizontal side-wall glazing rails.
    for (double y : {1.75, 2.85}) {
        greenhouse_box(
            world,
            Vec3(-3.86, y, -8.0),
            Vec3(-3.78, y + 0.055, 4.0),
            iron
        );

        greenhouse_box(
            world,
            Vec3(3.78, y, -8.0),
            Vec3(3.86, y + 0.055, 4.0),
            iron
        );
    }

    // Rear wall glazing.
    world.add(std::make_shared<Quad>(
        Vec3(-3.78, 0.72, -7.82),
        Vec3(7.56, 0.0, 0.0),
        Vec3(0.0, 3.18, 0.0),
        glass_pane
    ));

    // Roof glazing: left slope.
    world.add(std::make_shared<Quad>(
        Vec3(-3.96, 4.08, 3.82),
        Vec3(3.92, 1.94, 0.0),
        Vec3(0.0, 0.0, -11.64),
        glass_pane
    ));

    // Roof glazing: right slope.
    world.add(std::make_shared<Quad>(
        Vec3(0.04, 5.98, 3.82),
        Vec3(3.92, -1.94, 0.0),
        Vec3(0.0, 0.0, -11.64),
        glass_pane
    ));

    // Slim pressure caps divide the roof glazing into believable panels.
    // These sit just above the glass and follow each roof pitch.
    for (double t : {0.25, 0.50, 0.75}) {
        const double left_x = -3.96 + 3.92 * t;
        const double left_y = 4.08 + 1.94 * t;

        world.add(std::make_shared<Quad>(
            Vec3(left_x, left_y + 0.018, 3.82),
            Vec3(0.035, 0.017, 0.0),
            Vec3(0.0, 0.0, -11.64),
            iron
        ));

        const double right_x = 0.04 + 3.92 * t;
        const double right_y = 5.98 - 1.94 * t;

        world.add(std::make_shared<Quad>(
            Vec3(right_x, right_y + 0.018, 3.82),
            Vec3(0.035, -0.017, 0.0),
            Vec3(0.0, 0.0, -11.64),
            iron
        ));
    }

    // Fine transverse glazing caps create smaller roof-light modules.
    for (int i = 0; i <= 10; ++i) {
        const double z = 3.82 - 1.16 * i;

        world.add(std::make_shared<Quad>(
            Vec3(-3.96, 4.105, z),
            Vec3(3.92, 1.94, 0.0),
            Vec3(0.0, 0.025, 0.0),
            iron
        ));

        world.add(std::make_shared<Quad>(
            Vec3(0.04, 5.985, z),
            Vec3(3.92, -1.94, 0.0),
            Vec3(0.0, -0.025, 0.0),
            iron
        ));
    }

    // Front entrance glazing, leaving a central doorway.
    world.add(std::make_shared<Quad>(
        Vec3(-3.78, 0.72, 3.82),
        Vec3(2.58, 0.0, 0.0),
        Vec3(0.0, 3.18, 0.0),
        glass_pane
    ));

    world.add(std::make_shared<Quad>(
        Vec3(1.20, 0.72, 3.82),
        Vec3(2.58, 0.0, 0.0),
        Vec3(0.0, 3.18, 0.0),
        glass_pane
    ));

    // Imported botanical assets.
    auto anthurium_fallback =
        std::make_shared<PrincipledBSDF>(
            Vec3(0.12, 0.30, 0.075),
            0.0,
            0.48
        );

    auto fern_fallback =
        std::make_shared<PrincipledBSDF>(
            Vec3(0.075, 0.24, 0.055),
            0.0,
            0.52
        );

    // Anthurium: the main botanical subject on the exhibition plinth.
    TriangleMesh anthurium = greenhouse_load_plant_obj(
        "assets/anthurium_botany_01_2k.blend/"
        "anthurium_botany_01_2k.obj",
        anthurium_fallback,
        2.35,
        Vec3(1.90, 0.82, -2.70)
    );

    world.add(
        std::make_shared<TriangleMesh>(
            std::move(anthurium)
        )
    );

    // Fern planters provide foreground and background depth.
    auto fern_planter = std::make_shared<PrincipledBSDF>(
        Vec3(0.16, 0.12, 0.085),
        0.0,
        0.72
    );

    auto fern_soil = std::make_shared<Lambertian>(
        Vec3(0.075, 0.052, 0.028)
    );

    // Ferns planted directly in the ground read as jungle undergrowth
    // rather than a nursery; the side beds below place them densely.
    const auto add_ground_fern = [&](
        double x,
        double z,
        double height,
        double base_y
    ) {
        TriangleMesh fern = greenhouse_load_plant_obj(
            "assets/fern_02_2k.blend/fern_02_2k.obj",
            fern_fallback,
            height,
            Vec3(x, base_y, z)
        );

        world.add(
            std::make_shared<TriangleMesh>(
                std::move(fern)
            )
        );
    };

    // A few taller ferns anchor the bed openings.
    add_ground_fern(-2.45, 0.95, 1.55, 0.02);
    add_ground_fern(2.45, -0.55, 1.35, 0.02);

    // Foreground Monstera: the source model includes its own planter.
    auto monstera_fallback =
        std::make_shared<PrincipledBSDF>(
            Vec3(0.075, 0.24, 0.045),
            0.0,
            0.52
        );

    TriangleMesh monstera = greenhouse_load_plant_obj(
        "assets/potted-split-leaf-monstera.obj",
        monstera_fallback,
        1.55,
        Vec3(2.35, 0.0, 1.35)
    );

    world.add(
        std::make_shared<TriangleMesh>(
            std::move(monstera)
        )
    );

    // Background Pachira: a single tall focal silhouette.
    auto pachira_fallback =
        std::make_shared<PrincipledBSDF>(
            Vec3(0.16, 0.25, 0.075),
            0.0,
            0.62
        );

    auto pachira_pot =
        std::make_shared<PrincipledBSDF>(
            Vec3(0.095, 0.075, 0.060),
            0.0,
            0.82
        );

    const double pachira_pot_height = 0.52;

    greenhouse_add_tapered_pot(
        world,
        0.0,
        -6.15,
        0.34,
        0.43,
        pachira_pot_height,
        pachira_pot
    );

    world.add(std::make_shared<Quad>(
        Vec3(-0.36, pachira_pot_height - 0.025, -6.51),
        Vec3(0.72, 0.0, 0.0),
        Vec3(0.0, 0.0, 0.72),
        fern_soil
    ));

    TriangleMesh pachira = greenhouse_load_plant_obj(
        "assets/pachira_aquatica_01_2k.blend/"
        "pachira_aquatica_01_2k.obj",
        pachira_fallback,
        2.30,
        Vec3(0.0, pachira_pot_height, -6.15),
        {
            "pachira_aquatica_01_bark_d",
            "pachira_aquatica_01_leaves_d"
        }
    );
    
    world.add(
        std::make_shared<TriangleMesh>(
            std::move(pachira)
        )
    );

    // Dense rear understory: overlapping low shrub clusters.
    auto shrub_fallback =
        std::make_shared<PrincipledBSDF>(
            Vec3(0.075, 0.22, 0.045),
            0.0,
            0.72
        );

    auto add_shrub = [&](
        const std::string& path,
        double x,
        double z,
        double width
    ) {
        TriangleMesh shrub = greenhouse_load_plant_obj(
            path,
            shrub_fallback,
            0.0,
            Vec3(x, 0.0, z),
            {},
            width
        );

        world.add(
            std::make_shared<TriangleMesh>(
                std::move(shrub)
            )
        );
    };

    // Rear understory is arranged as an irregular, overlapping mass.
    // Larger shrubs anchor the outer ends; smaller shrubs soften the
    // transition toward the central Pachira without blocking its trunk.
    add_shrub(
        "assets/shrub_03_2k.blend/shrub_03_2k.obj",
        -2.25,
        -6.95,
        1.75
    );

    add_shrub(
        "assets/shrub_04_2k.blend/shrub_04_2k.obj",
        -1.05,
        -7.18,
        1.18
    );

    add_shrub(
        "assets/shrub_04_2k.blend/shrub_04_2k.obj",
        1.05,
        -7.05,
        1.28
    );

    add_shrub(
        "assets/shrub_03_2k.blend/shrub_03_2k.obj",
        2.30,
        -6.90,
        1.60
    );

    // Calathea: broad, low foliage on both sides of the walkway.
    auto calathea_fallback =
        std::make_shared<PrincipledBSDF>(
            Vec3(0.12, 0.28, 0.075),
            0.0,
            0.55
        );

    auto calathea_pot =
        std::make_shared<PrincipledBSDF>(
            Vec3(0.12, 0.085, 0.065),
            0.0,
            0.78
        );

    auto add_calathea = [&](double x, double z, double base_y) {
        TriangleMesh calathea = greenhouse_load_plant_obj(
            "assets/calathea_orbifolia_01_2k.blend/"
            "calathea_orbifolia_01_2k.obj",
            calathea_fallback,
            0.0,
            Vec3(x, base_y, z),
            {
                "calathea_orbifolia_01_a",
                "calathea_orbifolia_01_b",
                "calathea_orbifolia_01_c",
                "calathea_orbifolia_01_d",
                "calathea_orbifolia_01_e"
            },
            1.25
        );

        world.add(
            std::make_shared<TriangleMesh>(
                std::move(calathea)
            )
        );
    };

    // Broad-leaved calatheas grow straight out of the beds and
    // soften the hard edges of the paved areas.
    add_calathea(-2.25, -2.75, 0.02);

    // Hanging baskets suspended from the transverse roof ties.
    // They fill the empty upper volume without crowding the
    // central sight line down the walkway.
    auto basket_weave = std::make_shared<PrincipledBSDF>(
        Vec3(0.36, 0.25, 0.13),
        0.0,
        0.85
    );

    auto vine_a = std::make_shared<PrincipledBSDF>(
        Vec3(0.10, 0.26, 0.06),
        0.0,
        0.50
    );

    auto vine_b = std::make_shared<PrincipledBSDF>(
        Vec3(0.22, 0.34, 0.09),
        0.0,
        0.55
    );

    greenhouse_add_hanging_basket(
        world,
        -1.60,
        3.00,
        3.82,
        basket_weave,
        iron,
        fern_soil,
        vine_a,
        vine_b,
        11
    );

    greenhouse_add_hanging_basket(
        world,
        1.60,
        0.00,
        3.82,
        basket_weave,
        iron,
        fern_soil,
        vine_a,
        vine_b,
        29
    );

    greenhouse_add_hanging_basket(
        world,
        -1.60,
        -3.00,
        3.82,
        basket_weave,
        iron,
        fern_soil,
        vine_a,
        vine_b,
        47
    );

    greenhouse_add_hanging_basket(
        world,
        1.60,
        -6.00,
        3.82,
        basket_weave,
        iron,
        fern_soil,
        vine_a,
        vine_b,
        71
    );

    // Wooden specimen display bench along the front-left wall.
    // Iron legs carry slatted timber tops holding a small curated
    // collection of potted plants with species labels, filling the
    // bare floor near the entrance.
    auto bench_wood_texture = std::make_shared<PerlinNoiseTexture>(
        Vec3(0.26, 0.165, 0.09),
        Vec3(0.42, 0.30, 0.18),
        8.5,
        4,
        517
    );

    auto bench_wood = std::make_shared<Lambertian>(
        bench_wood_texture
    );

    auto terracotta = std::make_shared<PrincipledBSDF>(
        Vec3(0.45, 0.23, 0.13),
        0.0,
        0.78
    );

    auto label_plaque = std::make_shared<PrincipledBSDF>(
        Vec3(0.78, 0.74, 0.62),
        0.0,
        0.60
    );

    constexpr double BENCH_MIN_X = -3.45;
    constexpr double BENCH_MAX_X = -2.15;
    constexpr double BENCH_MIN_Z = 1.30;
    constexpr double BENCH_MAX_Z = 3.20;
    constexpr double BENCH_TOP_Y = 0.595;

    // Four iron legs, inset from the bench corners.
    for (double leg_x : {-3.35, -2.25}) {
        for (double leg_z : {1.42, 3.08}) {
            greenhouse_box(
                world,
                Vec3(leg_x - 0.028, 0.0, leg_z - 0.028),
                Vec3(leg_x + 0.028, 0.56, leg_z + 0.028),
                iron
            );
        }
    }

    // Five timber slats form the bench top.
    for (int slat = 0; slat < 5; ++slat) {
        const double slat_center_x =
            BENCH_MIN_X + 0.10 + slat * 0.275;

        greenhouse_box(
            world,
            Vec3(slat_center_x - 0.10, 0.56, BENCH_MIN_Z),
            Vec3(slat_center_x + 0.10, BENCH_TOP_Y, BENCH_MAX_Z),
            bench_wood
        );
    }

    // Three lower shelf boards between the legs.
    for (int shelf = 0; shelf < 3; ++shelf) {
        const double shelf_center_x =
            BENCH_MIN_X + 0.21 + shelf * 0.45;

        greenhouse_box(
            world,
            Vec3(shelf_center_x - 0.19, 0.16, BENCH_MIN_Z + 0.05),
            Vec3(shelf_center_x + 0.19, 0.185, BENCH_MAX_Z - 0.05),
            bench_wood
        );
    }

    // A display pot with a small species label plaque facing the
    // walkway, the way botanical gardens annotate their exhibits.
    const auto add_bench_specimen_pot = [&](
        double x,
        double z,
        double bottom_hw,
        double top_hw,
        double pot_h
    ) {
        greenhouse_add_tapered_pot(
            world,
            x,
            z,
            bottom_hw,
            top_hw,
            pot_h,
            terracotta,
            BENCH_TOP_Y
        );

        world.add(std::make_shared<Quad>(
            Vec3(
                x + top_hw * 0.92,
                BENCH_TOP_Y + pot_h - 0.012,
                z + top_hw * 0.92
            ),
            Vec3(-top_hw * 1.84, 0.0, 0.0),
            Vec3(0.0, 0.0, -top_hw * 1.84),
            fern_soil
        ));

        const double post_x = x + top_hw + 0.10;
        const double post_z = z + 0.10;
        const double post_top = BENCH_TOP_Y + 0.17;

        greenhouse_box(
            world,
            Vec3(post_x - 0.007, BENCH_TOP_Y, post_z - 0.007),
            Vec3(post_x + 0.007, post_top, post_z + 0.007),
            iron
        );

        constexpr double PLAQUE_WIDTH = 0.15;
        constexpr double PLAQUE_HEIGHT = 0.09;
        constexpr double PLAQUE_TILT = 0.35;

        world.add(std::make_shared<Quad>(
            Vec3(
                post_x,
                post_top - 0.03,
                post_z + PLAQUE_WIDTH / 2
            ),
            Vec3(0.0, 0.0, -PLAQUE_WIDTH),
            Vec3(
                -std::sin(PLAQUE_TILT) * PLAQUE_HEIGHT,
                std::cos(PLAQUE_TILT) * PLAQUE_HEIGHT,
                0.0
            ),
            label_plaque
        ));
    };

    // Specimen 1: young tree fern in a deep pot.
    add_bench_specimen_pot(-3.02, 1.85, 0.13, 0.175, 0.25);

    TriangleMesh bench_fern_a = greenhouse_load_plant_obj(
        "assets/fern_02_2k.blend/fern_02_2k.obj",
        fern_fallback,
        0.52,
        Vec3(-3.02, BENCH_TOP_Y + 0.24, 1.85)
    );

    world.add(
        std::make_shared<TriangleMesh>(
            std::move(bench_fern_a)
        )
    );

    // Specimen 2: calathea with broad ornamental leaves.
    add_bench_specimen_pot(-2.50, 2.35, 0.155, 0.20, 0.27);

    TriangleMesh bench_calathea = greenhouse_load_plant_obj(
        "assets/calathea_orbifolia_01_2k.blend/"
        "calathea_orbifolia_01_2k.obj",
        calathea_fallback,
        0.0,
        Vec3(-2.50, BENCH_TOP_Y + 0.26, 2.35),
        {
            "calathea_orbifolia_01_a",
            "calathea_orbifolia_01_b",
            "calathea_orbifolia_01_c",
            "calathea_orbifolia_01_d",
            "calathea_orbifolia_01_e"
        },
        0.55
    );

    world.add(
        std::make_shared<TriangleMesh>(
            std::move(bench_calathea)
        )
    );

    // Specimen 3: compact fern.
    add_bench_specimen_pot(-2.90, 2.90, 0.125, 0.165, 0.23);

    TriangleMesh bench_fern_b = greenhouse_load_plant_obj(
        "assets/fern_02_2k.blend/fern_02_2k.obj",
        fern_fallback,
        0.44,
        Vec3(-2.90, BENCH_TOP_Y + 0.22, 2.90)
    );

    world.add(
        std::make_shared<TriangleMesh>(
            std::move(bench_fern_b)
        )
    );

    // Naturalistic rear planting bed.
    // The bed edge wanders instead of running straight, and the
    // planting inside is layered and irregular so it reads as a
    // curated garden border rather than a nursery row.
    auto bed_soil_texture = std::make_shared<PerlinNoiseTexture>(
        Vec3(0.10, 0.07, 0.045),
        Vec3(0.21, 0.15, 0.10),
        3.0,
        4,
        731
    );

    auto bed_soil = std::make_shared<Lambertian>(
        bed_soil_texture
    );

    const auto add_bed_segment = [&](
        double x0,
        double x1,
        double front_z
    ) {
        world.add(std::make_shared<Quad>(
            Vec3(x0, 0.012, front_z),
            Vec3(x1 - x0, 0.0, 0.0),
            Vec3(0.0, 0.0, -7.55 - front_z),
            bed_soil
        ));

        // Low stone curb along the wandering bed edge.
        greenhouse_box(
            world,
            Vec3(x0, 0.0, front_z - 0.075),
            Vec3(x1, 0.10, front_z + 0.02),
            stone
        );
    };

    add_bed_segment(-3.45, -1.85, -6.55);
    add_bed_segment(-1.85, -0.15, -6.40);
    add_bed_segment(-0.15, 1.75, -6.62);
    add_bed_segment(1.75, 3.45, -6.45);

    // Weathered stones scattered along the bed edge.
    const auto add_bed_stone = [&](double x, double z, double radius) {
        world.add(std::make_shared<Sphere>(
            Vec3(x, radius * 0.55, z),
            radius,
            stone
        ));
    };

    add_bed_stone(-2.85, -6.34, 0.09);
    add_bed_stone(-2.05, -6.28, 0.07);
    add_bed_stone(-1.15, -6.24, 0.11);
    add_bed_stone(0.85, -6.44, 0.08);
    add_bed_stone(1.75, -6.30, 0.12);
    add_bed_stone(2.72, -6.26, 0.07);

    // Corner shrubs close the understory mass against the walls.
    add_shrub(
        "assets/shrub_04_2k.blend/shrub_04_2k.obj",
        -3.10,
        -6.70,
        0.65
    );

    add_shrub(
        "assets/shrub_04_2k.blend/shrub_04_2k.obj",
        3.05,
        -6.75,
        0.70
    );

    // Low ferns knit the shrubs together at ground level.
    const auto add_bed_fern = [&](double x, double z, double height) {
        TriangleMesh bed_fern = greenhouse_load_plant_obj(
            "assets/fern_02_2k.blend/fern_02_2k.obj",
            fern_fallback,
            height,
            Vec3(x, 0.02, z)
        );

        world.add(
            std::make_shared<TriangleMesh>(
                std::move(bed_fern)
            )
        );
    };

    add_bed_fern(-3.15, -6.90, 0.55);
    add_bed_fern(-1.55, -6.65, 0.50);
    add_bed_fern(0.42, -6.95, 0.52);
    add_bed_fern(3.02, -6.55, 0.48);

    // A second pair of calatheas softens the bed front.
    add_calathea(-2.85, -5.55, 0.02);
    add_calathea(2.75, -5.90, 0.02);

    // Interior water features and stone dressing.
    // Water is a closed dielectric volume so refraction, Fresnel
    // reflections and absorption all behave like real water.
    auto water_surface = std::make_shared<Dielectric>(
        1.33,
        Vec3(0.92, 0.98, 0.96),
        Vec3(0.9, 0.25, 0.15)
    );

    auto basin_floor = std::make_shared<Lambertian>(
        Vec3(0.05, 0.045, 0.038)
    );

    auto rock_fallback = std::make_shared<PrincipledBSDF>(
        Vec3(0.28, 0.27, 0.25),
        0.0,
        0.78
    );

    auto moss_fallback = std::make_shared<PrincipledBSDF>(
        Vec3(0.10, 0.18, 0.05),
        0.0,
        0.85
    );

    // Round cistern on the open floor left of the walkway.
    greenhouse_add_cistern(
        world,
        -1.45,
        1.55,
        stone,
        water_surface,
        basin_floor
    );

    // Rectangular trough tucked into the front-right corner.
    greenhouse_add_stone_trough(
        world,
        2.72,
        2.32,
        3.52,
        3.32,
        stone,
        water_surface,
        basin_floor
    );

    // Mossy boulders from the imported rock set.
    const auto add_rock = [&](
        const std::string& rock_name,
        double x,
        double z,
        double width,
        double base_y
    ) {
        TriangleMesh rock = greenhouse_load_plant_obj(
            "assets/rock_moss_set_01_2k.blend/rock_moss_set_01_2k.obj",
            rock_fallback,
            0.0,
            Vec3(x, base_y, z),
            { rock_name },
            width
        );

        world.add(
            std::make_shared<TriangleMesh>(
                std::move(rock)
            )
        );
    };

    add_rock("rock_moss_set_01_rock03", -1.10, 2.45, 0.50, 0.0);
    add_rock("rock_moss_set_01_rock01", -2.05, 0.30, 0.62, 0.0);
    add_rock("rock_moss_set_01_rock02", -2.85, -6.80, 0.45, 0.02);
    add_rock("rock_moss_set_01_rock04", 0.95, -6.90, 0.55, 0.02);
    add_rock("rock_moss_set_01_rock05", 2.60, -6.72, 0.42, 0.02);
    add_rock("rock_moss_set_01_rock06", 3.10, -2.30, 0.45, 0.0);

    // Moss tufts layer the bed, wall bases and stone edges.
    const auto add_moss = [&](
        const std::string& clump_name,
        double x,
        double y,
        double z,
        double size
    ) {
        TriangleMesh clump = greenhouse_load_plant_obj(
            "assets/moss_01_2k.blend/moss_01_2k.obj",
            moss_fallback,
            size,
            Vec3(x, y, z),
            { clump_name }
        );

        world.add(
            std::make_shared<TriangleMesh>(
                std::move(clump)
            )
        );
    };

    // Around the cistern.
    add_moss("moss_01_c_LOD0", -1.90, 0.365, 1.25, 0.05);
    add_moss("moss_01_e_LOD0", -1.05, 0.365, 1.95, 0.045);
    add_moss("moss_01_tall_b_LOD0", -1.80, 0.365, 1.90, 0.055);
    add_moss("moss_01_a_LOD0", -1.90, 0.005, 2.10, 0.07);
    add_moss("moss_01_b_LOD0", -0.95, 0.005, 1.05, 0.06);
    add_moss("moss_01_d_LOD0", -2.05, 0.005, 1.75, 0.075);

    // At the pachira base.
    add_moss("moss_01_d_LOD0", -0.45, 0.02, -5.80, 0.08);
    add_moss("moss_01_g_LOD0", 0.50, 0.02, -5.65, 0.07);
    add_moss("moss_01_tall_a_LOD0", 0.05, 0.02, -5.45, 0.06);
    add_moss("moss_01_e_LOD0", -0.20, 0.02, -5.90, 0.075);

    // Rear bed ground layer.
    add_moss("moss_01_f_LOD0", -2.55, 0.02, -6.90, 0.09);
    add_moss("moss_01_h_LOD0", -1.55, 0.02, -7.05, 0.08);
    add_moss("moss_01_i_LOD0", -0.55, 0.02, -6.85, 0.075);
    add_moss("moss_01_g_LOD0", 1.45, 0.02, -6.95, 0.085);
    add_moss("moss_01_j_LOD0", 2.10, 0.02, -6.80, 0.08);
    add_moss("moss_01_i_LOD0", 2.95, 0.02, -7.00, 0.09);
    add_moss("moss_01_c_LOD0", -3.20, 0.02, -6.65, 0.07);
    add_moss("moss_01_tall_b_LOD0", 1.30, 0.02, -7.25, 0.095);

    // Along the side wall bases.
    add_moss("moss_01_a_LOD0", -3.30, 0.005, -2.20, 0.075);
    add_moss("moss_01_b_LOD0", -3.35, 0.005, -4.90, 0.08);
    add_moss("moss_01_f_LOD0", 3.30, 0.005, -4.30, 0.075);
    add_moss("moss_01_e_LOD0", 3.35, 0.005, -1.10, 0.07);

    // Beside the trough.
    add_moss("moss_01_f_LOD0", 2.45, 0.005, 3.35, 0.07);
    add_moss("moss_01_h_LOD0", 3.30, 0.005, 2.15, 0.075);

    // Layered planting pass: the imported shrub 01 variants knit
    // the bed into a deep understory, ferns climb the wall bases,
    // and periwinkle accents edge the walkway.
    auto periwinkle_fallback = std::make_shared<PrincipledBSDF>(
        Vec3(0.30, 0.20, 0.30),
        0.0,
        0.60
    );

    const auto add_shrub01 = [&](
        const std::string& variant,
        double x,
        double z,
        double height,
        double base_y
    ) {
        TriangleMesh plant = greenhouse_load_plant_obj(
            "assets/shrub_01_2k.blend/shrub_01_2k.obj",
            shrub_fallback,
            height,
            Vec3(x, base_y, z),
            { "shrub_01_" + variant + "_LOD0" }
        );

        world.add(
            std::make_shared<TriangleMesh>(
                std::move(plant)
            )
        );
    };

    const auto add_periwinkle = [&](
        const std::string& variant,
        double x,
        double z,
        double height,
        double base_y
    ) {
        TriangleMesh plant = greenhouse_load_plant_obj(
            "assets/periwinkle_plant_2k.blend/"
            "periwinkle_plant_2k.obj",
            periwinkle_fallback,
            height,
            Vec3(x, base_y, z),
            { "periwinkle_plant_" + variant + "_LOD0" }
        );

        world.add(
            std::make_shared<TriangleMesh>(
                std::move(plant)
            )
        );
    };

    // Rear understory, filled between the established shrubs.
    add_shrub01("b", 0.00, -6.88, 0.70, 0.02);
    add_shrub01("c", -1.68, -7.12, 0.75, 0.02);
    add_shrub01("d", 1.70, -7.02, 0.80, 0.02);
    add_shrub01("e", -3.15, -7.18, 0.60, 0.02);
    add_shrub01("a", 2.85, -7.05, 0.72, 0.02);
    add_shrub01("h", -2.85, -7.32, 0.55, 0.02);
    add_shrub01("i", 1.78, -7.22, 0.50, 0.02);

    // Side wall bases: ferns rise over the moss layer.
    add_bed_fern(-3.25, -0.10, 0.72);
    add_bed_fern(-3.25, -5.70, 0.66);
    add_bed_fern(3.25, -0.15, 0.72);
    add_bed_fern(3.30, -4.60, 0.68);
    add_shrub01("f", -3.12, -3.80, 0.62, 0.005);
    add_shrub01("g", 3.15, 1.10, 0.70, 0.005);

    // Periwinkle border along the walkway edges.
    add_periwinkle("01", 1.02, 2.30, 0.44, 0.005);
    add_periwinkle("02", -1.05, 1.90, 0.38, 0.005);
    add_periwinkle("03", 1.08, 0.55, 0.40, 0.005);
    add_periwinkle("04", -0.98, -0.35, 0.46, 0.005);
    add_periwinkle("05", 1.05, -1.65, 0.38, 0.005);
    add_periwinkle("06", -1.06, -3.35, 0.42, 0.005);
    add_periwinkle("01", 1.02, -3.70, 0.45, 0.005);
    add_periwinkle("02", -1.20, -6.70, 0.42, 0.02);
    add_periwinkle("03", 2.10, -6.72, 0.40, 0.02);
    add_periwinkle("04", -0.85, -5.30, 0.42, 0.005);

    // Two flowering pots on the bench lower shelf.
    greenhouse_add_tapered_pot(
        world,
        -3.24,
        2.00,
        0.075,
        0.105,
        0.16,
        terracotta,
        0.185
    );

    greenhouse_add_tapered_pot(
        world,
        -2.79,
        2.62,
        0.075,
        0.105,
        0.16,
        terracotta,
        0.185
    );

    world.add(std::make_shared<Quad>(
        Vec3(-3.14, 0.333, 2.10),
        Vec3(-0.20, 0.0, 0.0),
        Vec3(0.0, 0.0, -0.20),
        fern_soil
    ));

    world.add(std::make_shared<Quad>(
        Vec3(-2.69, 0.333, 2.72),
        Vec3(-0.20, 0.0, 0.0),
        Vec3(0.0, 0.0, -0.20),
        fern_soil
    ));

    add_periwinkle("05", -3.24, 2.00, 0.20, 0.333);
    add_periwinkle("06", -2.79, 2.62, 0.18, 0.333);

    // Tropical ground layer: continuous planted beds run along both
    // walls from the rear border up to the entrance corners, so the
    // house reads as jungle growth instead of arranged pots.
    const auto add_side_bed_segment = [&](
        double z0,
        double z1,
        double walkway_edge,
        int side,
        bool with_curb
    ) {
        const double soil_inner = side * walkway_edge;
        const double soil_outer = side * 3.50;

        world.add(std::make_shared<Quad>(
            Vec3(soil_inner, 0.012, z1),
            Vec3(soil_outer - soil_inner, 0.0, 0.0),
            Vec3(0.0, 0.0, z0 - z1),
            bed_soil
        ));

        if (with_curb) {
            greenhouse_box(
                world,
                Vec3(
                    soil_inner - side * 0.045,
                    0.0,
                    z0
                ),
                Vec3(
                    soil_inner + side * 0.045,
                    0.10,
                    z1
                ),
                stone
            );
        }
    };

    // Left wall beds (the display bench backs onto the last strip).
    add_side_bed_segment(-6.45, -4.45, 0.96, -1, true);
    add_side_bed_segment(-4.45, -2.45, 0.90, -1, true);
    add_side_bed_segment(-2.45, -0.45, 0.98, -1, true);
    add_side_bed_segment(-0.45, 1.25, 0.92, -1, true);
    add_side_bed_segment(1.25, 3.50, 2.15, -1, false);

    // Right wall beds (the water trough sits inside the bed).
    add_side_bed_segment(-6.45, -4.45, 0.94, 1, true);
    add_side_bed_segment(-4.45, -2.45, 1.00, 1, true);
    add_side_bed_segment(-2.45, -0.45, 0.92, 1, true);
    add_side_bed_segment(-0.45, 1.55, 0.96, 1, true);
    add_side_bed_segment(1.55, 3.45, 0.90, 1, true);

    // Left bed planting: ferns rising out of the ground layer.
    add_ground_fern(-3.15, 0.55, 0.75, 0.02);
    add_ground_fern(-3.05, -1.05, 0.90, 0.02);
    add_ground_fern(-3.15, -2.20, 0.70, 0.02);
    add_ground_fern(-2.85, -3.55, 0.80, 0.02);
    add_ground_fern(-3.20, -5.05, 0.95, 0.02);
    add_ground_fern(-3.15, 2.60, 0.85, 0.02);
    add_calathea(-1.60, -0.80, 0.02);
    add_calathea(-1.55, 0.15, 0.02);
    add_shrub01("c", -2.20, -0.55, 0.65, 0.02);
    add_shrub01("a", -2.75, 0.35, 0.60, 0.02);
    add_moss("moss_01_f_LOD0", -3.35, 0.02, 0.10, 0.08);
    add_moss("moss_01_b_LOD0", -2.35, 0.02, -1.60, 0.09);
    add_moss("moss_01_d_LOD0", -1.35, 0.02, -1.90, 0.08);
    add_moss("moss_01_j_LOD0", -2.45, 0.02, -3.15, 0.075);
    add_moss("moss_01_g_LOD0", -1.55, 0.02, -4.40, 0.085);
    add_moss("moss_01_a_LOD0", -3.25, 0.02, -5.60, 0.075);
    add_periwinkle("05", -1.15, -2.60, 0.36, 0.02);
    add_periwinkle("02", -1.20, 0.35, 0.34, 0.02);
    add_periwinkle("04", -2.90, -0.30, 0.38, 0.02);

    // Right bed planting.
    add_ground_fern(3.10, -1.95, 0.85, 0.02);
    add_ground_fern(3.20, 2.00, 0.75, 0.02);
    add_ground_fern(3.15, -4.90, 0.90, 0.02);
    add_calathea(1.60, -1.20, 0.02);
    add_calathea(1.45, -4.90, 0.02);
    add_shrub01("b", 1.30, 2.10, 0.60, 0.02);
    add_shrub01("h", 2.60, -4.30, 0.70, 0.02);
    add_moss("moss_01_e_LOD0", 1.25, 0.02, -0.60, 0.08);
    add_moss("moss_01_i_LOD0", 2.20, 0.02, -1.75, 0.085);
    add_moss("moss_01_c_LOD0", 3.30, 0.02, -0.65, 0.075);
    add_moss("moss_01_h_LOD0", 1.45, 0.02, -3.90, 0.08);
    add_moss("moss_01_tall_a_LOD0", 2.85, 0.02, -3.10, 0.075);
    add_moss("moss_01_f_LOD0", 1.15, 0.02, 1.60, 0.075);
    add_periwinkle("06", 1.15, -2.80, 0.38, 0.02);
    add_periwinkle("03", 1.10, 1.30, 0.36, 0.02);
    add_periwinkle("01", 3.30, 1.20, 0.38, 0.02);
    add_periwinkle("05", 2.10, 2.85, 0.36, 0.02);

    // Entrance corners framed with planted foliage.
    add_shrub01("d", 1.90, 3.40, 0.55, 0.02);
    add_ground_fern(3.15, 3.30, 0.80, 0.02);
    add_moss("moss_01_tall_b_LOD0", -2.60, 0.02, 3.00, 0.075);
    add_moss("moss_01_i_LOD0", -3.30, 0.02, 1.60, 0.08);

    // Climbing vines wind up the wall columns, growing inward so
    // they stay clear of the glazing.
    const auto add_column_vine = [&](
        double column_x,
        double column_z,
        double inward_x,
        int seed
    ) {
        const auto vine_variation = [seed](int index) {
            const double value = std::sin(
                seed * 12.9898 + index * 78.233
            ) * 43758.5453;

            return value - std::floor(value);
        };

        for (int level = 0; level < 8; ++level) {
            const double height =
                0.35 + 0.32 * static_cast<double>(level);

            const double angle =
                1.1 * static_cast<double>(level)
                + 0.8 * vine_variation(level);

            const Vec3 base(
                column_x + inward_x * 0.07,
                height,
                column_z + 0.07 * std::sin(angle)
            );

            const Vec3 direction(
                inward_x * (0.26 + 0.10 * vine_variation(level + 8)),
                0.16 + 0.05 * vine_variation(level + 16),
                0.14 * std::cos(angle)
            );

            greenhouse_add_leaf(
                world,
                base,
                direction,
                0.15,
                -0.06,
                (level % 2 == 0) ? vine_a : vine_b,
                false
            );
        }
    };

    add_column_vine(3.78, 3.00, -1.0, 51);
    add_column_vine(-3.78, 3.00, 1.0, 67);
    add_column_vine(3.78, -6.00, -1.0, 83);
    add_column_vine(-3.78, -6.00, 1.0, 97);

    // Side exhibition plinth.
    greenhouse_box(
        world,
        Vec3(1.25, 0.0, -3.35),
        Vec3(2.55, 0.72, -2.05),
        stone
    );

    greenhouse_box(
        world,
        Vec3(1.18, 0.72, -3.42),
        Vec3(2.62, 0.82, -1.98),
        stone
    );

    // Large soft fill light, facing downward.
    // The panel stays inside the roof envelope: its outer corners
    // must remain below the glazing line 5.98 - 0.485 * |x|.
    auto emitter = std::make_shared<DiffuseLight>(
        Vec3(0.85, 0.95, 0.80)
    );

    world.add(std::make_shared<Quad>(
        Vec3(-1.2, 5.28, -4.0),
        Vec3(2.4, 0.0, 0.0),
        Vec3(0.0, 0.0, 2.0),
        emitter
    ));

    return world;
}
