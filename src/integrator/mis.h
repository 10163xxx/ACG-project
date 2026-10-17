#pragma once

#include <algorithm>
#include <cmath>

namespace mis {

// Convert a surface-area PDF (1 / area) into a
// solid-angle PDF (1 / steradian).
//
// area_pdf: PDF per unit area at the sampled light point.
// distance_squared: Squared distance from the shading point
//                   to the sampled light point.
// cos_light: Absolute cosine between the light normal and
//            the direction from the light toward the shading point.
inline double area_to_solid_angle_pdf(
    double area_pdf,
    double distance_squared,
    double cos_light
) {
    if (!std::isfinite(area_pdf)
        || !std::isfinite(distance_squared)
        || !std::isfinite(cos_light)) {
        return 0.0;
    }

    if (area_pdf <= 0.0
        || distance_squared <= 0.0
        || cos_light <= 1e-12) {
        return 0.0;
    }

    const double pdf =
        area_pdf * distance_squared / cos_light;

    if (!std::isfinite(pdf) || pdf <= 0.0) {
        return 0.0;
    }

    return pdf;
}

// Balance the contribution of two sampling strategies
// using the Power Heuristic.
//
// pdf_a and pdf_b must be PDFs expressed in the same measure,
// such as both PDFs per unit solid angle.
inline double power_heuristic(
    double pdf_a,
    double pdf_b
) {
    if (!std::isfinite(pdf_a) || pdf_a < 0.0) {
        pdf_a = 0.0;
    }

    if (!std::isfinite(pdf_b) || pdf_b < 0.0) {
        pdf_b = 0.0;
    }

    const double max_pdf =
        std::max(pdf_a, pdf_b);

    if (max_pdf == 0.0) {
        return 0.0;
    }

    // Scale first to avoid overflow when squaring large PDFs.
    const double a = pdf_a / max_pdf;
    const double b = pdf_b / max_pdf;

    const double a_squared = a * a;
    const double b_squared = b * b;

    return a_squared / (a_squared + b_squared);
}

} // namespace mis
