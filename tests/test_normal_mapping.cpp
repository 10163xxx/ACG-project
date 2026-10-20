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

    std::cout << "Normal mapping test passed.\n";
    return 0;
}