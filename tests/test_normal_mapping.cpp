#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

#include "core/ray.h"
#include "core/vec3.h"
#include "geometry/hit_record.h"
#include "materials/lambertian.h"
#include "materials/normal_mapped_lambertian.h"
#include "materials/texture.h"
#include "geometry/sphere.h"
#include "core/interval.h"
#include "materials/bump_mapped_material.h"

static double vector_distance(
    const Vec3& a,
    const Vec3& b
) {
    return std::sqrt(
        (a.x - b.x) * (a.x - b.x) +
        (a.y - b.y) * (a.y - b.y) +
        (a.z - b.z) * (a.z - b.z)
    );
}

class LinearHeightTexture final : public Texture {
public:
    Vec3 value(
        double u,
        double,
        const Vec3&,
        const TextureSampleContext&
    ) const override {
        return Vec3(u, u, u);
    }
};

static void test_bump_mapping_world_space_gradient() {
    const auto base_material =
        std::make_shared<Lambertian>(
            Vec3(0.8, 0.7, 0.3)
        );

    const auto height_texture =
        std::make_shared<LinearHeightTexture>();

    const BumpMappedMaterial material(
        base_material,
        height_texture,
        1.0
    );

    const Ray incoming(
        Vec3(0.0, 0.0, 2.0),
        Vec3(0.0, 0.0, -1.0)
    );

    // Test a non-orthogonal UV parameterization.
    HitRecord hit;
    hit.point = Vec3(0.0, 0.0, 0.0);
    hit.normal = Vec3(0.0, 0.0, 1.0);
    hit.u = 0.5;
    hit.v = 0.5;

    hit.dpdu = Vec3(2.0, 0.0, 0.0);
    hit.dpdv = Vec3(1.0, 3.0, 0.0);
    hit.has_uv_parameterization = true;

    const Vec3 mapped_normal =
        material.mapped_normal(incoming, hit);

    // The correct world-space gradient is:
    // G = (0.5, -1/6, 0).
    const Vec3 expected_normal =
        normalize(Vec3(
            -0.5,
            1.0 / 6.0,
            1.0
        ));

    assert(
        vector_distance(
            mapped_normal,
            expected_normal
        ) < 1e-9
    );

    // Changing the world-space UV scale must change
    // the world-space gradient accordingly.
    hit.dpdu = Vec3(4.0, 0.0, 0.0);
    hit.dpdv = Vec3(0.0, 3.0, 0.0);

    const Vec3 scaled_normal =
        material.mapped_normal(incoming, hit);

    const Vec3 expected_scaled_normal =
        normalize(Vec3(
            -0.25,
            0.0,
            1.0
        ));

    assert(
        vector_distance(
            scaled_normal,
            expected_scaled_normal
        ) < 1e-9
    );

    assert(
        vector_distance(
            mapped_normal,
            scaled_normal
        ) > 0.05
    );
}

static void test_sphere_normal_mapping_tangent_frame() {
    constexpr double PI = 3.14159265358979323846;

    Sphere sphere(
        Vec3(0.0, 0.0, 0.0),
        1.0
    );

    const Vec3 outward_normal =
        normalize(Vec3(
            0.3,
            0.4,
            0.8660254037844386
        ));

    const Ray incoming(
        2.0 * outward_normal,
        -outward_normal
    );

    HitRecord hit;

    assert(sphere.hit(
        incoming,
        Interval(0.001, 10.0),
        hit
    ));

    assert(hit.has_uv_parameterization);

    // The tangent must follow increasing sphere U.
    const Vec3 expected_tangent =
        normalize(hit.dpdu);

    assert(
        dot(hit.tangent, expected_tangent) > 1.0 - 1e-9
    );

    // The bitangent must follow increasing sphere V.
    const Vec3 expected_bitangent =
        normalize(hit.dpdv);

    assert(
        dot(hit.bitangent, expected_bitangent) > 1.0 - 1e-9
    );

    // The tangent frame must be orthogonal.
    assert(
        std::abs(dot(hit.tangent, hit.normal)) < 1e-9
    );

    assert(
        std::abs(dot(hit.bitangent, hit.normal)) < 1e-9
    );

    assert(
        std::abs(dot(hit.tangent, hit.bitangent)) < 1e-9
    );

    // A tangent-space +X normal must map to the sphere's
    // actual increasing-U direction.
    const auto base_material =
        std::make_shared<Lambertian>(
            Vec3(0.8, 0.7, 0.3)
        );

    const auto tangent_normal_map =
        std::make_shared<SolidColor>(
            Vec3(1.0, 0.5, 0.5)
        );

    const NormalMappedMaterial normal_material(
        base_material,
        tangent_normal_map
    );

    const Vec3 mapped_normal =
        normal_material.mapped_normal(
            incoming,
            hit
        );

    assert(
        vector_distance(
            mapped_normal,
            expected_tangent
        ) < 1e-9
    );

    // Also check the frame when the sphere is hit from inside.
    const Ray inside_ray(
        Vec3(0.0, 0.0, 0.0),
        outward_normal
    );

    HitRecord inside_hit;

    assert(sphere.hit(
        inside_ray,
        Interval(0.001, 10.0),
        inside_hit
    ));

    assert(!inside_hit.front_face);

    assert(
        dot(
            inside_hit.bitangent,
            -1.0 * expected_bitangent
        ) > 1.0 - 1e-9
    );

    assert(
        std::abs(
            dot(
                inside_hit.tangent,
                inside_hit.bitangent
            )
        ) < 1e-9
    );

    (void)PI;
}

int main() {
    const auto normal_texture =
        std::make_shared<UVNormalTexture>(8.0);

    const auto base_material =
        std::make_shared<Lambertian>(
            Vec3(0.8, 0.7, 0.3)
        );

    const NormalMappedMaterial material(
        base_material,
        normal_texture
    );

    HitRecord hit;
    hit.point = Vec3(0.0, 0.0, 0.0);
    hit.normal = Vec3(0.0, 0.0, 1.0);
    hit.tangent = Vec3(1.0, 0.0, 0.0);
    hit.bitangent = Vec3(0.0, 1.0, 0.0);
    hit.front_face = true;

    const Ray incoming(
        Vec3(0.0, 0.0, 1.0),
        Vec3(0.0, 0.0, -1.0)
    );

    hit.u = 0.0;
    hit.v = 0.0;

    const Vec3 normal_a =
        material.shading_normal(incoming, hit);

    hit.u = 1.0 / 32.0;
    hit.v = 1.0 / 32.0;

    const Vec3 normal_b =
        material.shading_normal(incoming, hit);

    assert(
        std::abs(normal_a.length() - 1.0) < 1e-9
    );

    assert(
        std::abs(normal_b.length() - 1.0) < 1e-9
    );

    assert(
        vector_distance(normal_a, normal_b) > 0.1
    );

    assert(dot(normal_a, hit.normal) > 0.0);
    assert(dot(normal_b, hit.normal) > 0.0);

    const Vec3 default_normal =
        base_material->shading_normal(incoming, hit);

    assert(
        vector_distance(default_normal, hit.normal) < 1e-9
    );

    test_bump_mapping_world_space_gradient();
    test_sphere_normal_mapping_tangent_frame();

    std::cout << "Normal mapping test passed.\n";
    return 0;
}