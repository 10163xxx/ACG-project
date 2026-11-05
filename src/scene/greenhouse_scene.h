#pragma once

#include <memory>
#include <array>
#include <cmath>
#include <vector>

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

inline void greenhouse_add_stone_path(
    HittableList& world,
    const std::shared_ptr<const Material>& stone_material,
    const std::shared_ptr<const Material>& joint_material
) {
    constexpr int ROWS = 7;
    constexpr int COLUMNS = 2;
    constexpr int SIDES = 6;

    constexpr double PATH_CENTER_X = 0.0;
    constexpr double FIRST_ROW_Z = 2.75;

    constexpr double COLUMN_OFFSET = 0.34;
    constexpr double ROW_SPACING = 0.88;

    constexpr double HALF_WIDTH = 0.295;
    constexpr double HALF_LENGTH = 0.375;

    constexpr double BOTTOM_Y = 0.008;
    constexpr double TOP_Y = 0.105;

    constexpr double PI = 3.14159265358979323846;

    std::vector<Vec3> vertices;
    std::vector<TriangleMesh::Face> faces;

    const int slab_count = ROWS * COLUMNS;

    vertices.reserve(
        static_cast<std::size_t>(slab_count)
        * (2 * SIDES + 2)
    );

    faces.reserve(
        static_cast<std::size_t>(slab_count)
        * (4 * SIDES)
    );

    const auto variation = [](
        int row,
        int column,
        int index
    ) {
        const double value =
            std::sin(
                row * 12.9898
                + column * 78.233
                + index * 37.719
            ) * 43758.5453;

        return value - std::floor(value);
    };

    // Dark substrate remains visible through the stone joints.
    world.add(std::make_shared<Quad>(
        Vec3(-0.78, 0.003, 3.15),
        Vec3(1.56, 0.0, 0.0),
        Vec3(0.0, 0.0, -6.35),
        joint_material
    ));

    for (int row = 0; row < ROWS; ++row) {
        for (int column = 0; column < COLUMNS; ++column) {
            const double row_shift =
                (row % 2 == 0) ? 0.0 : 0.055;

            const double center_x =
                PATH_CENTER_X
                + (column == 0 ? -COLUMN_OFFSET : COLUMN_OFFSET)
                + row_shift;

            const double center_z =
                FIRST_ROW_Z - row * ROW_SPACING;

            const double width_variation =
                0.94 + 0.10 * variation(row, column, 0);

            const double length_variation =
                0.94 + 0.10 * variation(row, column, 1);

            const double rotation =
                (variation(row, column, 2) - 0.5) * 0.10;

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
                TOP_Y + 0.012 * (variation(row, column, 3) - 0.5),
                center_z
            );

            for (int side = 0; side < SIDES; ++side) {
                const double angle =
                    -2.0 * PI * side / SIDES;

                const double corner_x =
                    std::cos(angle)
                    * HALF_WIDTH
                    * width_variation;

                const double corner_z =
                    std::sin(angle)
                    * HALF_LENGTH
                    * length_variation;

                const double local_x =
                    corner_x * cos_rotation
                    - corner_z * sin_rotation;

                const double local_z =
                    corner_x * sin_rotation
                    + corner_z * cos_rotation;

                const double edge_variation =
                    0.94 + 0.12 * variation(
                        row,
                        column,
                        side + 4
                    );

                const double x =
                    center_x + local_x * edge_variation;

                const double z =
                    center_z + local_z * edge_variation;

                const double top_offset =
                    0.008 * (
                        variation(row, column, side + 12) - 0.5
                    );

                vertices.emplace_back(
                    x,
                    BOTTOM_Y,
                    z
                );

                vertices.emplace_back(
                    x,
                    TOP_Y + top_offset,
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
        stone_material
    ));
}

inline void greenhouse_add_leaf(
    HittableList& world,
    const Vec3& base,
    const Vec3& direction,
    double width,
    double curvature,
    const std::shared_ptr<const Material>& material
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
    const std::shared_ptr<const Material>& material
) {
    const double y0 = 0.0;
    const double y1 = height;

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

struct GreenhousePlantPlacement {
    Vec3 position;
    double target_height;
};

inline HittableList make_greenhouse_scene() {
    HittableList world;

    auto floor_texture = std::make_shared<PerlinNoiseTexture>(
        Vec3(0.22, 0.215, 0.19),
        Vec3(0.39, 0.37, 0.32),
        0.72,
        5,
        417
    );

    auto floor = std::make_shared<Lambertian>(
        floor_texture
    );

    auto stone_path_texture =
        std::make_shared<PerlinNoiseTexture>(
            Vec3(0.22, 0.205, 0.175),
            Vec3(0.36, 0.33, 0.28),
            4.5,
            5,
            2026
        );

    auto stone_path_material =
        std::make_shared<Lambertian>(
            stone_path_texture
        );

    auto stone_joint_material =
        std::make_shared<Lambertian>(
            Vec3(0.075, 0.060, 0.042)
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
        1.52,
        Vec3(0.96, 0.985, 1.0)
    );

    world.add(std::make_shared<Quad>(
        Vec3(-4, 0, 4), Vec3(8, 0, 0), Vec3(0, 0, -12), floor));

    greenhouse_add_stone_path(
        world,
        stone_path_material,
        stone_joint_material
    );

    // Exterior landscape surrounding the conservatory.
    auto exterior_ground_texture = std::make_shared<PerlinNoiseTexture>(
        Vec3(0.075, 0.105, 0.045),
        Vec3(0.22, 0.27, 0.12),
        0.38,
        5,
        913
    );

    auto exterior_ground = std::make_shared<Lambertian>(
        exterior_ground_texture
    );

    auto exterior_path = std::make_shared<PrincipledBSDF>(
        Vec3(0.34, 0.32, 0.27),
        0.0,
        0.88
    );

    world.add(std::make_shared<Quad>(
        Vec3(-30.0, -0.08, 30.0),
        Vec3(60.0, 0.0, 0.0),
        Vec3(0.0, 0.0, -60.0),
        exterior_ground
    ));

    // A stone approach path outside the entrance.
    world.add(std::make_shared<Quad>(
        Vec3(-1.25, -0.035, 15.0),
        Vec3(2.5, 0.0, 0.0),
        Vec3(0.0, 0.0, -11.0),
        exterior_path
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

            world.add(std::make_shared<Quad>(
                Vec3(side_glass_left_x, y_start, z_start),
                pane_u,
                pane_v,
                glass_pane
            ));

            world.add(std::make_shared<Quad>(
                Vec3(side_glass_right_x, y_start, z_start),
                pane_u,
                pane_v,
                glass_pane
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
    TriangleMesh anthurium = ObjLoader::load_file(
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

    auto add_fern = [&](double x, double z, double height) {
        const double bottom_half_width = 0.27;
        const double top_half_width = 0.36;
        const double pot_height = 0.42;

        greenhouse_add_tapered_pot(
            world,
            x,
            z,
            bottom_half_width,
            top_half_width,
            pot_height,
            fern_planter
        );

        world.add(std::make_shared<Quad>(
            Vec3(
                x - top_half_width * 0.88,
                pot_height - 0.025,
                z - top_half_width * 0.88
            ),
            Vec3(top_half_width * 1.76, 0.0, 0.0),
            Vec3(0.0, 0.0, top_half_width * 1.76),
            fern_soil
        ));

        TriangleMesh fern = ObjLoader::load_file(
            "assets/fern_02_2k.blend/fern_02_2k.obj",
            fern_fallback,
            height,
            Vec3(x, pot_height + 0.01, z)
        );

        world.add(
            std::make_shared<TriangleMesh>(
                std::move(fern)
            )
        );
    };

    // Ferns: foreground and rear exhibition zones.
    const std::array<GreenhousePlantPlacement, 4> fern_layout = {{
        {
            Vec3(-2.35, 0.0, 0.75),
            1.45
        },
        {
            Vec3(2.85, 0.0, -0.55),
            1.45
        },
        {
            Vec3(-2.45, 0.0, -4.35),
            1.85
        },
        {
            Vec3(2.75, 0.0, -4.80),
            1.90
        }
    }};

    for (const GreenhousePlantPlacement& placement : fern_layout) {
        add_fern(
            placement.position.x,
            placement.position.z,
            placement.target_height
        );
    }

    // Foreground Monstera: the source model includes its own planter.
    auto monstera_fallback =
        std::make_shared<PrincipledBSDF>(
            Vec3(0.075, 0.24, 0.045),
            0.0,
            0.52
        );

    TriangleMesh monstera = ObjLoader::load_file(
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

    TriangleMesh pachira = ObjLoader::load_file(
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
        TriangleMesh shrub = ObjLoader::load_file(
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

    // Shrub 03: wider, low-growing foliage.
    add_shrub(
        "assets/shrub_03_2k.blend/shrub_03_2k.obj",
        -1.45,
        -7.25,
        1.55
    );

    // Shrub 04: compact foliage clusters.
    add_shrub(
        "assets/shrub_04_2k.blend/shrub_04_2k.obj",
        0.0,
        -7.25,
        1.05
    );

    add_shrub(
        "assets/shrub_04_2k.blend/shrub_04_2k.obj",
        1.0,
        -7.25,
        1.05
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

    auto add_calathea = [&](double x, double z) {
        const double pot_height = 0.40;

        greenhouse_add_tapered_pot(
            world,
            x,
            z,
            0.43,
            0.54,
            pot_height,
            calathea_pot
        );

        world.add(std::make_shared<Quad>(
            Vec3(x - 0.49, pot_height - 0.025, z - 0.49),
            Vec3(0.98, 0.0, 0.0),
            Vec3(0.0, 0.0, 0.98),
            fern_soil
        ));

        TriangleMesh calathea = ObjLoader::load_file(
            "assets/calathea_orbifolia_01_2k.blend/"
            "calathea_orbifolia_01_2k.obj",
            calathea_fallback,
            0.0,
            Vec3(x, pot_height, z),
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

    // A single broad-leaf specimen frames the left exhibition area.
    add_calathea(-2.40, -2.25);

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
    auto emitter = std::make_shared<DiffuseLight>(
        Vec3(0.85, 0.95, 0.80)
    );

    world.add(std::make_shared<Quad>(
        Vec3(-1.8, 5.35, -4.0),
        Vec3(3.6, 0.0, 0.0),
        Vec3(0.0, 0.0, 2.0),
        emitter
    ));

    return world;
}
