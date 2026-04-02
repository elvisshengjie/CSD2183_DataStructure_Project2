#include "../include/simplifier.hpp"
#include <cmath>
#include <limits>
#include <algorithm>
#include <cmath>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

namespace apsc {
    namespace {

        constexpr double kEpsilon = 1e-9;
        constexpr double kZeroCostEpsilon = 1e-6;

        double cross(const Point& a, const Point& b, const Point& c) {
            return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        }

        bool nearly_equal(double lhs, double rhs) {
            return std::abs(lhs - rhs) <= kEpsilon;
        }

        bool same_point(const Point& p, const Point& q) {
            return nearly_equal(p.x, q.x) && nearly_equal(p.y, q.y);
        }

        bool point_on_segment(const Point& p, const Point& a, const Point& b) {
            return std::min(a.x, b.x) - kEpsilon <= p.x &&
                p.x <= std::max(a.x, b.x) + kEpsilon &&
                std::min(a.y, b.y) - kEpsilon <= p.y &&
                p.y <= std::max(a.y, b.y) + kEpsilon &&
                nearly_equal(cross(a, b, p), 0.0);
        }

        bool segments_intersect(
            const Point& a1, const Point& a2,
            const Point& b1, const Point& b2) {

            const double d1 = cross(b1, b2, a1);
            const double d2 = cross(b1, b2, a2);
            const double d3 = cross(a1, a2, b1);
            const double d4 = cross(a1, a2, b2);

            const bool proper =
                (((d1 > kEpsilon && d2 < -kEpsilon) || (d1 < -kEpsilon && d2 >  kEpsilon)) &&
                    ((d3 > kEpsilon && d4 < -kEpsilon) || (d3 < -kEpsilon && d4 >  kEpsilon)));
            if (proper) return true;

            return (std::abs(d1) < kEpsilon && point_on_segment(a1, b1, b2)) ||
                (std::abs(d2) < kEpsilon && point_on_segment(a2, b1, b2)) ||
                (std::abs(d3) < kEpsilon && point_on_segment(b1, a1, a2)) ||
                (std::abs(d4) < kEpsilon && point_on_segment(b2, a1, a2));
        }

        bool bboxes_overlap(
            const Point& a1, const Point& a2,
            const Point& b1, const Point& b2) {

            return std::min(a1.x, a2.x) - kEpsilon <= std::max(b1.x, b2.x) + kEpsilon &&
                std::min(b1.x, b2.x) - kEpsilon <= std::max(a1.x, a2.x) + kEpsilon &&
                std::min(a1.y, a2.y) - kEpsilon <= std::max(b1.y, b2.y) + kEpsilon &&
                std::min(b1.y, b2.y) - kEpsilon <= std::max(a1.y, a2.y) + kEpsilon;
        }

        double side_of_line(const Point& point, const Point& a, const Point& b) {
            return cross(a, b, point);
        }

        double dist_to_line(const Point& point, const Point& a, const Point& b) {
            const double denom = std::hypot(b.x - a.x, b.y - a.y);
            if (denom <= kEpsilon) return 0.0;
            return std::abs(cross(a, b, point)) / denom;
        }

        double signed_area_of(const std::vector<Point>& v) {
            if (v.size() < 3) return 0.0;
            double area = 0.0;
            for (std::size_t i = 0; i < v.size(); ++i) {
                const Point& cur = v[i];
                const Point& next = v[(i + 1) % v.size()];
                area += cur.x * next.y - next.x * cur.y;
            }
            return 0.5 * area;
        }

        struct ELine { double a, b, c; };

        ELine compute_e_line(
            const Point& A, const Point& B, const Point& C, const Point& D) {

            return {
                D.y - A.y,
                A.x - D.x,
                -B.y * A.x + (A.y - C.y) * B.x + (B.y - D.y) * C.x + C.y * D.x
            };
        }

        std::optional<Point> line_intersect(
            const Point& p1, const Point& p2,
            const Point& q1, const Point& q2) {

            const double a1 = p2.y - p1.y, b1 = p1.x - p2.x;
            const double a2 = q2.y - q1.y, b2 = q1.x - q2.x;
            const double det = a1 * b2 - a2 * b1;
            if (std::abs(det) <= kEpsilon) return std::nullopt;

            const double c1 = a1 * p1.x + b1 * p1.y;
            const double c2 = a2 * q1.x + b2 * q1.y;
            return Point{ (b2 * c1 - b1 * c2) / det,
                          (a1 * c2 - a2 * c1) / det };
        }

        std::optional<Point> eline_intersect_segment(
            const ELine& el, const Point& p1, const Point& p2) {

            Point ep1, ep2;
            if (std::abs(el.b) > kEpsilon) {
                ep1 = { 0.0,  -el.c / el.b };
                ep2 = { 1.0, -(el.a + el.c) / el.b };
            }
            else if (std::abs(el.a) > kEpsilon) {
                ep1 = { -el.c / el.a, 0.0 };
                ep2 = { -el.c / el.a, 1.0 };
            }
            else {
                return std::nullopt;
            }
            return line_intersect(ep1, ep2, p1, p2);
        }

        std::optional<Point> compute_apsc_point(
            const Point& A, const Point& B, const Point& C, const Point& D) {

            const ELine el = compute_e_line(A, B, C, D);
            const auto isect_ab = eline_intersect_segment(el, A, B);
            const auto isect_cd = eline_intersect_segment(el, C, D);

            auto pick = [&](bool prefer_ab) -> std::optional<Point> {
                if (prefer_ab) return isect_ab ? isect_ab : isect_cd;
                else           return isect_cd ? isect_cd : isect_ab;
                };

            const double sb = side_of_line(B, A, D);
            const double sc = side_of_line(C, A, D);
            const bool same_side = (sb * sc) > 0.0;

            if (same_side) {
                const double db = dist_to_line(B, A, D);
                const double dc = dist_to_line(C, A, D);
                if (db > dc + kEpsilon)
                    return pick(false);
                else
                    return pick(true);
            }

            Point pt_on_e;
            if (std::abs(el.b) > kEpsilon)
                pt_on_e = { 0.0, -el.c / el.b };
            else
                pt_on_e = { -el.c / el.a, 0.0 };

            const double se = side_of_line(pt_on_e, A, D);
            const bool prefer_ab = (sb > 0.0) == (se > 0.0);
            return pick(prefer_ab);
        }

        double compute_areal_displacement(
            const Point& A, const Point& B, const Point& C, const Point& D,
            const Point& E) {

            const bool on_ab = nearly_equal(cross(A, B, E), 0.0);
            const bool on_cd = nearly_equal(cross(C, D, E), 0.0);

            if (on_ab && !on_cd) {
                if (const auto split = line_intersect(E, D, B, C)) {
                    return std::abs(signed_area_of({ B, E, *split })) +
                        std::abs(signed_area_of({ *split, C, D }));
                }
            }

            if (on_cd && !on_ab) {
                if (const auto split = line_intersect(A, E, B, C)) {
                    return std::abs(signed_area_of({ A, B, *split })) +
                        std::abs(signed_area_of({ *split, E, C }));
                }
            }

            return std::abs(signed_area_of({ A, B, E })) +
                std::abs(signed_area_of({ E, C, D }));
        }

        bool topology_safe(
            const Polygon& polygon,
            int            ring_id,
            std::size_t    a_idx,
            std::size_t    b_idx,
            std::size_t    c_idx,
            std::size_t    d_idx,
            const Point& E) {

            const Ring& ring_ref = polygon.rings[static_cast<std::size_t>(ring_id)];
            const Point& A = ring_ref.vertices[a_idx];
            const Point& D = ring_ref.vertices[d_idx];

            for (std::size_t ri = 0; ri < polygon.rings.size(); ++ri) {
                const Ring& tr = polygon.rings[ri];
                const std::size_t n = tr.vertices.size();

                for (std::size_t i = 0; i < n; ++i) {
                    const Point& p1 = tr.vertices[i];
                    const Point& p2 = tr.vertices[(i + 1) % n];

                    if (static_cast<int>(ri) == ring_id) {
                        if ((i == a_idx && (i + 1) % n == b_idx) ||
                            (i == b_idx && (i + 1) % n == c_idx) ||
                            (i == c_idx && (i + 1) % n == d_idx) ||
                            (i == d_idx && (i + 1) % n == a_idx)) {
                            continue;
                        }
                        if (same_point(p1, A) || same_point(p2, A) ||
                            same_point(p1, D) || same_point(p2, D)) {
                            continue;
                        }
                    }

                    if (bboxes_overlap(A, E, p1, p2) && segments_intersect(A, E, p1, p2))
                        return false;
                    if (bboxes_overlap(E, D, p1, p2) && segments_intersect(E, D, p1, p2))
                        return false;
                }
            }
            return true;
        }

        struct QueueEntry {
            double      cost{};
            double      replacement_y{};
            std::size_t start_index{};
            std::size_t sequence{};
            int         ring_id{};
            int         id_a{}, id_b{}, id_c{}, id_d{};

            bool operator>(const QueueEntry& o) const {
            if (cost != o.cost) return cost > o.cost;
            return sequence > o.sequence;
}
        };

        double normalise_cost(double cost) {
            if (std::abs(cost) < kZeroCostEpsilon) return 0.0;
            return std::round(cost * 1e12) / 1e12;
        }

    }  // namespace

    SimplificationResult Simplifier::simplify(
        const Polygon& input,
        std::size_t    target_vertices) const {

        SimplificationResult result;
        result.polygon = input;
        result.input_area = total_signed_area(input);
        result.output_area = result.input_area;
        result.areal_displacement = 0.0;
        result.seeded_candidate_windows = 0;
        result.successful_collapses = 0;

        std::size_t current_vertices = input.total_vertices();
        if (target_vertices >= current_vertices) return result;

        std::vector<Ring> rings = input.rings;
        std::vector<std::vector<int>> node_ids(rings.size());
        std::vector<std::unordered_map<int, std::size_t>> node_pos(rings.size());
        int next_id = 0;
        std::vector<int> fixed_id(rings.size());

        for (std::size_t ri = 0; ri < rings.size(); ++ri) {
            rings[ri].ring_id = input.rings[ri].ring_id;
            node_ids[ri].resize(rings[ri].vertices.size());
            for (std::size_t i = 0; i < rings[ri].vertices.size(); ++i) {
                node_ids[ri][i] = next_id;
                node_pos[ri][next_id] = i;
                ++next_id;
            }
            fixed_id[ri] = node_ids[ri][0];
        }

        std::priority_queue<QueueEntry, std::vector<QueueEntry>,
            std::greater<QueueEntry>> heap;
        std::size_t sequence = 0;

        auto push_candidate = [&](int ring_id, std::size_t start) {
            const Ring& ring = rings[static_cast<std::size_t>(ring_id)];
            const std::size_t n = ring.vertices.size();
            if (n < 4) return;

            const std::size_t a = start % n;
            const std::size_t b = (a + 1) % n;
            const std::size_t c = (a + 2) % n;
            const std::size_t d = (a + 3) % n;

            if (node_ids[ring_id][b] == fixed_id[ring_id] ||
                node_ids[ring_id][c] == fixed_id[ring_id]) return;

            const auto E = compute_apsc_point(
                ring.vertices[a], ring.vertices[b],
                ring.vertices[c], ring.vertices[d]);
            if (!E) return;

            const double cost = compute_areal_displacement(
                ring.vertices[a], ring.vertices[b],
                ring.vertices[c], ring.vertices[d], *E);

            heap.push(QueueEntry{
                normalise_cost(cost),
                E->y,
                a,
                sequence++,
                ring_id,
                node_ids[ring_id][a],
                node_ids[ring_id][b],
                node_ids[ring_id][c],
                node_ids[ring_id][d],
                });
            ++result.seeded_candidate_windows;
            };

        for (std::size_t ri = 0; ri < rings.size(); ++ri)
            for (std::size_t v = 0; v < rings[ri].vertices.size(); ++v)
                push_candidate(static_cast<int>(ri), v);

        while (current_vertices > target_vertices && !heap.empty()) {
            const QueueEntry entry = heap.top();
            heap.pop();

            if (entry.ring_id < 0 ||
                static_cast<std::size_t>(entry.ring_id) >= rings.size())
                continue;

            Ring& ring = rings[static_cast<std::size_t>(entry.ring_id)];
            const std::size_t n = ring.vertices.size();
            if (n < 4) continue;

            auto it = node_pos[entry.ring_id].find(entry.id_a);
            if (it == node_pos[entry.ring_id].end()) continue;
            const std::size_t a = it->second;
            const std::size_t b = (a + 1) % n;
            const std::size_t c = (a + 2) % n;
            const std::size_t d = (a + 3) % n;

            if (node_ids[entry.ring_id][b] != entry.id_b ||
                node_ids[entry.ring_id][c] != entry.id_c ||
                node_ids[entry.ring_id][d] != entry.id_d)
                continue;

            if (node_ids[entry.ring_id][b] == fixed_id[entry.ring_id] ||
                node_ids[entry.ring_id][c] == fixed_id[entry.ring_id])
                continue;

            const Point& pA = ring.vertices[a];
            const Point& pB = ring.vertices[b];
            const Point& pC = ring.vertices[c];
            const Point& pD = ring.vertices[d];

            const auto E = compute_apsc_point(pA, pB, pC, pD);
            if (!E) continue;

            {
                Polygon tmp;
                tmp.rings = rings;
                if (!topology_safe(tmp, entry.ring_id, a, b, c, d, *E))
                    continue;
            }

            const double disp = compute_areal_displacement(pA, pB, pC, pD, *E);
            result.areal_displacement += disp;

            const std::size_t hi = std::max(b, c);
            const std::size_t lo = std::min(b, c);

            ring.vertices.erase(ring.vertices.begin() + static_cast<std::ptrdiff_t>(hi));
            ring.vertices.erase(ring.vertices.begin() + static_cast<std::ptrdiff_t>(lo));

            node_pos[entry.ring_id].erase(node_ids[entry.ring_id][hi]);
            node_ids[entry.ring_id].erase(
                node_ids[entry.ring_id].begin() + static_cast<std::ptrdiff_t>(hi));

            node_pos[entry.ring_id].erase(node_ids[entry.ring_id][lo]);
            node_ids[entry.ring_id].erase(
                node_ids[entry.ring_id].begin() + static_cast<std::ptrdiff_t>(lo));

            ring.vertices.insert(
                ring.vertices.begin() + static_cast<std::ptrdiff_t>(lo), *E);
            const int eid = next_id++;
            node_ids[entry.ring_id].insert(
                node_ids[entry.ring_id].begin() + static_cast<std::ptrdiff_t>(lo), eid);

            for (std::size_t i = lo; i < node_ids[entry.ring_id].size(); ++i)
                node_pos[entry.ring_id][node_ids[entry.ring_id][i]] = i;

            --current_vertices;
            ++result.successful_collapses;

            const std::size_t new_n = ring.vertices.size();
            const std::size_t e_pos = lo % new_n;

            for (int offset = -3; offset <= 0; ++offset) {
                const std::size_t start = static_cast<std::size_t>(
                    (static_cast<long long>(e_pos) + offset +
                        static_cast<long long>(new_n)) %
                    static_cast<long long>(new_n));
                push_candidate(entry.ring_id, start);
            }
        }

        result.polygon.rings = std::move(rings);
        result.output_area = total_signed_area(result.polygon);
        return result;
    }

    std::vector<CollapseCandidate> Simplifier::build_initial_candidates(
        const Polygon& polygon) const {

        std::vector<CollapseCandidate> candidates;
        for (const Ring& ring : polygon.rings) {
            if (ring.vertices.size() < 4) continue;
            for (std::size_t i = 0; i < ring.vertices.size(); ++i) {
                if (const auto c = compute_candidate(ring, i))
                    candidates.push_back(*c);
            }
        }
        return candidates;
    }

    std::optional<CollapseCandidate> Simplifier::compute_candidate(
        const Ring& ring, std::size_t start_index) const {

        const std::size_t n = ring.vertices.size();
        if (n < 4) return std::nullopt;

        const std::size_t a = start_index % n;
        const std::size_t b = (a + 1) % n;
        const std::size_t c = (a + 2) % n;
        const std::size_t d = (a + 3) % n;

        const auto E = compute_apsc_point(
            ring.vertices[a], ring.vertices[b],
            ring.vertices[c], ring.vertices[d]);
        if (!E) return std::nullopt;

        CollapseCandidate cand;
        cand.ring_id = ring.ring_id;
        cand.start_index = a;
        cand.replacement_point = *E;
        cand.estimated_areal_displacement = compute_areal_displacement(
            ring.vertices[a], ring.vertices[b],
            ring.vertices[c], ring.vertices[d], *E);
        return cand;
    }

    bool Simplifier::candidate_is_topology_safe(
        const Polygon& polygon,
        const CollapseCandidate& candidate) const {

        if (candidate.ring_id < 0 ||
            static_cast<std::size_t>(candidate.ring_id) >= polygon.rings.size())
            return false;

        const Ring& ring = polygon.rings[static_cast<std::size_t>(candidate.ring_id)];
        if (ring.vertices.size() < 4) return false;

        const std::size_t n = ring.vertices.size();
        const std::size_t a = candidate.start_index % n;
        const std::size_t b = (a + 1) % n;
        const std::size_t c = (a + 2) % n;
        const std::size_t d = (a + 3) % n;

        return topology_safe(polygon, candidate.ring_id, a, b, c, d,
            candidate.replacement_point);
    }

}  // namespace apsc