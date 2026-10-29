#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

#include "core/random.h"
#include "materials/lambertian.h"
#include "materials/metal.h"
#include "materials/dielectric.h"
#include "materials/principled.h"
#include "integrator/mis.h"

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
    ScatterSampleInfo scatter_info;

    // 1. Lambertian
    Lambertian diffuse(Vec3(0.8, 0.3, 0.2));

    const bool diffuse_ok =
        diffuse.scatter(incoming, hit, attenuation, scattered, scatter_info);

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

    // 1c. Principled BSDF multiple-scattering compensation.
    const Vec3 metal_color(0.9, 0.8, 0.7);

    PrincipledBSDF rough_metal(
        metal_color,
        1.0,
        1.0
    );

    const Vec3 principled_bsdf = rough_metal.evaluate_bsdf(
        hit,
        outgoing_direction,
        incoming_direction
    );

    // At normal incidence with roughness = 1:
    // GGX D = 1 / PI, Smith G = 1, and cos_o = cos_i = 1.
    // For a fully metallic material, the single-scattering
    // BSDF is F0 / (4 * PI); the diffuse term is zero.
    const Vec3 single_scattering =
        metal_color / (4.0 * PI);

    // Multiple-scattering compensation must add energy
    // beyond the single-scattering GGX result.
    assert(principled_bsdf.x > single_scattering.x);
    assert(principled_bsdf.y > single_scattering.y);
    assert(principled_bsdf.z > single_scattering.z);

    assert(std::isfinite(principled_bsdf.x));
    assert(std::isfinite(principled_bsdf.y));
    assert(std::isfinite(principled_bsdf.z));

    assert(principled_bsdf.x >= 0.0);
    assert(principled_bsdf.y >= 0.0);
    assert(principled_bsdf.z >= 0.0);

    // 2. Perfect metal
    Metal mirror(Vec3(0.9, 0.9, 0.9), 0.0);

    const bool metal_ok =
        mirror.scatter(incoming, hit, attenuation, scattered, scatter_info);

    assert(metal_ok);
    assert(dot(scattered.direction, hit.normal) > 0.0);
    assert(std::abs(scattered.direction.x) < 1e-9);
    assert(std::abs(scattered.direction.y - 1.0) < 1e-9);
    assert(std::abs(scattered.direction.z) < 1e-9);

    // 3. Dielectric
    Dielectric glass(1.5);

    assert(!glass.blocks_shadow(hit));
    assert(std::abs(glass.shadow_transmittance(hit) - 1.0) < 1e-9);

    const bool glass_ok =
        glass.scatter(incoming, hit, attenuation, scattered, scatter_info);

    assert(glass_ok);
    assert(std::abs(attenuation.x - 1.0) < 1e-9);
    assert(std::abs(attenuation.y - 1.0) < 1e-9);
    assert(std::abs(attenuation.z - 1.0) < 1e-9);
    assert(scattered.direction.length_squared() > 0.0);

    // MIS: area PDF to solid-angle PDF conversion.
    assert(std::abs(
        mis::area_to_solid_angle_pdf(0.5, 4.0, 0.5)
        - 4.0
    ) < 1e-12);

    assert(std::abs(
        mis::area_to_solid_angle_pdf(0.25, 9.0, 0.75)
        - 3.0
    ) < 1e-12);

    assert(mis::area_to_solid_angle_pdf(
        0.0, 4.0, 0.5
    ) == 0.0);

    assert(mis::area_to_solid_angle_pdf(
        0.5, 0.0, 0.5
    ) == 0.0);

    assert(mis::area_to_solid_angle_pdf(
        0.5, 4.0, 0.0
    ) == 0.0);

    // MIS: Power Heuristic.
    assert(std::abs(
        mis::power_heuristic(1.0, 1.0) - 0.5
    ) < 1e-12);

    assert(std::abs(
        mis::power_heuristic(2.0, 1.0) - 0.8
    ) < 1e-12);

    assert(std::abs(
        mis::power_heuristic(1.0, 2.0) - 0.2
    ) < 1e-12);

    assert(std::abs(
        mis::power_heuristic(0.0, 1.0)
    ) < 1e-12);

    assert(std::abs(
        mis::power_heuristic(1.0, 0.0) - 1.0
    ) < 1e-12);

    assert(mis::power_heuristic(0.0, 0.0) == 0.0);

    // Invalid PDFs must not produce NaN or infinity.
    assert(std::isfinite(
        mis::power_heuristic(
            std::numeric_limits<double>::infinity(),
            1.0
        )
    ));

    assert(std::isfinite(
        mis::area_to_solid_angle_pdf(
            std::numeric_limits<double>::infinity(),
            1.0,
            1.0
        )
    ));

    std::cout << "All material tests passed.\n";
    return 0;
}
