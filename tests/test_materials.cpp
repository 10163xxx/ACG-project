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

    // 1b. Lambertian BSDF evaluation
    constexpr double PI = 3.14159265358979323846;

    const Vec3 outgoing_direction(0.0, 1.0, 0.0);
    const Vec3 incoming_direction(0.0, 1.0, 0.0);

    const Vec3 bsdf = diffuse.evaluate_bsdf(
        hit,
        outgoing_direction,
        incoming_direction
    );

    assert(std::abs(bsdf.x - 0.8 / PI) < 1e-9);
    assert(std::abs(bsdf.y - 0.3 / PI) < 1e-9);
    assert(std::abs(bsdf.z - 0.2 / PI) < 1e-9);

    // Directions below the surface must contribute zero.
    const Vec3 below_surface(0.0, -1.0, 0.0);

    const Vec3 invalid_bsdf = diffuse.evaluate_bsdf(
        hit,
        outgoing_direction,
        below_surface
    );

    assert(std::abs(invalid_bsdf.x) < 1e-12);
    assert(std::abs(invalid_bsdf.y) < 1e-12);
    assert(std::abs(invalid_bsdf.z) < 1e-12);

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
