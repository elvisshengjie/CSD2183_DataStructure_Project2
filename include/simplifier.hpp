#ifndef SIMPLIFIER_HPP
#define SIMPLIFIER_HPP

#include <cstddef>
#include <optional>
#include <vector>
#include "geometry.hpp"

namespace apsc {

    struct CollapseCandidate {
        int ring_id = -1;
        std::size_t start_index = 0;
        Point replacement_point;
        double estimated_areal_displacement = 0.0;
    };

    struct SimplificationResult {
        Polygon polygon;
        double input_area = 0.0;
        double output_area = 0.0;
        double areal_displacement = 0.0;
        std::size_t seeded_candidate_windows = 0;
        std::size_t successful_collapses = 0;
    };

    class Simplifier {
    public:
        SimplificationResult simplify(
            const Polygon& input,
            std::size_t    target_vertices) const;

        std::vector<CollapseCandidate> build_initial_candidates(
            const Polygon& polygon) const;

        std::optional<CollapseCandidate> compute_candidate(
            const Ring& ring,
            std::size_t start_index) const;

        bool candidate_is_topology_safe(
            const Polygon& polygon,
            const CollapseCandidate& candidate) const;
    };

}  // namespace apsc

#endif