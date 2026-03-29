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


// Cyclic list helpers
RingIter Simplifier::next_cyclic(RingList& ring, RingIter it) {
    auto nxt = std::next(it);
    return (nxt == ring.end()) ? ring.begin() : nxt;
}

RingIter Simplifier::prev_cyclic(RingList& ring, RingIter it) {
    return (it == ring.begin()) ? std::prev(ring.end()) : std::prev(it);
}

// Convert a finished RingList back to a plain Ring for output
Ring Simplifier::ring_list_to_ring(const RingList& ring_list, const int ring_id) {
    Ring ring;
    ring.ring_id = ring_id;
    for (const RingNode& node : ring_list) {
        ring.vertices.push_back(node.point);
    }
    return ring;
}

// APSC point E derivation (Kronenfeld et al. 2020, Section 3)
static double tri_area(const Point& p1, const Point& p2, const Point& p3) {
    return 0.5 * ((p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x));
}

static bool segments_cross(const Point& a, const Point& b,
    const Point& c, const Point& d) {
    auto cross2 = [](const Point& o, const Point& p, const Point& q) {
        return (p.x - o.x) * (q.y - o.y) - (p.y - o.y) * (q.x - o.x);
        };
    constexpr double kEps = 1e-9;
    const double d1 = cross2(a, b, c), d2 = cross2(a, b, d);
    const double d3 = cross2(c, d, a), d4 = cross2(c, d, b);
    return ((d1 > kEps && d2 < -kEps) || (d1 < -kEps && d2 > kEps)) &&
        ((d3 > kEps && d4 < -kEps) || (d3 < -kEps && d4 > kEps));
}

std::optional<CollapseCandidate> Simplifier::compute_candidate(
    RingList& ring,
    RingIter  iter_a,
    const int ring_id) {

    if (ring.size() < 4) return std::nullopt;

    RingIter iter_b = next_cyclic(ring, iter_a);
    RingIter iter_c = next_cyclic(ring, iter_b);
    RingIter iter_d = next_cyclic(ring, iter_c);
    if (iter_d == iter_a) return std::nullopt;

    const Point& A = iter_a->point;
    const Point& B = iter_b->point;
    const Point& C = iter_c->point;
    const Point& D = iter_d->point;

    const double S_quad = tri_area(A, B, C) + tri_area(A, C, D);
    const double S_tri = tri_area(A, B, D);
    const double denom = S_quad - S_tri;

    Point E;
    constexpr double kEps = 1e-12;
    if (std::abs(denom) < kEps) {
        E = { (A.x + D.x) * 0.5, (A.y + D.y) * 0.5 };
    }
    else {
        const double t = S_quad / denom;
        // t outside [0,1] means E lands outside the AD segment — degenerate, skip.
        if (t < 0.0 || t > 1.0) return std::nullopt;
        E = { A.x + t * (D.x - A.x), A.y + t * (D.y - A.y) };
    }

    // Displacement is the residual area not cancelled by the area-preserving placement.
    // Use absolute value so sign convention of the ring doesn't matter.
    const double displaced = std::abs(tri_area(A, B, C) + tri_area(A, C, D)
        - tri_area(A, E, D));

    CollapseCandidate cand;
    cand.ring_id = ring_id;
    cand.node_id_a = iter_a->node_id;
    cand.generation_a = iter_a->generation;
    cand.replacement_point = E;
    cand.estimated_areal_displacement = displaced;
    std::cerr << "A=(" << A.x << "," << A.y << ") D=(" << D.x << "," << D.y
        << ") E=(" << E.x << "," << E.y << ") t=" << (S_quad / denom)
        << " disp=" << displaced << "\n";
    return cand;
}

// Build initial priority queue
CandidateQueue Simplifier::build_initial_queue(WorkingRings& rings) {
    CandidateQueue queue;
    for (std::size_t r = 0; r < rings.size(); ++r) {
        RingList& ring = rings[r];
        if (ring.size() < 4) continue;
        for (RingIter it = ring.begin(); it != ring.end(); ++it) {
            if (auto cand = compute_candidate(ring, it, static_cast<int>(r))) {
                queue.push(*cand);
            }
        }
    }
    return queue;
}

void Simplifier::push_neighbourhood_candidates(
    RingList& ring,
    RingIter        iter_e,
    const int       ring_id,
    CandidateQueue& queue) {

    if (ring.size() < 4) return;

    RingIter start = iter_e;
    for (int step = 0; step < 3; ++step) {
        start = prev_cyclic(ring, start);
    }

    RingIter cur = start;
    for (int w = 0; w < 4; ++w) {
        if (auto cand = compute_candidate(ring, cur, ring_id)) {
            queue.push(*cand);
        }
        cur = next_cyclic(ring, cur);
        if (cur == start) break;
    }
}



bool Simplifier::candidate_is_topology_safe(
    const WorkingRings& rings,
    const int           ring_id,
    RingIter            iter_a,
    const Point& replacement_point) {

    RingList& ring = const_cast<RingList&>(rings[static_cast<std::size_t>(ring_id)]);
    RingIter iter_b = next_cyclic(ring, iter_a);
    RingIter iter_c = next_cyclic(ring, iter_b);

    Ring test_ring;
    test_ring.ring_id = ring_id;
    bool inserted_e = false;
    for (auto it = ring.begin(); it != ring.end(); ++it) {
        if (it == iter_b || it == iter_c) {
            if (!inserted_e) {
                test_ring.vertices.push_back(replacement_point);
                inserted_e = true;
            }
        }
        else {
            test_ring.vertices.push_back(it->point);
        }
    }

    if (test_ring.vertices.size() < 3) return false;
    if (!ring_is_simple(test_ring))    return false;

    for (std::size_t r = 0; r < rings.size(); ++r) {
        if (static_cast<int>(r) == ring_id) continue;
        Ring other = ring_list_to_ring(rings[r], static_cast<int>(r));
        const std::size_t n = test_ring.vertices.size();
        const std::size_t m = other.vertices.size();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < m; ++j) {
                if (segments_cross(
                    test_ring.vertices[i], test_ring.vertices[(i + 1) % n],
                    other.vertices[j], other.vertices[(j + 1) % m])) {
                    return false;
                }
            }
        }
    }
    return true;
}

}  // namespace apsc