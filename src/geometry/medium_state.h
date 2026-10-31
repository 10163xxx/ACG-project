#pragma once

#include <algorithm>
#include <stdexcept>
#include <vector>
#include <cmath>

#include "geometry/hit_record.h"
#include "materials/material.h"

class MediumState {
public:
    struct Transition {
        double incident_ior = 1.0;
        double transmitted_ior = 1.0;
    };

    struct ActiveMedium {
        const Hittable* boundary = nullptr;
        double ior = 1.0;
        Vec3 absorption;
    };

    double current_ior() const {
        return media_.empty()
            ? 1.0
            : media_.back().ior;
    }

    Vec3 current_absorption() const {
        return media_.empty()
            ? Vec3(0.0, 0.0, 0.0)
            : media_.back().absorption;
    }

    bool empty() const {
        return media_.empty();
    }

    std::size_t size() const {
        return media_.size();
    }

    bool contains(const Hittable* boundary) const {
        return std::any_of(
            media_.begin(),
            media_.end(),
            [boundary](const ActiveMedium& medium) {
                return medium.boundary == boundary;
            }
        );
    }

    bool prepare_transition(
        const HitRecord& hit,
        Transition& transition
    ) const {
        if (!hit.material ||
            !hit.material->is_medium_boundary() ||
            hit.medium_boundary == nullptr) {
            return false;
        }

        if (hit.front_face) {
            if (contains(hit.medium_boundary)) {
                return false;
            }

            const double ior =
                hit.material->index_of_refraction();

            if (!std::isfinite(ior) || ior <= 0.0) {
                return false;
            }

            transition.incident_ior = current_ior();
            transition.transmitted_ior = ior;

            return true;
        }

        if (media_.empty() ||
            media_.back().boundary != hit.medium_boundary) {
            return false;
        }

        transition.incident_ior = media_.back().ior;

        transition.transmitted_ior =
            media_.size() > 1
                ? media_[media_.size() - 2].ior
                : 1.0;

        return true;
    }

    bool commit_transition(
        const HitRecord& hit,
        const Transition& transition
    ) {
        if (!hit.material ||
            !hit.material->is_medium_boundary() ||
            hit.medium_boundary == nullptr) {
            return false;
        }

        if (hit.front_face) {
            if (contains(hit.medium_boundary) ||
                !std::isfinite(transition.transmitted_ior) ||
                transition.transmitted_ior <= 0.0) {
                return false;
            }

            media_.push_back({
                hit.medium_boundary,
                transition.transmitted_ior,
                hit.material->shadow_absorption_coefficient()
            });

            return true;
        }

        if (media_.empty() ||
            media_.back().boundary != hit.medium_boundary) {
            return false;
        }

        media_.pop_back();
        return true;
    }

    void clear() {
        media_.clear();
    }

    bool initialize_from_intersections(
        std::vector<HitRecord> intersections
    ) {
        clear();

        std::sort(
            intersections.begin(),
            intersections.end(),
            [](const HitRecord& a, const HitRecord& b) {
                return a.t < b.t;
            }
        );

        for (const HitRecord& hit : intersections) {
            if (!hit.material ||
                !hit.material->is_medium_boundary()) {
                continue;
            }

            Transition transition;

            if (!prepare_transition(hit, transition)) {
                clear();
                return false;
            }

            if (!commit_transition(hit, transition)) {
                clear();
                return false;
            }
        }

        return true;
    }

private:
    std::vector<ActiveMedium> media_;
};