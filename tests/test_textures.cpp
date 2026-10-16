#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

#include "core/random.h"
#include "materials/texture.h"
#include "materials/lambertian.h"
#include "materials/normal_mapped_lambertian.h"

namespace {
    bool approximately_equal(double a, double b) {
        constexpr double epsilon = 1e-9;
        return std::abs(a - b) < epsilon;
    }

    bool color_equal(const Vec3& a, const Vec3& b) {
        return approximately_equal(a.x, b.x)
            && approximately_equal(a.y, b.y)
            && approximately_equal(a.z, b.z);
    }

    void test_solid_color() {
        const Vec3 expected(0.2, 0.4, 0.8);
        SolidColor texture(expected);

        assert(color_equal(
            texture.value(0.0, 0.0, Vec3(1.0, 2.0, 3.0)),
            expected
        ));

        assert(color_equal(
            texture.value(0.7, 0.9, Vec3(-1.0, 0.0, 2.0)),
            expected
        ));
    }

    void test_uv_checker() {
        const Vec3 color_a(1.0, 0.0, 0.0);
        const Vec3 color_b(0.0, 1.0, 0.0);

        UVCheckerTexture texture(2.0, color_a, color_b);

        assert(color_equal(
            texture.value(0.1, 0.1, Vec3()),
            color_a
        ));

        assert(color_equal(
            texture.value(0.6, 0.1, Vec3()),
            color_b
        ));

        assert(color_equal(
            texture.value(0.6, 0.6, Vec3()),
            color_a
        ));
    }

    void test_uv_gradient() {
        const Vec3 bottom(0.0, 0.0, 0.0);
        const Vec3 top(1.0, 0.5, 0.25);

        UVGradientTexture texture(bottom, top);

        assert(color_equal(
            texture.value(0.3, 0.0, Vec3()),
            bottom
        ));

        assert(color_equal(
            texture.value(0.3, 1.0, Vec3()),
            top
        ));

        const Vec3 middle = texture.value(
            0.3, 0.5, Vec3()
        );

        assert(color_equal(
            middle,
            Vec3(0.5, 0.25, 0.125)
        ));

        // Values outside [0, 1] are clamped.
        assert(color_equal(
            texture.value(0.3, -0.5, Vec3()),
            bottom
        ));

        assert(color_equal(
            texture.value(0.3, 1.5, Vec3()),
            top
        ));
    }

    void test_lambertian_uses_texture() {
        Random::seed(20261016);

        const Vec3 color_a(0.8, 0.2, 0.1);
        const Vec3 color_b(0.1, 0.7, 0.3);

        auto texture = std::make_shared<UVCheckerTexture>(
            2.0, color_a, color_b
        );

        Lambertian material(texture);

        const Ray incoming(
            Vec3(0.0, 1.0, 0.0),
            Vec3(0.0, -1.0, 0.0)
        );

        HitRecord hit;
        hit.point = Vec3(0.0, 0.0, 0.0);
        hit.normal = Vec3(0.0, 1.0, 0.0);
        hit.u = 0.6;
        hit.v = 0.1;

        Vec3 attenuation;
        Ray scattered;

        const bool scattered_ok = material.scatter(
            incoming, hit, attenuation, scattered
        );

        assert(scattered_ok);

        assert(color_equal(attenuation, color_b));

        assert(scattered.direction.length_squared() > 0.0);
    }

    
    void test_normal_mapping() {
        const Ray incoming(
            Vec3(0.0, 0.0, 2.0),
            Vec3(0.0, 0.0, -1.0));

        HitRecord hit;
        hit.point = Vec3(0.0, 0.0, 1.0);
        hit.normal = Vec3(0.0, 0.0, 1.0);
        hit.front_face = true;
        hit.u = 0.5;
        hit.v = 0.5;

        auto neutral_map = std::make_shared<SolidColor>(
            Vec3(0.5, 0.5, 1.0));

        NormalMappedLambertian neutral_material(
            Vec3(0.8, 0.7, 0.6), neutral_map);

        const Vec3 neutral_normal =
            neutral_material.mapped_normal(incoming, hit);

        assert(approximately_equal(neutral_normal.x, 0.0));
        assert(approximately_equal(neutral_normal.y, 0.0));
        assert(approximately_equal(neutral_normal.z, 1.0));

        auto tilted_map = std::make_shared<SolidColor>(
            Vec3(1.0, 0.5, 0.5));

        NormalMappedLambertian tilted_material(
            Vec3(0.8, 0.7, 0.6), tilted_map);

        const Vec3 tilted_normal =
            tilted_material.mapped_normal(incoming, hit);

        assert(tilted_normal.x > 0.99);
        assert(std::abs(tilted_normal.y) < 1e-9);
        assert(std::abs(tilted_normal.z) < 1e-9);

        Random::seed(20261016);

        Vec3 attenuation;
        Ray scattered;

        assert(tilted_material.scatter(
            incoming, hit, attenuation, scattered));

        assert(color_equal(attenuation, Vec3(0.8, 0.7, 0.6)));
        assert(scattered.direction.length_squared() > 0.0);
    }

    void test_normal_mapping_tangent_orientation() {
        const Ray incoming(
            Vec3(2.0, 0.0, 0.0),
            Vec3(-1.0, 0.0, 0.0));

        HitRecord hit;
        hit.point = Vec3(1.0, 0.0, 0.0);
        hit.normal = Vec3(1.0, 0.0, 0.0);
        hit.front_face = true;
        hit.u = 0.5;
        hit.v = 0.5;

        // Tangent-space +X should follow increasing u.
        auto u_map = std::make_shared<SolidColor>(
            Vec3(1.0, 0.5, 0.5));

        NormalMappedLambertian u_material(
            Vec3(0.8, 0.7, 0.6), u_map);

        const Vec3 u_normal =
            u_material.mapped_normal(incoming, hit);

        assert(std::abs(u_normal.x) < 1e-9);
        assert(std::abs(u_normal.y) < 1e-9);
        assert(u_normal.z < -0.99);

        // Tangent-space +Y should follow increasing v.
        auto v_map = std::make_shared<SolidColor>(
            Vec3(0.5, 1.0, 0.5));

        NormalMappedLambertian v_material(
            Vec3(0.8, 0.7, 0.6), v_map);

        const Vec3 v_normal =
            v_material.mapped_normal(incoming, hit);

        assert(std::abs(v_normal.x) < 1e-9);
        assert(v_normal.y > 0.99);
        assert(std::abs(v_normal.z) < 1e-9);
    }


    void test_normal_mapping_stays_in_geometric_hemisphere() {
        Random::seed(20261016);

        const Ray incoming(
            Vec3(0.0, 0.0, 2.0),
            Vec3(0.0, 0.0, -1.0));

        HitRecord hit;
        hit.point = Vec3(0.0, 0.0, 1.0);
        hit.normal = Vec3(0.0, 0.0, 1.0);
        hit.front_face = true;
        hit.u = 0.5;
        hit.v = 0.5;

        // Strongly tilted tangent-space normal.
        auto normal_map = std::make_shared<SolidColor>(
            Vec3(1.0, 0.5, 0.5));

        NormalMappedLambertian material(
            Vec3(0.8, 0.7, 0.6), normal_map);

        Vec3 attenuation;
        Ray scattered;

        // Test many random samples to catch directions
        // that incorrectly cross the geometric surface.
        for (int i = 0; i < 1000; ++i) {
            const bool ok = material.scatter(
                incoming, hit, attenuation, scattered);

            assert(ok);

            const double geometric_side =
                scattered.direction.x * hit.normal.x
                + scattered.direction.y * hit.normal.y
                + scattered.direction.z * hit.normal.z;

            assert(geometric_side > 0.0);
        }
    }
}

int main() {
    test_solid_color();
    test_uv_checker();
    test_uv_gradient();
    test_lambertian_uses_texture();
    test_normal_mapping();
    test_normal_mapping_tangent_orientation();
    test_normal_mapping_stays_in_geometric_hemisphere();
    std::cout << "Texture tests passed.\n";
    return 0;
}
