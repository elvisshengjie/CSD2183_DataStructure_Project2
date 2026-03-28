#include "simplifier.hpp"

#include <iostream>
#include <limits>
#include <cmath>
#include <unordered_map>
namespace apsc {

SimplificationResult Simplifier::simplify(
    const Polygon& input,
    const std::size_t target_vertices) const {
    // Start from a copy of the input so the current baseline is always safe and the
    // future implementation can mutate the working polygon in place.
    SimplificationResult result;
    result.polygon = input;
    result.input_area = total_signed_area(input);
    result.output_area = result.input_area;
    result.areal_displacement = 0.0;

    const std::size_t current_vertices = input.total_vertices();
    if (target_vertices >= current_vertices) {
        // Nothing to do when the input already satisfies the target.
        return result;
    }

    // Seed the candidate windows now so the project already has a clear APSC entry point.
   // --- Build working rings as doubly-linked lists for O(1) local edits ---
    WorkingRings rings(input.rings.size());
    std::unordered_map<int, RingIter> node_iter;
    int global_node_id = 0;
    for (std::size_t r = 0; r < input.rings.size(); ++r) {
        for (const Point& pt : input.rings[r].vertices) {
            RingNode node;
            node.point = pt;
            node.ring_id = static_cast<int>(r);
            node.node_id = global_node_id++;
            node.generation = 0;
            rings[r].push_back(node);
            node_iter[node.node_id] = std::prev(rings[r].end());
        }
    }

    // Seed the priority queue from every 4-node window in every ring.
    CandidateQueue queue = build_initial_queue(rings);
    result.seeded_candidate_windows = queue.size();

    auto count_vertices = [&]() {
        std::size_t total = 0;
        for (const RingList& rl : rings) total += rl.size();
        return total;
        };

    double total_displacement = 0.0;

    // Greedy APSC collapse loop.
    while (count_vertices() > target_vertices && !queue.empty()) {
        const CollapseCandidate cand = queue.top();
        queue.pop();

        if (cand.ring_id < 0 || static_cast<std::size_t>(cand.ring_id) >= rings.size()) {
            continue;
        }
        RingList& ring = rings[static_cast<std::size_t>(cand.ring_id)];
        if (ring.size() < 4) continue;

        // O(1) staleness check via stable iterator map.
        auto map_it = node_iter.find(cand.node_id_a);
        if (map_it == node_iter.end()) continue;               // A was already erased
        RingIter iter_a = map_it->second;
        if (iter_a->generation != cand.generation_a) continue; // A's window changed

        if (!candidate_is_topology_safe(rings, cand.ring_id, iter_a, cand.replacement_point)) {
            continue;
        }

        // Apply A -> B -> C -> D  =>  A -> E -> D.
        RingIter iter_b = next_cyclic(ring, iter_a);
        RingIter iter_c = next_cyclic(ring, iter_b);

        const int id_b = iter_b->node_id;
        const int id_c = iter_c->node_id;

        RingNode node_e;
        node_e.point = cand.replacement_point;
        node_e.ring_id = cand.ring_id;
        node_e.node_id = global_node_id++;
        node_e.generation = 0;

        RingIter iter_e = ring.insert(iter_b, node_e);
        ring.erase(iter_b);
        ring.erase(iter_c);

        node_iter[node_e.node_id] = iter_e;
        node_iter.erase(id_b);
        node_iter.erase(id_c);

        iter_a->generation++;
        next_cyclic(ring, iter_e)->generation++;

        total_displacement += cand.estimated_areal_displacement;
        ++result.successful_collapses;

        push_neighbourhood_candidates(ring, iter_e, cand.ring_id, queue);
    }

    // Convert linked lists back to output Polygon.
    Polygon output;
    for (std::size_t r = 0; r < rings.size(); ++r) {
        output.rings.push_back(ring_list_to_ring(rings[r], static_cast<int>(r)));
    }
    result.polygon = output;
    result.output_area = total_signed_area(output);
    result.areal_displacement = total_displacement;
    return result;
}
}

std::vector<CollapseCandidate> Simplifier::build_initial_candidates(
    const Polygon& polygon) const {
    std::vector<CollapseCandidate> candidates;

    for (const Ring& ring : polygon.rings) {
        // APSC works on windows of four consecutive vertices A -> B -> C -> D.
        if (ring.vertices.size() < 4) {
            continue;
        }

        for (std::size_t start_index = 0; start_index < ring.vertices.size(); ++start_index) {
            if (const auto candidate = compute_candidate(ring, start_index)) {
                candidates.push_back(*candidate);
            }
        }
    }

    return candidates;
}

std::optional<CollapseCandidate> Simplifier::compute_candidate(
    const Ring& ring,
    const std::size_t start_index) const {
    if (ring.vertices.size() < 4) {
        return std::nullopt;
    }

    const std::size_t a = start_index % ring.vertices.size();
    const std::size_t d = (start_index + 3) % ring.vertices.size();

    // TODO: Compute the true APSC replacement point E using the derivation from the paper.
    // The midpoint is only a placeholder to keep the starter project easy to extend and
    // to make the candidate data flow visible before the real math is implemented.
    const Point& point_a = ring.vertices[a];
    const Point& point_d = ring.vertices[d];
    const Point midpoint {
        (point_a.x + point_d.x) * 0.5,
        (point_a.y + point_d.y) * 0.5,
    };

    CollapseCandidate candidate;
    candidate.ring_id = ring.ring_id;
    candidate.start_index = start_index;
    candidate.replacement_point = midpoint;
    // Infinity makes it obvious that this value is still a placeholder and must be
    // replaced by the real areal-displacement cost from the paper.
    candidate.estimated_areal_displacement = std::numeric_limits<double>::infinity();
    return candidate;
}

bool Simplifier::candidate_is_topology_safe(
    const Polygon& /*polygon*/,
    const CollapseCandidate& /*candidate*/) const {
    // TODO: Apply the candidate to a local copy or local linked structure and test:
    // - ring simplicity
    // - ring-ring intersections
    // - unchanged ring count/orientation assumptions
    // Returning false keeps the baseline implementation conservative until the real
    // topology-preserving checks are added.
    return false;
}

}  // namespace apsc
