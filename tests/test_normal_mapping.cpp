#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

#include "core/ray.h"
#include "core/vec3.h"
#include "geometry/hit_record.h"
#include "materials/normal_mapped_lambertian.h"
#include "materials/texture.h"
#include "materials/lambertian.h"

static double vector_distance(const Vec3& a, const Vec3& b) {
    return std::sqrt(
        (a.x - b.x) * (a.x - b.x) +
        (a.y - b.y) * (a.y - b.y) +
        (a.z - b.z) * (a.z - b.z)
    );
}

int main() {
    const auto normal_texture =
        std::make_shared<UVNormalTexture>(8.0);

    const NormalMappedLambertian material(
        Vec3(0.8, 0.7, 0.3),
        normal_texture
    );

    HitRecord hit;
    hit.point = Vec3(0.0, 0.0, 0.0);
    hit.normal = Vec3(0.0, 0.0, 1.0);
    hit.front_face = true;

    const Ray incoming(
        Vec3(0.0, 0.0, 1.0),
        Vec3(0.0, 0.0, -1.0)
    );

    hit.u = 0.0;
    hit.v = 0.0;
    const Vec3 normal_a = material.mapped_normal(incoming, hit);

    hit.u = 1.0 / 32.0;
    hit.v = 1.0 / 32.0;
    const Vec3 normal_b = material.mapped_normal(incoming, hit);

    const Vec3 shading_normal =
        material.shading_normal(incoming, hit);

    const Vec3 mapped_normal =
        material.mapped_normal(incoming, hit);

    assert(vector_distance(shading_normal, mapped_normal) < 1e-9);
    assert(std::abs(shading_normal.length() - 1.0) < 1e-9);

    assert(std::abs(normal_a.length() - 1.0) < 1e-9);
    assert(std::abs(normal_b.length() - 1.0) < 1e-9);

    assert(vector_distance(normal_a, normal_b) > 0.1);

    const Lambertian diffuse_material(
        Vec3(0.8, 0.7, 0.3)
    );

    const Vec3 default_normal =
        diffuse_material.shading_normal(incoming, hit);

    assert(vector_distance(default_normal, hit.normal) < 1e-9);

    std::cout << "Normal mapping test passed.\n";
    return 0;
}
