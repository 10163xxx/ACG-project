
#pragma once

#include <limits>

struct Interval {
    double min;
    double max;

    Interval()
        : min(std::numeric_limits<double>::infinity()),
          max(-std::numeric_limits<double>::infinity()) {}

    Interval(double min_, double max_)
        : min(min_), max(max_) {}

    bool contains(double t) const {
        return min <= t && t <= max;
    }

    bool surrounds(double t) const {
        return min < t && t < max;
    }

    bool is_empty() const {
        return min > max;
    }
};
