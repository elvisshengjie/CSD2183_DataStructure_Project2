#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "geometry.hpp"

namespace apsc {

struct CollapseCandidate {
    int ring_id {};
    std::size_t start_index {};
    Point replacement_point {};
    double estimated_areal_displacement {};
};

struct SimplificationResult {
    Polygon polygon;
    double input_area {};
    double output_area {};
    double areal_displacement {};
    std::size_t seeded_candidate_windows {};
    std::size_t successful_collapses {};
};

class Simplifier {
public:
    [[nodiscard]] SimplificationResult simplify(
        const Polygon& input,
        std::size_t target_vertices) const;

private:
    [[nodiscard]] std::vector<CollapseCandidate> build_initial_candidates(
        const Polygon& polygon) const;

    [[nodiscard]] std::optional<CollapseCandidate> compute_candidate(
        const Ring& ring,
        std::size_t start_index) const;

    [[nodiscard]] bool candidate_is_topology_safe(
        const Polygon& polygon,
        const CollapseCandidate& candidate) const;
};

}  // namespace apsc
