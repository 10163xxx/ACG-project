#pragma once

#include <cmath>
#include <cstdint>
#include <random>
#include <limits>

#include "core/vec3.h"

namespace Random {

    inline std::mt19937_64& engine() {
        static thread_local std::mt19937_64 rng{
            std::random_device{}()
        };
        return rng;
    }

    inline void seed(std::uint64_t value) {
        engine().seed(value);
    }

    inline double random_double() {
        return std::generate_canonical<
            double,
            std::numeric_limits<double>::digits
        >(engine());
    }

    inline double random_double(double min, double max) {
        return min + (max - min) * random_double();
    }

    inline Vec3 random_vec(
        double min = 0.0,
        double max = 1.0
    ) {
        return {
            random_double(min, max),
            random_double(min, max),
            random_double(min, max)
        };
    }

    inline Vec3 random_in_unit_sphere() {
        while (true) {
            const Vec3 p = random_vec(-1.0, 1.0);
            if (p.length_squared() < 1.0) {
                return p;
            }
        }
    }

    inline Vec3 random_unit_vector() {
        while (true) {
            const Vec3 p = random_in_unit_sphere();
            const double len_sq = p.length_squared();

            if (len_sq > 1e-24) {
                return p / std::sqrt(len_sq);
            }
        }
    }

    inline Vec3 random_in_hemisphere(const Vec3& normal) {
        const Vec3 p = random_unit_vector();

        if (dot(p, normal) > 0.0) {
            return p;
        }

        return -p;
    }
}
