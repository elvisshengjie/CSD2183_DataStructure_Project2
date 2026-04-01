#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "geometry.hpp"

namespace apsc {

    // Stores the data needed to evaluate and apply one segment-collapse operation.
    // A collapse takes the four-vertex window A-B-C-D, removes B and C, and
    // inserts the Steiner point E (replacement_point) between A and D.
    struct CollapseCandidate {
        int ring_id{};
        std::size_t start_index{};      // index of A in the ring vertex array
        Point replacement_point{};      // computed Steiner point E
        double estimated_areal_displacement{};
    };

    // Holds everything the caller needs after simplification completes.
    struct SimplificationResult {
        Polygon polygon;
        double input_area{};
        double output_area{};
        double areal_displacement{};    // sum of per-step displacement estimates

        // Diagnostic counters (not part of the output format).
        std::size_t seeded_candidate_windows{};
        std::size_t successful_collapses{};
    };

    // Stateless simplifier – thread-safe to call from multiple threads.
    class Simplifier {
    public:
        // Simplify `input` until it has at most `target_vertices` total vertices
        // across all rings, using the APSC algorithm with topology check.
        SimplificationResult simplify(
            const Polygon& input,
            std::size_t target_vertices) const;

        // Build the initial list of candidate collapse operations for `polygon`.
        std::vector<CollapseCandidate> build_initial_candidates(
            const Polygon& polygon) const;

        // Compute the collapse candidate whose window starts at `start_index`
        // in `ring`. Returns nullopt when the window is degenerate.
        std::optional<CollapseCandidate> compute_candidate(
            const Ring& ring,
            std::size_t start_index) const;

        // Returns true if applying `candidate` would not introduce any new
        // self-intersections in `polygon`.
        bool candidate_is_topology_safe(
            const Polygon& polygon,
            const CollapseCandidate& candidate) const;
    };

}  // namespace apsc