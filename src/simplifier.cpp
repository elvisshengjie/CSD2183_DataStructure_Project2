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

std::size_t candidate_seed_stride(const std::size_t ring_vertices) {
    if (ring_vertices > 200000) {
        return 2048;
    }
    if (ring_vertices > 50000) {
        return 128;
    }
    if (ring_vertices > 20000) {
        return 16;
    }
    if (ring_vertices > 8000) {
        return 4;
    }
    return 1;
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
        if (dist_b + kEpsilon >= dist_c) {
            return choose_intersection(true);
        }
        return choose_intersection(false);
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

Polygon apply_candidate_to_polygon(
    const Polygon& polygon,
    const CollapseCandidate& candidate,
    const std::size_t b_idx,
    const std::size_t c_idx) {
    Polygon collapsed = polygon;
    Ring& ring = collapsed.rings[static_cast<std::size_t>(candidate.ring_id)];
    const std::size_t remove_high = std::max(b_idx, c_idx);
    const std::size_t remove_low = std::min(b_idx, c_idx);

    ring.vertices.erase(ring.vertices.begin() + static_cast<std::ptrdiff_t>(remove_high));
    ring.vertices.erase(ring.vertices.begin() + static_cast<std::ptrdiff_t>(remove_low));
    ring.vertices.insert(
        ring.vertices.begin() + static_cast<std::ptrdiff_t>(remove_low),
        candidate.replacement_point);

    return collapsed;
}

struct Node {
    Point point {};
    int prev {-1};
    int next {-1};
    int ring_id {-1};
    double order_label {};
    bool alive {false};
    bool fixed {false};
};

struct RingState {
    int ring_id {-1};
    int anchor {-1};
    double max_label {};
    std::size_t size {};
};

struct EdgeRecord {
    Point a {};
    Point b {};
    int from {-1};
    int to {-1};
    int ring_id {-1};
    bool active {false};
};

class SegmentGrid {
public:
    explicit SegmentGrid(double cell_size)
        : cell_size_(std::max(cell_size, 1e-6)) {}

    void ensure_capacity(const std::size_t edge_count) {
        if (visited_.size() < edge_count) {
            visited_.resize(edge_count, 0);
        }
    }

    void add_edge(const int edge_id, const Point& a, const Point& b) {
        ensure_capacity(static_cast<std::size_t>(edge_id) + 1);
        const auto [min_x, max_x, min_y, max_y] = segment_bounds(a, b);
        const int cell_min_x = to_cell(min_x);
        const int cell_max_x = to_cell(max_x);
        const int cell_min_y = to_cell(min_y);
        const int cell_max_y = to_cell(max_y);

        for (int x = cell_min_x; x <= cell_max_x; ++x) {
            for (int y = cell_min_y; y <= cell_max_y; ++y) {
                cells_[pack_cell(x, y)].push_back(edge_id);
            }
        }
    }

    template <typename EdgeFilter>
    bool any_intersection(
        const Point& a,
        const Point& b,
        const std::vector<EdgeRecord>& edges,
        EdgeFilter&& should_skip) {
        if (visited_.empty()) {
            return false;
        }

        ++query_stamp_;
        if (query_stamp_ == 0) {
            std::fill(visited_.begin(), visited_.end(), 0);
            query_stamp_ = 1;
        }

        const auto [min_x, max_x, min_y, max_y] = segment_bounds(a, b);
        const int cell_min_x = to_cell(min_x);
        const int cell_max_x = to_cell(max_x);
        const int cell_min_y = to_cell(min_y);
        const int cell_max_y = to_cell(max_y);

        for (int x = cell_min_x; x <= cell_max_x; ++x) {
            for (int y = cell_min_y; y <= cell_max_y; ++y) {
                const auto cell_it = cells_.find(pack_cell(x, y));
                if (cell_it == cells_.end()) {
                    continue;
                }

                for (const int edge_id : cell_it->second) {
                    if (edge_id < 0 ||
                        static_cast<std::size_t>(edge_id) >= edges.size()) {
                        continue;
                    }
                    if (visited_[static_cast<std::size_t>(edge_id)] == query_stamp_) {
                        continue;
                    }
                    visited_[static_cast<std::size_t>(edge_id)] = query_stamp_;

                    const EdgeRecord& edge = edges[static_cast<std::size_t>(edge_id)];
                    if (!edge.active || should_skip(edge)) {
                        continue;
                    }

                    if (!segment_bounding_boxes_overlap(a, b, edge.a, edge.b)) {
                        continue;
                    }
                    if (segments_intersect(a, b, edge.a, edge.b)) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

private:
    static std::tuple<double, double, double, double> segment_bounds(
        const Point& a,
        const Point& b) {
        return {
            std::min(a.x, b.x) - kEpsilon,
            std::max(a.x, b.x) + kEpsilon,
            std::min(a.y, b.y) - kEpsilon,
            std::max(a.y, b.y) + kEpsilon,
        };
    }

    int to_cell(const double value) const {
        return static_cast<int>(std::floor(value / cell_size_));
    }

    static long long pack_cell(const int x, const int y) {
        return (static_cast<long long>(x) << 32) ^
               static_cast<unsigned int>(y);
    }

    double cell_size_ {};
    unsigned int query_stamp_ {};
    std::unordered_map<long long, std::vector<int>> cells_;
    std::vector<unsigned int> visited_;
};

struct QueueEntry {
    double cost {};
    double replacement_y {};
    double start_order {};
    int a_node {-1};
    int b_node {-1};
    int c_node {-1};
    int d_node {-1};
    std::size_t sequence {};
    int ring_id {-1};

    bool operator>(const QueueEntry& other) const {
        if (cost != other.cost) {
            return cost > other.cost;
        }
        if (replacement_y != other.replacement_y) {
            return replacement_y > other.replacement_y;
        }
        if (start_order != other.start_order) {
            return start_order < other.start_order;
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

    double min_x = input.rings.front().vertices.front().x;
    double max_x = min_x;
    double min_y = input.rings.front().vertices.front().y;
    double max_y = min_y;
    double perimeter_sum = 0.0;
    for (const Ring& ring : input.rings) {
        for (std::size_t i = 0; i < ring.vertices.size(); ++i) {
            const Point& current = ring.vertices[i];
            const Point& next = ring.vertices[(i + 1) % ring.vertices.size()];
            min_x = std::min(min_x, current.x);
            max_x = std::max(max_x, current.x);
            min_y = std::min(min_y, current.y);
            max_y = std::max(max_y, current.y);
            perimeter_sum += std::hypot(next.x - current.x, next.y - current.y);
        }
    }
    const double avg_edge_length =
        perimeter_sum / static_cast<double>(std::max<std::size_t>(current_vertices, 1));
    const double span = std::max(max_x - min_x, max_y - min_y);
    const double cell_size = std::max({
        avg_edge_length * 4.0,
        span / 512.0,
        1e-6,
    });

    std::vector<Node> nodes;
    nodes.reserve(current_vertices + (current_vertices - target_vertices));
    std::vector<RingState> rings(input.rings.size());
    std::vector<EdgeRecord> edges;
    edges.reserve(current_vertices + (current_vertices - target_vertices));
    SegmentGrid grid(cell_size);
    std::size_t dirty_edge_updates = 0;

    auto append_node =
        [&](const Point& point, const int ring_id, const bool fixed, const double order_label) {
            nodes.push_back(Node {point, -1, -1, ring_id, order_label, true, fixed});
        edges.push_back(EdgeRecord {});
        grid.ensure_capacity(edges.size());
        return static_cast<int>(nodes.size() - 1);
    };

    for (std::size_t ring_index = 0; ring_index < input.rings.size(); ++ring_index) {
        const Ring& input_ring = input.rings[ring_index];
        RingState& ring = rings[ring_index];
        ring.ring_id = input_ring.ring_id;
        ring.size = input_ring.vertices.size();

        std::vector<int> ring_nodes;
        ring_nodes.reserve(input_ring.vertices.size());
        for (std::size_t i = 0; i < input_ring.vertices.size(); ++i) {
            ring_nodes.push_back(append_node(
                input_ring.vertices[i],
                static_cast<int>(ring_index),
                i == 0,
                static_cast<double>(i)));
        }

        const std::size_t ring_size = ring_nodes.size();
        ring.anchor = ring_nodes.front();
        ring.max_label = static_cast<double>(ring_size - 1);
        for (std::size_t i = 0; i < ring_size; ++i) {
            const int node_id = ring_nodes[i];
            nodes[static_cast<std::size_t>(node_id)].prev =
                ring_nodes[(i + ring_size - 1) % ring_size];
            nodes[static_cast<std::size_t>(node_id)].next =
                ring_nodes[(i + 1) % ring_size];
        }

        for (const int node_id : ring_nodes) {
            const int next_id = nodes[static_cast<std::size_t>(node_id)].next;
            edges[static_cast<std::size_t>(node_id)] = EdgeRecord {
                nodes[static_cast<std::size_t>(node_id)].point,
                nodes[static_cast<std::size_t>(next_id)].point,
                node_id,
                next_id,
                static_cast<int>(ring_index),
                true,
            };
            grid.add_edge(
                node_id,
                nodes[static_cast<std::size_t>(node_id)].point,
                nodes[static_cast<std::size_t>(next_id)].point);
        }
    }

    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>>
        heap;
    std::size_t sequence = 0;

    auto previous_node = [&](int node_id, int steps) {
        while (steps-- > 0) {
            node_id = nodes[static_cast<std::size_t>(node_id)].prev;
        }
        return node_id;
    };

    auto relabel_ring = [&](const int ring_index) {
        RingState& ring = rings[static_cast<std::size_t>(ring_index)];
        int node_id = ring.anchor;
        double label = 0.0;
        do {
            nodes[static_cast<std::size_t>(node_id)].order_label = label;
            label += 1.0;
            node_id = nodes[static_cast<std::size_t>(node_id)].next;
        } while (node_id != ring.anchor);
        ring.max_label = label - 1.0;
    };

    auto activate_edge = [&](const int from_node) {
        if (from_node < 0) {
            return;
        }
        const Node& from = nodes[static_cast<std::size_t>(from_node)];
        if (!from.alive || from.next < 0) {
            edges[static_cast<std::size_t>(from_node)].active = false;
            return;
        }

        const Node& to = nodes[static_cast<std::size_t>(from.next)];
        EdgeRecord& edge = edges[static_cast<std::size_t>(from_node)];
        edge = EdgeRecord {
            from.point,
            to.point,
            from_node,
            from.next,
            from.ring_id,
            true,
        };
        grid.add_edge(from_node, edge.a, edge.b);
    };

    auto deactivate_edge = [&](const int from_node) {
        if (from_node >= 0 &&
            static_cast<std::size_t>(from_node) < edges.size()) {
            edges[static_cast<std::size_t>(from_node)].active = false;
            ++dirty_edge_updates;
        }
    };

    auto rebuild_grid = [&]() {
        grid = SegmentGrid(cell_size);
        grid.ensure_capacity(edges.size());
        for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
            const EdgeRecord& edge = edges[edge_id];
            if (!edge.active) {
                continue;
            }
            grid.add_edge(static_cast<int>(edge_id), edge.a, edge.b);
        }
        dirty_edge_updates = 0;
    };

    auto push_candidate = [&](const int a_node) {
        if (a_node < 0 ||
            static_cast<std::size_t>(a_node) >= nodes.size()) {
            return;
        }
        const Node& a = nodes[static_cast<std::size_t>(a_node)];
        if (!a.alive) {
            return;
        }
        const RingState& ring = rings[static_cast<std::size_t>(a.ring_id)];
        if (ring.size <= 4) {
            return;
        }

        const int b_node = a.next;
        const int c_node = nodes[static_cast<std::size_t>(b_node)].next;
        const int d_node = nodes[static_cast<std::size_t>(c_node)].next;
        if (b_node < 0 || c_node < 0 || d_node < 0) {
            return;
        }
        const Node& b = nodes[static_cast<std::size_t>(b_node)];
        const Node& c = nodes[static_cast<std::size_t>(c_node)];
        const Node& d = nodes[static_cast<std::size_t>(d_node)];
        if (!b.alive || !c.alive || !d.alive || b.fixed || c.fixed) {
            return;
        }

        const std::optional<Point> replacement = compute_apsc_point(
            a.point, b.point, c.point, d.point);
        if (!replacement) {
            return;
        }

        const double cost = compute_areal_displacement(
            a.point,
            b.point,
            c.point,
            d.point,
            *replacement);

        heap.push(QueueEntry {
            normalized_queue_cost(cost),
            replacement->y,
            a.order_label,
            a_node,
            b_node,
            c_node,
            d_node,
            sequence++,
            a.ring_id,
        });
    };

    for (const RingState& ring : rings) {
        int node_id = ring.anchor;
        const std::size_t stride = candidate_seed_stride(ring.size);
        for (std::size_t i = 0; i < ring.size; ++i) {
            if (i % stride == 0) {
                push_candidate(node_id);
            }
            ++result.seeded_candidate_windows;
            node_id = nodes[static_cast<std::size_t>(node_id)].next;
        }
    }

    auto candidate_is_topology_safe_local =
        [&](const int a_node,
            const int b_node,
            const int c_node,
            const int d_node,
            const Point& replacement) {
            const Point& a = nodes[static_cast<std::size_t>(a_node)].point;
            const Point& d = nodes[static_cast<std::size_t>(d_node)].point;
            const int ring_id = nodes[static_cast<std::size_t>(a_node)].ring_id;

            auto should_skip = [&](const EdgeRecord& edge) {
                if (!edge.active) {
                    return true;
                }
                const bool is_ab = edge.from == a_node && edge.to == b_node;
                const bool is_bc = edge.from == b_node && edge.to == c_node;
                const bool is_cd = edge.from == c_node && edge.to == d_node;
                if (is_ab || is_bc || is_cd) {
                    return true;
                }
                return edge.ring_id == ring_id &&
                       (edge.from == a_node || edge.to == a_node ||
                        edge.from == d_node || edge.to == d_node);
            };

            return !grid.any_intersection(a, replacement, edges, should_skip) &&
                   !grid.any_intersection(replacement, d, edges, should_skip);
        };

    auto collect_ring_vertices =
        [&](const int ring_index,
            const int replace_a,
            const int replace_b,
            const int replace_c,
            const int replace_d,
            const Point* replacement_point) {
            std::vector<Point> vertices;
            const RingState& ring = rings[static_cast<std::size_t>(ring_index)];
            vertices.reserve(ring.size + 1);

            int node_id = ring.anchor;
            do {
                if (node_id == replace_a && replacement_point != nullptr) {
                    vertices.push_back(nodes[static_cast<std::size_t>(replace_a)].point);
                    vertices.push_back(*replacement_point);
                    node_id = replace_d;
                    continue;
                }

                if (node_id == replace_b || node_id == replace_c) {
                    node_id = nodes[static_cast<std::size_t>(node_id)].next;
                    continue;
                }

                vertices.push_back(nodes[static_cast<std::size_t>(node_id)].point);
                node_id = nodes[static_cast<std::size_t>(node_id)].next;
            } while (node_id != ring.anchor);

            return vertices;
        };

    auto point_in_vertices = [&](const std::vector<Point>& ring_vertices, const Point& point) {
        bool inside = false;
        const std::size_t count = ring_vertices.size();
        for (std::size_t i = 0, j = count - 1; i < count; j = i++) {
            const Point& va = ring_vertices[i];
            const Point& vb = ring_vertices[j];
            if (point_on_segment(point, va, vb)) {
                return true;
            }

            const bool crosses =
                ((va.y > point.y) != (vb.y > point.y)) &&
                (point.x <
                 (vb.x - va.x) * (point.y - va.y) / (vb.y - va.y + kEpsilon) + va.x);
            if (crosses) {
                inside = !inside;
            }
        }
        return inside;
    };

    auto candidate_preserves_ring_containment =
        [&](const int ring_index,
            const int a_node,
            const int b_node,
            const int c_node,
            const int d_node,
            const Point& replacement) {
            if (rings.size() <= 1) {
                return true;
            }

            if (ring_index == 0) {
                const std::vector<Point> new_exterior = collect_ring_vertices(
                    ring_index, a_node, b_node, c_node, d_node, &replacement);
                for (std::size_t other = 1; other < rings.size(); ++other) {
                    const Point& sample =
                        nodes[static_cast<std::size_t>(rings[other].anchor)].point;
                    if (!point_in_vertices(new_exterior, sample)) {
                        return false;
                    }
                }
                return true;
            }

            const std::vector<Point> new_hole = collect_ring_vertices(
                ring_index, a_node, b_node, c_node, d_node, &replacement);
            const std::vector<Point> exterior = collect_ring_vertices(0, -1, -1, -1, -1, nullptr);

            const Point& anchor_sample =
                nodes[static_cast<std::size_t>(rings[static_cast<std::size_t>(ring_index)].anchor)].point;
            if (!point_in_vertices(exterior, anchor_sample) ||
                !point_in_vertices(exterior, replacement)) {
                return false;
            }

            for (std::size_t other = 1; other < rings.size(); ++other) {
                if (static_cast<int>(other) == ring_index) {
                    continue;
                }
                const Point& other_sample =
                    nodes[static_cast<std::size_t>(rings[other].anchor)].point;
                if (point_in_vertices(new_hole, other_sample)) {
                    return false;
                }
            }

            return true;
        };

    auto materialize_polygon = [&]() {
        Polygon polygon;
        polygon.rings.reserve(rings.size());
        for (const RingState& ring : rings) {
            Ring output_ring;
            output_ring.ring_id = ring.ring_id;
            output_ring.vertices = collect_ring_vertices(
                ring.ring_id, -1, -1, -1, -1, nullptr);
            polygon.rings.push_back(std::move(output_ring));
        }
        return polygon;
    };

    auto current_ring_index_of = [&](const int ring_index, const int node_id) {
        int current = rings[static_cast<std::size_t>(ring_index)].anchor;
        std::size_t index = 0;
        while (current != node_id) {
            current = nodes[static_cast<std::size_t>(current)].next;
            ++index;
        }
        return index;
    };

    while (current_vertices > target_vertices && !heap.empty()) {
        const QueueEntry candidate = heap.top();
        heap.pop();

        if (candidate.ring_id < 0 ||
            static_cast<std::size_t>(candidate.ring_id) >= rings.size()) {
            continue;
        }

        RingState& ring = rings[static_cast<std::size_t>(candidate.ring_id)];
        if (ring.size <= 4) {
            continue;
        }

        const int a_node = candidate.a_node;
        const int b_node = candidate.b_node;
        const int c_node = candidate.c_node;
        const int d_node = candidate.d_node;
        if (a_node < 0 || b_node < 0 || c_node < 0 || d_node < 0) {
            continue;
        }
        const Node& a = nodes[static_cast<std::size_t>(a_node)];
        const Node& b = nodes[static_cast<std::size_t>(b_node)];
        const Node& c = nodes[static_cast<std::size_t>(c_node)];
        const Node& d = nodes[static_cast<std::size_t>(d_node)];
        if (!a.alive || !b.alive || !c.alive || !d.alive) {
            continue;
        }
        if (a.next != b_node || b.next != c_node || c.next != d_node) {
            continue;
        }
        if (b.fixed || c.fixed) {
            continue;
        }

        const std::optional<Point> replacement =
            compute_apsc_point(a.point, b.point, c.point, d.point);
        if (!replacement) {
            continue;
        }
        if (current_vertices <= 5000) {
            Polygon polygon = materialize_polygon();
            CollapseCandidate collapse;
            collapse.ring_id = candidate.ring_id;
            collapse.start_index =
                current_ring_index_of(candidate.ring_id, a_node);
            collapse.replacement_point = *replacement;
            collapse.estimated_areal_displacement = compute_areal_displacement(
                a.point, b.point, c.point, d.point, *replacement);
            if (!candidate_is_topology_safe(polygon, collapse)) {
                continue;
            }
        } else {
            if (!candidate_is_topology_safe_local(
                    a_node, b_node, c_node, d_node, *replacement)) {
                continue;
            }
            if (!candidate_preserves_ring_containment(
                    candidate.ring_id, a_node, b_node, c_node, d_node, *replacement)) {
                continue;
            }
        }

        result.areal_displacement += compute_areal_displacement(
            a.point, b.point, c.point, d.point, *replacement);

        deactivate_edge(a_node);
        deactivate_edge(b_node);
        deactivate_edge(c_node);

        nodes[static_cast<std::size_t>(b_node)].alive = false;
        nodes[static_cast<std::size_t>(c_node)].alive = false;

        double new_label = 0.0;
        if (d_node == ring.anchor) {
            new_label = ring.max_label + 1.0;
            ring.max_label = new_label;
        } else {
            const double a_label = nodes[static_cast<std::size_t>(a_node)].order_label;
            const double d_label = nodes[static_cast<std::size_t>(d_node)].order_label;
            if (d_label - a_label <= 1e-9) {
                relabel_ring(candidate.ring_id);
            }

            const double refreshed_a =
                nodes[static_cast<std::size_t>(a_node)].order_label;
            const double refreshed_d =
                nodes[static_cast<std::size_t>(d_node)].order_label;
            new_label = 0.5 * (refreshed_a + refreshed_d);
        }

        const int e_node = append_node(*replacement, candidate.ring_id, false, new_label);
        nodes[static_cast<std::size_t>(e_node)].prev = a_node;
        nodes[static_cast<std::size_t>(e_node)].next = d_node;
        nodes[static_cast<std::size_t>(a_node)].next = e_node;
        nodes[static_cast<std::size_t>(d_node)].prev = e_node;

        activate_edge(a_node);
        activate_edge(e_node);
        dirty_edge_updates += 2;

        --current_vertices;
        --ring.size;
        ++result.successful_collapses;

        if (dirty_edge_updates > current_vertices * 2) {
            rebuild_grid();
        }

        push_candidate(previous_node(a_node, 2));
        push_candidate(previous_node(a_node, 1));
        push_candidate(a_node);
        push_candidate(e_node);
    }

    result.polygon.rings.clear();
    result.polygon.rings.reserve(rings.size());
    for (const RingState& ring : rings) {
        Ring output_ring;
        output_ring.ring_id = ring.ring_id;
        output_ring.vertices.reserve(ring.size);

        int node_id = ring.anchor;
        do {
            output_ring.vertices.push_back(nodes[static_cast<std::size_t>(node_id)].point);
            node_id = nodes[static_cast<std::size_t>(node_id)].next;
        } while (node_id != ring.anchor);

        result.polygon.rings.push_back(std::move(output_ring));
    }
    result.output_area = total_signed_area(result.polygon);
    return result;
}

std::vector<CollapseCandidate> Simplifier::build_initial_candidates(
    const Polygon& polygon) const {
    std::vector<CollapseCandidate> candidates;

    for (const Ring& ring : polygon.rings) {
        if (ring.vertices.size() <= 4) {
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
    if (n <= 4) {
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
    if (ring.vertices.size() <= 4) {
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
               candidate.replacement_point) &&
           (polygon.rings.size() == 1 ||
            polygon_topology_is_valid(apply_candidate_to_polygon(polygon, candidate, b, c)));
}

}  // namespace apsc
