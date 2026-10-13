#include <cassert>
#include <cmath>
#include <iostream>

#include "core/random.h"
#include "materials/lambertian.h"
#include "materials/metal.h"
#include "materials/dielectric.h"

int main() {
    Random::seed(12345);

    const Ray incoming(
        Vec3(0.0, 1.0, 0.0),
        Vec3(0.0, -1.0, 0.0)
    );

    HitRecord hit;
    hit.point = Vec3(0.0, 0.0, 0.0);
    hit.normal = Vec3(0.0, 1.0, 0.0);
    hit.front_face = true;

    Vec3 attenuation;
    Ray scattered;

    // 1. Lambertian
    Lambertian diffuse(Vec3(0.8, 0.3, 0.2));

    const bool diffuse_ok =
        diffuse.scatter(incoming, hit, attenuation, scattered);

    assert(diffuse_ok);
    assert(std::abs(attenuation.x - 0.8) < 1e-9);
    assert(std::abs(attenuation.y - 0.3) < 1e-9);
    assert(std::abs(attenuation.z - 0.2) < 1e-9);
    assert(dot(scattered.direction, hit.normal) > 0.0);

    // 2. Perfect metal
    Metal mirror(Vec3(0.9, 0.9, 0.9), 0.0);

    const bool metal_ok =
        mirror.scatter(incoming, hit, attenuation, scattered);

    assert(metal_ok);
    assert(dot(scattered.direction, hit.normal) > 0.0);
    assert(std::abs(scattered.direction.x) < 1e-9);
    assert(std::abs(scattered.direction.y - 1.0) < 1e-9);
    assert(std::abs(scattered.direction.z) < 1e-9);

    // 3. Dielectric
    Dielectric glass(1.5);

    const bool glass_ok =
        glass.scatter(incoming, hit, attenuation, scattered);

    assert(glass_ok);
    assert(std::abs(attenuation.x - 1.0) < 1e-9);
    assert(std::abs(attenuation.y - 1.0) < 1e-9);
    assert(std::abs(attenuation.z - 1.0) < 1e-9);
    assert(scattered.direction.length_squared() > 0.0);

    std::cout << "All material tests passed.\n";
    return 0;
}
