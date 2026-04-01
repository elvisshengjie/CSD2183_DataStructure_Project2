#include "simplifier.hpp"

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

bool nearly_equal(const double lhs, const double rhs) {
    return std::abs(lhs - rhs) <= kEpsilon;
}

bool same_point(const Point& lhs, const Point& rhs) {
    return nearly_equal(lhs.x, rhs.x) && nearly_equal(lhs.y, rhs.y);
}

bool point_on_segment(const Point& point, const Point& a, const Point& b) {
    return std::min(a.x, b.x) - kEpsilon <= point.x &&
           point.x <= std::max(a.x, b.x) + kEpsilon &&
           std::min(a.y, b.y) - kEpsilon <= point.y &&
           point.y <= std::max(a.y, b.y) + kEpsilon &&
           nearly_equal(cross(a, b, point), 0.0);
}

bool segments_intersect(
    const Point& a1,
    const Point& a2,
    const Point& b1,
    const Point& b2) {
    const double d1 = cross(b1, b2, a1);
    const double d2 = cross(b1, b2, a2);
    const double d3 = cross(a1, a2, b1);
    const double d4 = cross(a1, a2, b2);

    const bool proper_cross =
        (((d1 > kEpsilon && d2 < -kEpsilon) ||
          (d1 < -kEpsilon && d2 > kEpsilon)) &&
         ((d3 > kEpsilon && d4 < -kEpsilon) ||
          (d3 < -kEpsilon && d4 > kEpsilon)));
    if (proper_cross) {
        return true;
    }

    return (std::abs(d1) < kEpsilon && point_on_segment(a1, b1, b2)) ||
           (std::abs(d2) < kEpsilon && point_on_segment(a2, b1, b2)) ||
           (std::abs(d3) < kEpsilon && point_on_segment(b1, a1, a2)) ||
           (std::abs(d4) < kEpsilon && point_on_segment(b2, a1, a2));
}

bool segment_bounding_boxes_overlap(
    const Point& a1,
    const Point& a2,
    const Point& b1,
    const Point& b2) {
    const double a_min_x = std::min(a1.x, a2.x) - kEpsilon;
    const double a_max_x = std::max(a1.x, a2.x) + kEpsilon;
    const double a_min_y = std::min(a1.y, a2.y) - kEpsilon;
    const double a_max_y = std::max(a1.y, a2.y) + kEpsilon;
    const double b_min_x = std::min(b1.x, b2.x) - kEpsilon;
    const double b_max_x = std::max(b1.x, b2.x) + kEpsilon;
    const double b_min_y = std::min(b1.y, b2.y) - kEpsilon;
    const double b_max_y = std::max(b1.y, b2.y) + kEpsilon;

    return a_min_x <= b_max_x && b_min_x <= a_max_x &&
           a_min_y <= b_max_y && b_min_y <= a_max_y;
}

double point_side_of_line(const Point& point, const Point& a, const Point& b) {
    return cross(a, b, point);
}

double dist_to_line(const Point& point, const Point& a, const Point& b) {
    const double denom = std::hypot(b.x - a.x, b.y - a.y);
    if (denom <= kEpsilon) {
        return 0.0;
    }
    return std::abs(cross(a, b, point)) / denom;
}

double signed_area_points(const std::vector<Point>& vertices) {
    if (vertices.size() < 3) {
        return 0.0;
    }

    double area = 0.0;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const Point& current = vertices[i];
        const Point& next = vertices[(i + 1) % vertices.size()];
        area += current.x * next.y - next.x * current.y;
    }
    return 0.5 * area;
}

struct Line {
    double a {};
    double b {};
    double c {};
};

Line compute_e_line(const Point& a, const Point& b, const Point& c, const Point& d) {
    return {
        d.y - a.y,
        a.x - d.x,
        -b.y * a.x + (a.y - c.y) * b.x + (b.y - d.y) * c.x + c.y * d.x,
    };
}

std::optional<Point> line_line_intersection(
    const Point& p1,
    const Point& p2,
    const Point& q1,
    const Point& q2) {
    const double a1 = p2.y - p1.y;
    const double b1 = p1.x - p2.x;
    const double c1 = a1 * p1.x + b1 * p1.y;

    const double a2 = q2.y - q1.y;
    const double b2 = q1.x - q2.x;
    const double c2 = a2 * q1.x + b2 * q1.y;

    const double det = a1 * b2 - a2 * b1;
    if (std::abs(det) <= kEpsilon) {
        return std::nullopt;
    }

    return Point {
        (b2 * c1 - b1 * c2) / det,
        (a1 * c2 - a2 * c1) / det,
    };
}

std::optional<Point> intersect_e_line_with_segment(
    const Line& line,
    const Point& p1,
    const Point& p2) {
    Point ep1;
    Point ep2;

    if (std::abs(line.b) > kEpsilon) {
        ep1 = {0.0, -line.c / line.b};
        ep2 = {1.0, -(line.a + line.c) / line.b};
    } else if (std::abs(line.a) > kEpsilon) {
        ep1 = {-line.c / line.a, 0.0};
        ep2 = {-line.c / line.a, 1.0};
    } else {
        return std::nullopt;
    }

    return line_line_intersection(ep1, ep2, p1, p2);
}

std::optional<Point> compute_apsc_point(
    const Point& a,
    const Point& b,
    const Point& c,
    const Point& d) {
    const Line e_line = compute_e_line(a, b, c, d);
    const auto intersection_ab = intersect_e_line_with_segment(e_line, a, b);
    const auto intersection_cd = intersect_e_line_with_segment(e_line, c, d);
    const double side_b = point_side_of_line(b, a, d);
    const double side_c = point_side_of_line(c, a, d);
    const bool same_side = (side_b * side_c) > 0.0;

    auto choose_intersection = [&](const bool prefer_ab) -> std::optional<Point> {
        if (prefer_ab) {
            if (intersection_ab) {
                return intersection_ab;
            }
            return intersection_cd;
        }

        if (intersection_cd) {
            return intersection_cd;
        }
        return intersection_ab;
    };

    if (same_side) {
        const double dist_b = dist_to_line(b, a, d);
        const double dist_c = dist_to_line(c, a, d);
        if (dist_b > dist_c + kEpsilon) {
            return choose_intersection(false);
        }
        return choose_intersection(true);
    }

    Point point_on_e_line;
    if (std::abs(e_line.b) > kEpsilon) {
        point_on_e_line = {0.0, -e_line.c / e_line.b};
    } else {
        point_on_e_line = {-e_line.c / e_line.a, 0.0};
    }

    const double side_e_line = point_side_of_line(point_on_e_line, a, d);
    return choose_intersection((side_b > 0.0) == (side_e_line > 0.0));
}

double compute_areal_displacement(
    const Point& a,
    const Point& b,
    const Point& c,
    const Point& d,
    const Point& e) {
    const bool on_ab = nearly_equal(cross(a, b, e), 0.0);
    const bool on_cd = nearly_equal(cross(c, d, e), 0.0);

    if (on_ab && !on_cd) {
        if (const auto split = line_line_intersection(e, d, b, c)) {
            return std::abs(signed_area_points({b, e, *split})) +
                   std::abs(signed_area_points({*split, c, d}));
        }
    }

    if (on_cd && !on_ab) {
        if (const auto split = line_line_intersection(a, e, b, c)) {
            return std::abs(signed_area_points({a, b, *split})) +
                   std::abs(signed_area_points({*split, e, c}));
        }
    }

    return std::abs(signed_area_points({a, b, e})) +
           std::abs(signed_area_points({e, c, d}));
}

bool check_candidate_topology(
    const Polygon& polygon,
    const CollapseCandidate& candidate,
    const Ring& ring,
    const std::size_t a_idx,
    const std::size_t b_idx,
    const std::size_t c_idx,
    const std::size_t d_idx,
    const Point& replacement) {
    const Point& a = ring.vertices[a_idx];
    const Point& d = ring.vertices[d_idx];

    for (std::size_t ring_index = 0; ring_index < polygon.rings.size(); ++ring_index) {
        const Ring& test_ring = polygon.rings[ring_index];
        const std::size_t n = test_ring.vertices.size();

        for (std::size_t i = 0; i < n; ++i) {
            const Point& p1 = test_ring.vertices[i];
            const Point& p2 = test_ring.vertices[(i + 1) % n];

            if (ring_index == static_cast<std::size_t>(candidate.ring_id)) {
                const bool is_ab = i == a_idx && (i + 1) % n == b_idx;
                const bool is_bc = i == b_idx && (i + 1) % n == c_idx;
                const bool is_cd = i == c_idx && (i + 1) % n == d_idx;
                const bool is_da = i == d_idx && (i + 1) % n == a_idx;
                if (is_ab || is_bc || is_cd || is_da) {
                    continue;
                }
                if (same_point(p1, a) || same_point(p2, a) ||
                    same_point(p1, d) || same_point(p2, d)) {
                    continue;
                }
            }

            if (segment_bounding_boxes_overlap(a, replacement, p1, p2) &&
                segments_intersect(a, replacement, p1, p2)) {
                return false;
            }
            if (segment_bounding_boxes_overlap(replacement, d, p1, p2) &&
                segments_intersect(replacement, d, p1, p2)) {
                return false;
            }
        }
    }

    return true;
}

struct QueueEntry {
    double cost {};
    double replacement_y {};
    std::size_t start_index {};
    std::size_t sequence {};
    int ring_id {};
    int id_a {};
    int id_b {};
    int id_c {};
    int id_d {};

    bool operator>(const QueueEntry& other) const {
        if (cost != other.cost) {
            return cost > other.cost;
        }
        if (replacement_y != other.replacement_y) {
            return replacement_y > other.replacement_y;
        }
        if (start_index != other.start_index) {
            return start_index < other.start_index;
        }
        return sequence > other.sequence;
    }
};

double normalized_queue_cost(const double cost) {
    if (std::abs(cost) < kZeroCostEpsilon) {
        return 0.0;
    }
    return std::round(cost * 1e12) / 1e12;
}

}  // namespace

SimplificationResult Simplifier::simplify(
    const Polygon& input,
    const std::size_t target_vertices) const {
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
    std::vector<std::vector<int>> node_ids(rings.size());
    std::vector<std::unordered_map<int, std::size_t>> node_positions(rings.size());
    std::vector<int> fixed_node_ids(rings.size());
    int next_node_id = 0;
    for (std::size_t ring_index = 0; ring_index < rings.size(); ++ring_index) {
        node_ids[ring_index].resize(rings[ring_index].vertices.size());
        for (std::size_t i = 0; i < rings[ring_index].vertices.size(); ++i) {
            node_ids[ring_index][i] = next_node_id++;
            node_positions[ring_index][node_ids[ring_index][i]] = i;
        }
        fixed_node_ids[ring_index] = node_ids[ring_index].front();
    }

    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>>
        heap;
    std::size_t sequence = 0;

    auto push_candidate = [&](const int ring_id, const std::size_t start_index) {
        const Ring& ring = rings[ring_id];
        const std::size_t n = ring.vertices.size();
        if (n < 4) {
            return;
        }

        const std::size_t a = ((start_index % n) + n) % n;
        const std::size_t b = (a + 1) % n;
        const std::size_t c = (a + 2) % n;
        const std::size_t d = (a + 3) % n;
        if (node_ids[ring_id][b] == fixed_node_ids[ring_id] ||
            node_ids[ring_id][c] == fixed_node_ids[ring_id]) {
            return;
        }

        const std::optional<Point> replacement = compute_apsc_point(
            ring.vertices[a], ring.vertices[b], ring.vertices[c], ring.vertices[d]);
        if (!replacement) {
            return;
        }

        const double cost = compute_areal_displacement(
            ring.vertices[a],
            ring.vertices[b],
            ring.vertices[c],
            ring.vertices[d],
            *replacement);

        heap.push(QueueEntry {
            normalized_queue_cost(cost),
            replacement->y,
            a,
            sequence++,
            ring_id,
            node_ids[ring_id][a],
            node_ids[ring_id][b],
            node_ids[ring_id][c],
            node_ids[ring_id][d],
        });
    };

    for (std::size_t ring_index = 0; ring_index < rings.size(); ++ring_index) {
        for (std::size_t start_index = 0;
             start_index < rings[ring_index].vertices.size();
             ++start_index) {
            push_candidate(static_cast<int>(ring_index), start_index);
            ++result.seeded_candidate_windows;
        }
    }

    while (current_vertices > target_vertices && !heap.empty()) {
        const QueueEntry candidate = heap.top();
        heap.pop();

        if (candidate.ring_id < 0 ||
            static_cast<std::size_t>(candidate.ring_id) >= rings.size()) {
            continue;
        }

        Ring& ring = rings[static_cast<std::size_t>(candidate.ring_id)];
        const std::size_t n = ring.vertices.size();
        if (n < 4) {
            continue;
        }

        const auto node_position =
            node_positions[candidate.ring_id].find(candidate.id_a);
        if (node_position == node_positions[candidate.ring_id].end()) {
            continue;
        }
        const std::size_t a = node_position->second;
        const std::size_t b = (a + 1) % n;
        const std::size_t c = (a + 2) % n;
        const std::size_t d = (a + 3) % n;

        if (node_ids[candidate.ring_id][b] != candidate.id_b ||
            node_ids[candidate.ring_id][c] != candidate.id_c ||
            node_ids[candidate.ring_id][d] != candidate.id_d) {
            continue;
        }
        if (node_ids[candidate.ring_id][b] == fixed_node_ids[candidate.ring_id] ||
            node_ids[candidate.ring_id][c] == fixed_node_ids[candidate.ring_id]) {
            continue;
        }

        if (((a + 1) % n) != b || ((b + 1) % n) != c || ((c + 1) % n) != d) {
            continue;
        }

        const Point& point_a = ring.vertices[a];
        const Point& point_b = ring.vertices[b];
        const Point& point_c = ring.vertices[c];
        const Point& point_d = ring.vertices[d];

        const std::optional<Point> replacement =
            compute_apsc_point(point_a, point_b, point_c, point_d);
        if (!replacement) {
            continue;
        }

        CollapseCandidate collapse;
        collapse.ring_id = candidate.ring_id;
        collapse.start_index = a;
        collapse.replacement_point = *replacement;
        collapse.estimated_areal_displacement = compute_areal_displacement(
            point_a, point_b, point_c, point_d, *replacement);

        Polygon working_polygon;
        working_polygon.rings = rings;
        if (!candidate_is_topology_safe(working_polygon, collapse)) {
            continue;
        }

        result.areal_displacement += collapse.estimated_areal_displacement;

        const std::size_t remove_high = std::max(b, c);
        const std::size_t remove_low = std::min(b, c);

        ring.vertices.erase(ring.vertices.begin() + static_cast<std::ptrdiff_t>(remove_high));
        ring.vertices.erase(ring.vertices.begin() + static_cast<std::ptrdiff_t>(remove_low));
        node_ids[candidate.ring_id].erase(
            node_ids[candidate.ring_id].begin() +
            static_cast<std::ptrdiff_t>(remove_high));
        node_ids[candidate.ring_id].erase(
            node_ids[candidate.ring_id].begin() +
            static_cast<std::ptrdiff_t>(remove_low));
        node_positions[candidate.ring_id].erase(candidate.id_b);
        node_positions[candidate.ring_id].erase(candidate.id_c);

        const std::size_t insert_pos = remove_low;
        ring.vertices.insert(
            ring.vertices.begin() + static_cast<std::ptrdiff_t>(insert_pos),
            *replacement);
        const int replacement_id = next_node_id++;
        node_ids[candidate.ring_id].insert(
            node_ids[candidate.ring_id].begin() +
                static_cast<std::ptrdiff_t>(insert_pos),
            replacement_id);

        for (std::size_t index = insert_pos; index < node_ids[candidate.ring_id].size(); ++index) {
            node_positions[candidate.ring_id][node_ids[candidate.ring_id][index]] = index;
        }

        --current_vertices;
        ++result.successful_collapses;

        const std::size_t new_n = ring.vertices.size();
        const std::size_t new_e = insert_pos % new_n;

        for (int offset = -3; offset <= 0; ++offset) {
            const std::size_t start = static_cast<std::size_t>(
                (static_cast<long long>(new_e) + offset + static_cast<long long>(new_n)) %
                static_cast<long long>(new_n));
            push_candidate(candidate.ring_id, start);
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
        if (ring.vertices.size() < 4) {
            continue;
        }
        for (std::size_t start_index = 0; start_index < ring.vertices.size();
             ++start_index) {
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
    const std::size_t n = ring.vertices.size();
    if (n < 4) {
        return std::nullopt;
    }

    const std::size_t a = start_index % n;
    const std::size_t b = (a + 1) % n;
    const std::size_t c = (a + 2) % n;
    const std::size_t d = (a + 3) % n;

    const std::optional<Point> replacement = compute_apsc_point(
        ring.vertices[a], ring.vertices[b], ring.vertices[c], ring.vertices[d]);
    if (!replacement) {
        return std::nullopt;
    }

    CollapseCandidate candidate;
    candidate.ring_id = ring.ring_id;
    candidate.start_index = a;
    candidate.replacement_point = *replacement;
    candidate.estimated_areal_displacement = compute_areal_displacement(
        ring.vertices[a],
        ring.vertices[b],
        ring.vertices[c],
        ring.vertices[d],
        *replacement);
    return candidate;
}

bool Simplifier::candidate_is_topology_safe(
    const Polygon& polygon,
    const CollapseCandidate& candidate) const {
    if (candidate.ring_id < 0 ||
        static_cast<std::size_t>(candidate.ring_id) >= polygon.rings.size()) {
        return false;
    }

    const Ring& ring = polygon.rings[static_cast<std::size_t>(candidate.ring_id)];
    if (ring.vertices.size() < 4) {
        return false;
    }

    const std::size_t n = ring.vertices.size();
    const std::size_t a = candidate.start_index % n;
    const std::size_t b = (a + 1) % n;
    const std::size_t c = (a + 2) % n;
    const std::size_t d = (a + 3) % n;

    return check_candidate_topology(
        polygon,
        candidate,
        ring,
        a,
        b,
        c,
        d,
        candidate.replacement_point);
}

}  // namespace apsc
