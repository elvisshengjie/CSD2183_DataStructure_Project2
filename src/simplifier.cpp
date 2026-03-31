#include "simplifier.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <queue>
#include <vector>

namespace apsc {
    namespace {

        constexpr double kEpsilon = 1e-9;

        // 2D cross product
        double cross(const Point& a, const Point& b, const Point& c) {
            return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        }

        bool nearly_equal(double a, double b) {
            return std::abs(a - b) <= kEpsilon;
        }

        bool point_on_segment(const Point& p, const Point& a, const Point& b) {
            return std::min(a.x, b.x) - kEpsilon <= p.x && p.x <= std::max(a.x, b.x) + kEpsilon &&
                std::min(a.y, b.y) - kEpsilon <= p.y && p.y <= std::max(a.y, b.y) + kEpsilon &&
                nearly_equal(cross(a, b, p), 0.0);
        }

        bool segments_intersect(const Point& a1, const Point& a2, const Point& b1, const Point& b2) {
            double d1 = cross(a1, a2, b1);
            double d2 = cross(a1, a2, b2);
            double d3 = cross(b1, b2, a1);
            double d4 = cross(b1, b2, a2);

            bool proper_cross = ((d1 > kEpsilon && d2 < -kEpsilon) || (d1 < -kEpsilon && d2 > kEpsilon)) &&
                ((d3 > kEpsilon && d4 < -kEpsilon) || (d3 < -kEpsilon && d4 > kEpsilon));

            if (proper_cross) return true;

            return (nearly_equal(d1, 0.0) && point_on_segment(b1, a1, a2)) ||
                (nearly_equal(d2, 0.0) && point_on_segment(b2, a1, a2)) ||
                (nearly_equal(d3, 0.0) && point_on_segment(a1, b1, b2)) ||
                (nearly_equal(d4, 0.0) && point_on_segment(a2, b1, b2));
        }

        double point_side(const Point& p, const Point& a, const Point& b) {
            return cross(a, b, p);
        }

        double point_line_distance(const Point& p, const Point& a, const Point& b) {
            return std::abs(cross(a, b, p)) / std::hypot(b.x - a.x, b.y - a.y);
        }

        struct Line {
            double a, b, c;
        };

        Line compute_area_preserving_line(const Point& A, const Point& B, const Point& C, const Point& D) {
            double a = D.y - A.y;
            double b = A.x - D.x;
            double c = -B.y * A.x + (A.y - C.y) * B.x + (B.y - D.y) * C.x + C.y * D.x;
            return { a, b, c };
        }

        std::optional<Point> line_segment_intersection(const Line& L, const Point& P1, const Point& P2) {
            double dx = P2.x - P1.x;
            double dy = P2.y - P1.y;

            double denominator = L.a * dx + L.b * dy;
            if (std::abs(denominator) < kEpsilon) {
                return std::nullopt;
            }

            double t = -(L.a * P1.x + L.b * P1.y + L.c) / denominator;

            if (t < -kEpsilon || t > 1.0 + kEpsilon) {
                return std::nullopt;
            }

            t = std::max(0.0, std::min(1.0, t));

            return Point{ P1.x + t * dx, P1.y + t * dy };
        }

        std::optional<Point> compute_apsc_point(const Point& A, const Point& B, const Point& C, const Point& D) {
            Line E_line = compute_area_preserving_line(A, B, C, D);

            if (std::abs(E_line.a) < kEpsilon && std::abs(E_line.b) < kEpsilon) {
                return Point{ (A.x + D.x) * 0.5, (A.y + D.y) * 0.5 };
            }

            double side_B = point_side(B, A, D);
            double side_C = point_side(C, A, D);
            bool same_side = (side_B * side_C) > 0;

            Point line_point;
            if (std::abs(E_line.b) > kEpsilon) {
                line_point = Point{ 0.0, -E_line.c / E_line.b };
            }
            else {
                line_point = Point{ -E_line.c / E_line.a, 0.0 };
            }
            double side_line = point_side(line_point, A, D);

            std::optional<Point> E;

            if (same_side) {
                double dist_B = point_line_distance(B, A, D);
                double dist_C = point_line_distance(C, A, D);

                if (dist_B > dist_C) {
                    E = line_segment_intersection(E_line, A, B);
                }
                else {
                    E = line_segment_intersection(E_line, C, D);
                }
            }
            else {
                if ((side_B > 0) == (side_line > 0)) {
                    E = line_segment_intersection(E_line, A, B);
                }
                else {
                    E = line_segment_intersection(E_line, C, D);
                }
            }

            if (!E) {
                E = line_segment_intersection(E_line, A, D);
            }

            return E;
        }

        double compute_areal_displacement(const Point& A, const Point& B, const Point& C, const Point& D, const Point& E) {
            double area_abe = std::abs(cross(A, B, E)) * 0.5;
            double area_ecd = std::abs(cross(E, C, D)) * 0.5;
            return area_abe + area_ecd;
        }

        bool check_candidate_topology(const Polygon& polygon, const CollapseCandidate& candidate,
            const Ring& ring, size_t a_idx, size_t b_idx, size_t c_idx, size_t d_idx,
            const Point& E) {
            const Point& A = ring.vertices[a_idx];
            const Point& D = ring.vertices[d_idx];

            auto check_new_edges = [&](const Point& p1, const Point& p2, int exclude_ring_id) {
                for (size_t ri = 0; ri < polygon.rings.size(); ++ri) {
                    const Ring& test_ring = polygon.rings[ri];
                    size_t n = test_ring.vertices.size();

                    for (size_t i = 0; i < n; ++i) {
                        const Point& q1 = test_ring.vertices[i];
                        const Point& q2 = test_ring.vertices[(i + 1) % n];

                        if (ri == static_cast<size_t>(candidate.ring_id)) {
                            if (i == a_idx && (i + 1) % n == b_idx) continue;
                            if (i == b_idx && (i + 1) % n == c_idx) continue;
                            if (i == c_idx && (i + 1) % n == d_idx) continue;
                            if (i == d_idx && (i + 1) % n == a_idx) continue;
                        }

                        if ((q1 == A || q2 == A || q1 == D || q2 == D) && ri == static_cast<size_t>(candidate.ring_id)) {
                            continue;
                        }

                        if (segments_intersect(p1, p2, q1, q2)) {
                            return false;
                        }
                    }
                }
                return true;
                };

            return check_new_edges(A, E, candidate.ring_id) && check_new_edges(E, D, candidate.ring_id);
        }

        struct QueueEntry {
            double cost;
            int ring_id;
            size_t start_index;
            size_t generation;

            bool operator>(const QueueEntry& other) const {
                return cost > other.cost;
            }
        };

    }  // namespace

    SimplificationResult Simplifier::simplify(const Polygon& input, const std::size_t target_vertices) const {
        SimplificationResult result;
        result.polygon = input;
        result.input_area = total_signed_area(input);
        result.output_area = result.input_area;
        result.areal_displacement = 0.0;
        result.seeded_candidate_windows = 0;
        result.successful_collapses = 0;

        std::size_t current_vertices = input.total_vertices();
        if (target_vertices >= current_vertices) {
            return result;
        }

        std::vector<Ring> rings = input.rings;
        std::vector<std::vector<size_t>> generations(rings.size());
        for (size_t i = 0; i < rings.size(); ++i) {
            generations[i].resize(rings[i].vertices.size(), 0);
        }

        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> pq;

        auto push_candidate = [&](int ring_id, size_t start_index, size_t gen) {
            const Ring& ring = rings[ring_id];
            size_t n = ring.vertices.size();
            if (n < 4) return;

            size_t a = start_index % n;
            size_t b = (a + 1) % n;
            size_t c = (a + 2) % n;
            size_t d = (a + 3) % n;

            const Point& A = ring.vertices[a];
            const Point& B = ring.vertices[b];
            const Point& C = ring.vertices[c];
            const Point& D = ring.vertices[d];

            auto E_opt = compute_apsc_point(A, B, C, D);
            if (!E_opt) return;

            double cost = compute_areal_displacement(A, B, C, D, *E_opt);
            pq.push({ cost, ring_id, start_index, gen });
            };

        for (size_t ri = 0; ri < rings.size(); ++ri) {
            size_t n = rings[ri].vertices.size();
            for (size_t si = 0; si < n; ++si) {
                push_candidate(static_cast<int>(ri), si, generations[ri][si]);
                result.seeded_candidate_windows++;
            }
        }

        while (current_vertices > target_vertices && !pq.empty()) {
            QueueEntry entry = pq.top();
            pq.pop();

            int ring_id = entry.ring_id;
            size_t start_index = entry.start_index;

            if (ring_id >= static_cast<int>(rings.size())) continue;
            if (start_index >= rings[ring_id].vertices.size()) continue;
            if (entry.generation != generations[ring_id][start_index]) continue;

            Ring& ring = rings[ring_id];
            size_t n = ring.vertices.size();
            if (n < 4) continue;

            size_t a = start_index % n;
            size_t b = (a + 1) % n;
            size_t c = (a + 2) % n;
            size_t d = (a + 3) % n;

            if (b != (a + 1) % n || c != (a + 2) % n || d != (a + 3) % n) continue;

            const Point& A = ring.vertices[a];
            const Point& B = ring.vertices[b];
            const Point& C = ring.vertices[c];
            const Point& D = ring.vertices[d];

            auto E_opt = compute_apsc_point(A, B, C, D);
            if (!E_opt) continue;

            CollapseCandidate candidate;
            candidate.ring_id = ring_id;
            candidate.start_index = start_index;
            candidate.replacement_point = *E_opt;

            if (!check_candidate_topology(result.polygon, candidate, ring, a, b, c, d, *E_opt)) {
                continue;
            }

            double displacement = compute_areal_displacement(A, B, C, D, *E_opt);
            result.areal_displacement += displacement;

            size_t remove_first = std::max(b, c);
            size_t remove_second = std::min(b, c);

            ring.vertices.erase(ring.vertices.begin() + remove_first);
            generations[ring_id].erase(generations[ring_id].begin() + remove_first);
            ring.vertices.erase(ring.vertices.begin() + remove_second);
            generations[ring_id].erase(generations[ring_id].begin() + remove_second);

            size_t insert_pos = remove_second;
            ring.vertices.insert(ring.vertices.begin() + insert_pos, *E_opt);
            generations[ring_id].insert(generations[ring_id].begin() + insert_pos, 0);

            current_vertices--;

            size_t new_n = ring.vertices.size();
            size_t new_a = insert_pos;
            size_t new_e = (insert_pos + 1) % new_n;
            size_t new_d = (insert_pos + 2) % new_n;

            generations[ring_id][new_a]++;
            generations[ring_id][new_e]++;
            generations[ring_id][new_d]++;

            result.successful_collapses++;

            for (int offset = -3; offset <= 0; ++offset) {
                size_t si = (static_cast<size_t>(static_cast<int>(new_e) + offset + new_n) % new_n);
                push_candidate(ring_id, si, generations[ring_id][si]);
            }

            for (int offset = -4; offset <= 3; ++offset) {
                size_t si = (static_cast<size_t>(static_cast<int>(new_a) + offset + new_n) % new_n);
                push_candidate(ring_id, si, generations[ring_id][si]);
            }
        }

        result.polygon.rings = std::move(rings);
        result.output_area = total_signed_area(result.polygon);

        return result;
    }

    std::vector<CollapseCandidate> Simplifier::build_initial_candidates(const Polygon& polygon) const {
        std::vector<CollapseCandidate> candidates;

        for (const Ring& ring : polygon.rings) {
            if (ring.vertices.size() < 4) continue;

            for (std::size_t start_index = 0; start_index < ring.vertices.size(); ++start_index) {
                if (auto candidate = compute_candidate(ring, start_index)) {
                    candidates.push_back(*candidate);
                }
            }
        }

        return candidates;
    }

    std::optional<CollapseCandidate> Simplifier::compute_candidate(const Ring& ring, const std::size_t start_index) const {
        if (ring.vertices.size() < 4) return std::nullopt;

        std::size_t n = ring.vertices.size();
        std::size_t a_idx = start_index % n;
        std::size_t b_idx = (a_idx + 1) % n;
        std::size_t c_idx = (a_idx + 2) % n;
        std::size_t d_idx = (a_idx + 3) % n;

        const Point& A = ring.vertices[a_idx];
        const Point& B = ring.vertices[b_idx];
        const Point& C = ring.vertices[c_idx];
        const Point& D = ring.vertices[d_idx];

        auto E_opt = compute_apsc_point(A, B, C, D);
        if (!E_opt) return std::nullopt;

        CollapseCandidate candidate;
        candidate.ring_id = ring.ring_id;
        candidate.start_index = start_index;
        candidate.replacement_point = *E_opt;
        candidate.estimated_areal_displacement = compute_areal_displacement(A, B, C, D, *E_opt);

        return candidate;
    }

    bool Simplifier::candidate_is_topology_safe(const Polygon& polygon, const CollapseCandidate& candidate) const {
        const Ring* target_ring = nullptr;
        for (const Ring& ring : polygon.rings) {
            if (ring.ring_id == candidate.ring_id) {
                target_ring = &ring;
                break;
            }
        }
        if (!target_ring) return false;

        std::size_t n = target_ring->vertices.size();
        std::size_t a_idx = candidate.start_index % n;
        std::size_t b_idx = (a_idx + 1) % n;
        std::size_t c_idx = (a_idx + 2) % n;
        std::size_t d_idx = (a_idx + 3) % n;

        return check_candidate_topology(polygon, candidate, *target_ring, a_idx, b_idx, c_idx, d_idx,
            candidate.replacement_point);
    }

}  // namespace apsc