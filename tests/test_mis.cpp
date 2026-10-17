#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

#include "integrator/mis.h"

int main() {
    constexpr double EPSILON = 1e-12;

    // 1. Convert area PDF to solid-angle PDF.
    // pdf_omega = pdf_area * distance_squared / cos_light
    {
        const double pdf = mis::area_to_solid_angle_pdf(
            0.25,
            9.0,
            0.5
        );

        assert(std::abs(pdf - 4.5) < EPSILON);
    }

    // 2. Equal PDFs must receive equal weights.
    {
        const double weight = mis::power_heuristic(
            1.0,
            1.0
        );

        assert(std::abs(weight - 0.5) < EPSILON);
    }

    // 3. Verify the Power Heuristic ratio.
    // w(2, 1) = 2^2 / (2^2 + 1^2) = 0.8
    {
        const double weight = mis::power_heuristic(
            2.0,
            1.0
        );

        assert(std::abs(weight - 0.8) < EPSILON);
    }

    // 4. Boundary cases for zero PDFs.
    {
        assert(std::abs(
            mis::power_heuristic(0.0, 1.0)
        ) < EPSILON);

        assert(std::abs(
            mis::power_heuristic(1.0, 0.0) - 1.0
        ) < EPSILON);

        assert(std::abs(
            mis::power_heuristic(0.0, 0.0)
        ) < EPSILON);
    }

    // 5. Invalid area-PDF inputs must return zero.
    {
        assert(mis::area_to_solid_angle_pdf(
            0.0, 1.0, 1.0
        ) == 0.0);

        assert(mis::area_to_solid_angle_pdf(
            1.0, 0.0, 1.0
        ) == 0.0);

        assert(mis::area_to_solid_angle_pdf(
            1.0, 1.0, 0.0
        ) == 0.0);

        assert(mis::area_to_solid_angle_pdf(
            -1.0, 1.0, 1.0
        ) == 0.0);

        assert(mis::area_to_solid_angle_pdf(
            std::numeric_limits<double>::infinity(),
            1.0,
            1.0
        ) == 0.0);
    }

    // 6. Extreme PDF values must not overflow.
    {
        const double weight = mis::power_heuristic(
            1e300,
            1e300
        );

        assert(std::isfinite(weight));
        assert(std::abs(weight - 0.5) < EPSILON);
    }

    // 7. Invalid PDFs must not produce NaN or infinity.
    {
        const double weight = mis::power_heuristic(
            -1.0,
            std::numeric_limits<double>::infinity()
        );

        assert(std::isfinite(weight));
        assert(weight == 0.0);
    }

    std::cout << "All MIS tests passed.\n";
    return 0;
}
