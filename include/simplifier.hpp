#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "geometry.hpp"

namespace apsc {

// One possible APSC collapse for a 4-vertex window A -> B -> C -> D.
struct CollapseCandidate {
    int ring_id {};
    std::size_t start_index {};
    Point replacement_point {};
    double estimated_areal_displacement {};
};

// Bundles the final polygon together with the report values printed to stdout.
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
    // Entry point for the simplification pipeline.
    [[nodiscard]] SimplificationResult simplify(
        const Polygon& input,
        std::size_t target_vertices) const;

private:
    // Builds the initial list of 4-vertex windows that could become APSC collapses.
    [[nodiscard]] std::vector<CollapseCandidate> build_initial_candidates(
        const Polygon& polygon) const;

    // Computes the candidate geometry and cost for a single window if possible.
    [[nodiscard]] std::optional<CollapseCandidate> compute_candidate(
        const Ring& ring,
        std::size_t start_index) const;

    // Validates whether a candidate preserves topology before being applied.
    [[nodiscard]] bool candidate_is_topology_safe(
        const Polygon& polygon,
        const CollapseCandidate& candidate) const;
};

}  // namespace apsc
