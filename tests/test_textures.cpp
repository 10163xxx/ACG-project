#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <cstdio>
#include <fstream>

#include "core/random.h"
#include "materials/texture.h"
#include "materials/lambertian.h"
#include "materials/normal_mapped_lambertian.h"
#include "materials/image_texture.h"
#include "materials/texture_transform.h"

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

        NormalMappedMaterial neutral_material(
            std::make_shared<Lambertian>(
                Vec3(0.8, 0.7, 0.6)
            ),
            neutral_map
        );

        const Vec3 neutral_normal =
            neutral_material.mapped_normal(hit);

        assert(approximately_equal(neutral_normal.x, 0.0));
        assert(approximately_equal(neutral_normal.y, 0.0));
        assert(approximately_equal(neutral_normal.z, 1.0));

        auto tilted_map = std::make_shared<SolidColor>(
            Vec3(1.0, 0.5, 0.5));

        NormalMappedMaterial tilted_material(
            std::make_shared<Lambertian>(
                Vec3(0.8, 0.7, 0.6)
            ),
            tilted_map
        );

        const Vec3 tilted_normal =
            tilted_material.mapped_normal(hit);

        assert(tilted_normal.x > 0.99);
        assert(std::abs(tilted_normal.y) < 1e-9);
        assert(std::abs(tilted_normal.z) < 1e-9);

        Random::seed(20261016);
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

        NormalMappedMaterial u_material(
            std::make_shared<Lambertian>(
                Vec3(0.8, 0.7, 0.6)
            ),
            u_map
        );

        const Vec3 u_normal =
            u_material.mapped_normal(hit);

        assert(std::abs(u_normal.x) < 1e-9);
        assert(std::abs(u_normal.y) < 1e-9);
        assert(u_normal.z < -0.99);

        // Tangent-space +Y should follow increasing v.
        auto v_map = std::make_shared<SolidColor>(
            Vec3(0.5, 1.0, 0.5));

        NormalMappedMaterial v_material(
            std::make_shared<Lambertian>(
                Vec3(0.8, 0.7, 0.6)
            ),
            v_map
        );

        const Vec3 v_normal =
            v_material.mapped_normal(hit);

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

        NormalMappedMaterial material(
            std::make_shared<Lambertian>(
                Vec3(0.8, 0.7, 0.6)
            ),
            normal_map
        );

        Vec3 attenuation;
        Ray scattered;

        int valid_samples = 0;
        int rejected_samples = 0;

        for (int i = 0; i < 1000; ++i) {
            const bool ok = material.scatter(
                incoming,
                hit,
                attenuation,
                scattered
            );

            if (!ok) {
                ++rejected_samples;
                continue;
            }

            ++valid_samples;

            const double geometric_side =
                dot(scattered.direction, hit.normal);

            assert(geometric_side > 0.0);
        }

        assert(valid_samples > 0);
        assert(rejected_samples > 0);
    }

    void test_image_texture_srgb_decoding() {
        const char* filename = "srgb_test_texture.ppm";

        {
            std::ofstream file(filename);

            assert(file.good());

            file << "P3\n";
            file << "1 1\n";
            file << "255\n";
            file << "128 128 128\n";
        }

        {
            // Default behavior must preserve raw normalized values.
            ImageTexture linear_texture(filename);

            // Color textures can explicitly request sRGB decoding.
            ImageTexture srgb_texture(
                filename,
                true,
                true
            );

            const Vec3 linear_value =
                linear_texture.sample(0.5, 0.5, 0.0);

            const Vec3 decoded_value =
                srgb_texture.sample(0.5, 0.5, 0.0);

            const double expected_linear =
                128.0 / 255.0;

            const double expected_decoded =
                std::pow(
                    (expected_linear + 0.055) / 1.055,
                    2.4
                );

            assert(std::abs(
                linear_value.x - expected_linear
            ) < 1e-8);

            assert(std::abs(
                decoded_value.x - expected_decoded
            ) < 1e-8);

            assert(std::abs(
                decoded_value.y - expected_decoded
            ) < 1e-8);

            assert(std::abs(
                decoded_value.z - expected_decoded
            ) < 1e-8);

            // Mid-gray in sRGB should be substantially darker
            // after conversion to linear light.
            assert(decoded_value.x < linear_value.x);
            assert(decoded_value.x > 0.21);
            assert(decoded_value.x < 0.22);
        }

        std::remove(filename);
    }

    void test_image_texture_mipmap() {
        const char* filename = "mipmap_test_texture.ppm";

        {
            std::ofstream file(filename);

            assert(file.good());

            file << "P3\n";
            file << "4 4\n";
            file << "255\n";

            // High-frequency black-and-white checker pattern.
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    const int value =
                        ((x + y) % 2 == 0) ? 255 : 0;

                    file << value << ' '
                         << value << ' '
                         << value << '\n';
                }
            }
        }

        {
            ImageTexture texture(filename);

            // 4x4 -> 2x2 -> 1x1.
            assert(texture.width() == 4);
            assert(texture.height() == 4);
            assert(texture.mip_level_count() == 3);

            // At the top-left texel, level 0 is white.
            const Vec3 full_resolution =
                texture.sample(0.0, 1.0, 0.0);

            assert(full_resolution.x > 0.99);
            assert(full_resolution.y > 0.99);
            assert(full_resolution.z > 0.99);

            // The complete checker averages to gray at the
            // final mip level.
            const Vec3 coarse =
                texture.sample(0.0, 1.0, 2.0);

            assert(approximately_equal(coarse.x, 0.5));
            assert(approximately_equal(coarse.y, 0.5));
            assert(approximately_equal(coarse.z, 0.5));

            // Large screen-space UV derivatives should select
            // a coarser mip level automatically.
            const Vec3 automatic_lod = texture.value(
                0.0,
                1.0,
                Vec3(),
                1.0, 0.0,
                0.0, 1.0
            );

            assert(approximately_equal(
                automatic_lod.x, 0.5
            ));

            assert(approximately_equal(
                automatic_lod.y, 0.5
            ));

            assert(approximately_equal(
                automatic_lod.z, 0.5
            ));

                        // Ray-cone fallback should select a coarse mip level
            // when screen-space UV differentials are unavailable.
            const Vec3 cone_lod = texture.value_with_cone(
                0.0,
                1.0,
                Vec3(),
                0.0, 0.0,
                0.0, 0.0,
                false,
                1.0,
                true,
                Vec3(1.0, 0.0, 0.0),
                Vec3(0.0, 1.0, 0.0),
                true
            );

            assert(approximately_equal(cone_lod.x, 0.5));
            assert(approximately_equal(cone_lod.y, 0.5));
            assert(approximately_equal(cone_lod.z, 0.5));
        }

        std::remove(filename);
    }

    void test_texture_transform_and_address_modes() {
        const char* filename =
            "texture_transform_test.ppm";

        {
            std::ofstream file(filename);

            assert(file.good());

            file << "P3\n";
            file << "2 1\n";
            file << "255\n";
            file << "0 0 0 255 255 255\n";
        }

        try {
            ImageTexture clamp_texture(
                filename,
                false,
                false,
                TextureAddressMode::Clamp
            );

            ImageTexture repeat_texture(
                filename,
                false,
                false,
                TextureAddressMode::Repeat
            );

            const Vec3 clamped =
                clamp_texture.sample(0.0, 0.5, 0.0);

            const Vec3 repeated =
                repeat_texture.sample(0.0, 0.5, 0.0);

            // Clamp samples the first texel at u=0.
            assert(color_equal(
                clamped,
                Vec3(0.0, 0.0, 0.0)
            ));

            // Repeat interpolates across the texture seam.
            assert(color_equal(
                repeated,
                Vec3(0.5, 0.5, 0.5)
            ));

            auto underlying =
                std::make_shared<ImageTexture>(
                    filename,
                    false,
                    false,
                    TextureAddressMode::Clamp
                );

            TextureTransform transformed(
                underlying,
                0.5,
                1.0,
                0.25,
                0.0
            );

            // u=0.5 becomes 0.5*0.5+0.25=0.5,
            // which lies halfway between the two texels.
            const Vec3 transformed_color =
                transformed.value(
                    0.5,
                    0.5,
                    Vec3()
                );

            assert(color_equal(
                transformed_color,
                Vec3(0.5, 0.5, 0.5)
            ));
        } catch (...) {
            std::remove(filename);
            throw;
        }

        std::remove(filename);
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
    test_image_texture_srgb_decoding();
    test_image_texture_mipmap();
    test_texture_transform_and_address_modes();

    std::cout << "Texture tests passed.\n";
    return 0;
}
