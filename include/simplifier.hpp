#pragma once

#include <cstddef>
#include <functional>
#include <list>
#include <optional>
#include <queue>
#include <vector>

#include "geometry.hpp"

namespace apsc {


    struct RingNode {
        Point point;
        int   ring_id{};
        int   node_id{};   // stable ID assigned at load time; used for staleness checks
        int   generation{}; // bumped each time this node is touched; invalidates old candidates
    };

    using RingList = std::list<RingNode>;
    using RingIter = RingList::iterator;
    using WorkingRings = std::vector<RingList>;

    // One possible APSC collapse for a 4-vertex window A -> B -> C -> D.
    struct CollapseCandidate {
        int      ring_id{};
        int      node_id_a{};              // stable id of node A; used for staleness check
        int      generation_a{};           // generation of A when candidate was computed
        Point    replacement_point{};      // E: the area-preserving replacement vertex
        double   estimated_areal_displacement{};
    };

    // Min-heap keyed by areal displacement.
    struct CandidateCmp {
        bool operator()(const CollapseCandidate& lhs, const CollapseCandidate& rhs) const {
            return lhs.estimated_areal_displacement > rhs.estimated_areal_displacement;
        }
    };
    using CandidateQueue = std::priority_queue<CollapseCandidate,
        std::vector<CollapseCandidate>,
        CandidateCmp>;

    // Bundles the final polygon together with the report values printed to stdout.
    struct SimplificationResult {
        Polygon     polygon;
        double      input_area{};
        double      output_area{};
        double      areal_displacement{};
        std::size_t seeded_candidate_windows{};
        std::size_t successful_collapses{};
    };

    // ---------------------------------------------------------------------------
    // Simplifier
    // ---------------------------------------------------------------------------
    class Simplifier {
    public:
        // Entry point for the simplification pipeline.
        [[nodiscard]] SimplificationResult simplify(
            const Polygon& input,
            std::size_t    target_vertices) const;

    private:
        // Cyclic iteration helpers.
        static RingIter next_cyclic(RingList& ring, RingIter it);
        static RingIter prev_cyclic(RingList& ring, RingIter it);

        // Convert a finished RingList back to a plain Ring for output.
        static Ring ring_list_to_ring(const RingList& ring_list, int ring_id);

        // Compute the area-preserving E and cost for one 4-node window starting at iter_a.
        // Returns nullopt for degenerate windows (ring too small, A==D, collinear, etc.).
        [[nodiscard]] static std::optional<CollapseCandidate> compute_candidate(
            RingList& ring,
            RingIter  iter_a,
            int       ring_id);

        // True iff the collapse keeps all rings simple and non-intersecting.
        [[nodiscard]] static bool candidate_is_topology_safe(
            const WorkingRings& rings,
            int                 ring_id,
            RingIter            iter_a,
            const Point& replacement_point);

        // Seed the priority queue from every 4-node window in every ring.
        [[nodiscard]] static CandidateQueue build_initial_queue(WorkingRings& rings);

        // After a collapse, push fresh candidates for the affected neighbourhood.
        static void push_neighbourhood_candidates(
            RingList& ring,
            RingIter        iter_e,
            int             ring_id,
            CandidateQueue& queue);
    };

}  // namespace apsc