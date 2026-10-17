#include <cassert>
#include <cmath>
#include <iostream>

#include "geometry/sphere.h"

int main() {
    constexpr double PI = 3.14159265358979323846;

    Sphere sphere(
        Vec3(1.0, 2.0, 3.0),
        2.0
    );

    const double samples[][2] = {
        {0.25, 0.0},
        {0.50, 0.25},
        {0.75, 0.50},
        {0.10, 0.90}
    };

    for (const auto& uv : samples) {
        HitRecord sample;
        double pdf_area = 0.0;

        const bool ok = sphere.sample_surface(
            uv[0],
            uv[1],
            sample,
            pdf_area
        );

        assert(ok);

        const Vec3 offset =
            sample.point - Vec3(1.0, 2.0, 3.0);

        // The sampled point must lie on the sphere.
        assert(std::abs(offset.length() - 2.0) < 1e-10);

        // The outward normal must be unit length.
        assert(std::abs(sample.normal.length() - 1.0) < 1e-10);

        // Uniform area density.
        const double expected_pdf =
            1.0 / (4.0 * PI * 2.0 * 2.0);

        assert(std::abs(pdf_area - expected_pdf) < 1e-12);
    }

    HitRecord invalid_sample;
    double invalid_pdf = 0.0;

    assert(!sphere.sample_surface(
        1.0, 0.5, invalid_sample, invalid_pdf
    ));

    std::cout << "All sphere sampling tests passed.\n";
    return 0;
}
