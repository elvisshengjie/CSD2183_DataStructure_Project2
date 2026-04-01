#include "geometry.hpp"

#include <algorithm>
#include <cmath>

namespace apsc {
    namespace {

        // Small tolerance for floating-point comparisons in geometry predicates.
        constexpr double kEpsilon = 1e-9;

        // 2D cross product of AB and AC. The sign tells us the turn direction.
        double cross(const Point& a, const Point& b, const Point& c) {
            return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        }

        bool nearly_equal(double lhs, double rhs) {
            return std::abs(lhs - rhs) <= kEpsilon;
        }

        // Checks whether p lies on the closed line segment a-b, including endpoints.
        bool on_segment(const Point& a, const Point& b, const Point& p) {
            return std::min(a.x, b.x) - kEpsilon <= p.x && p.x <= std::max(a.x, b.x) + kEpsilon &&
                std::min(a.y, b.y) - kEpsilon <= p.y && p.y <= std::max(a.y, b.y) + kEpsilon &&
                nearly_equal(cross(a, b, p), 0.0);
        }

        // General segment intersection test used by both ring simplicity and ring-ring checks.
        bool segments_intersect(const Segment& first, const Segment& second) {
            const double d1 = cross(first.a, first.b, second.a);
            const double d2 = cross(first.a, first.b, second.b);
            const double d3 = cross(second.a, second.b, first.a);
            const double d4 = cross(second.a, second.b, first.b);

            const bool proper_cross = ((d1 > kEpsilon && d2 < -kEpsilon) || (d1 < -kEpsilon && d2 > kEpsilon)) &&
                ((d3 > kEpsilon && d4 < -kEpsilon) || (d3 < -kEpsilon && d4 > kEpsilon));
            if (proper_cross) {
                return true;
            }

            return (nearly_equal(d1, 0.0) && on_segment(first.a, first.b, second.a)) ||
                (nearly_equal(d2, 0.0) && on_segment(first.a, first.b, second.b)) ||
                (nearly_equal(d3, 0.0) && on_segment(second.a, second.b, first.a)) ||
                (nearly_equal(d4, 0.0) && on_segment(second.a, second.b, first.b));
        }

        // Standard ray-casting point-in-polygon test for one ring.
        bool point_in_ring(const Ring& ring, const Point& point) {
            bool inside = false;
            const std::size_t count = ring.vertices.size();
            for (std::size_t i = 0, j = count - 1; i < count; j = i++) {
                const Point& a = ring.vertices[i];
                const Point& b = ring.vertices[j];

                if (on_segment(a, b, point)) {
                    return true;
                }

                const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
                    (point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y + kEpsilon) + a.x);
                if (crosses) {
                    inside = !inside;
                }
            }
            return inside;
        }

        // Brute-force ring intersection check. This is acceptable for the starter project and
        // can later be replaced by a spatial index for better asymptotic performance.
        bool rings_intersect(const Ring& lhs, const Ring& rhs) {
            for (std::size_t i = 0; i < lhs.vertices.size(); ++i) {
                const Segment lhs_edge{
                    lhs.vertices[i],
                    lhs.vertices[(i + 1) % lhs.vertices.size()],
                };

                for (std::size_t j = 0; j < rhs.vertices.size(); ++j) {
                    const Segment rhs_edge{
                        rhs.vertices[j],
                        rhs.vertices[(j + 1) % rhs.vertices.size()],
                    };

                    if (segments_intersect(lhs_edge, rhs_edge)) {
                        return true;
                    }
                }
            }
            return false;
        }

    }  // namespace

    std::size_t Polygon::total_vertices() const {
        std::size_t total = 0;
        for (const Ring& ring : rings) {
            total += ring.vertices.size();
        }
        return total;
    }

    double signed_area(const Ring& ring) {
        if (ring.vertices.size() < 3) {
            return 0.0;
        }

        // Shoelace formula over a closed ring, where the last edge wraps to vertex 0.
        double area = 0.0;
        for (std::size_t i = 0; i < ring.vertices.size(); ++i) {
            const Point& current = ring.vertices[i];
            const Point& next = ring.vertices[(i + 1) % ring.vertices.size()];
            area += current.x * next.y - next.x * current.y;
        }
        return 0.5 * area;
    }

    double total_signed_area(const Polygon& polygon) {
        double area = 0.0;
        for (const Ring& ring : polygon.rings) {
            area += signed_area(ring);
        }
        return area;
    }

    bool is_counter_clockwise(const Ring& ring) {
        return signed_area(ring) > 0.0;
    }

    bool is_clockwise(const Ring& ring) {
        return signed_area(ring) < 0.0;
    }

    bool ring_is_simple(const Ring& ring) {
        const std::size_t count = ring.vertices.size();
        if (count < 3) {
            return false;
        }

        // Every edge is compared against non-adjacent edges only. Adjacent edges share a
        // vertex in any valid polygon and are therefore skipped.
        for (std::size_t i = 0; i < count; ++i) {
            const Segment first{
                ring.vertices[i],
                ring.vertices[(i + 1) % count],
            };

            for (std::size_t j = i + 1; j < count; ++j) {
                const bool share_endpoint =
                    i == j || (i + 1) % count == j || i == (j + 1) % count;
                if (share_endpoint) {
                    continue;
                }

                const Segment second{
                    ring.vertices[j],
                    ring.vertices[(j + 1) % count],
                };

                if (segments_intersect(first, second)) {
                    return false;
                }
            }
        }
        return true;
    }

    bool polygon_topology_is_valid(const Polygon& polygon) {
        if (polygon.rings.empty()) {
            return false;
        }

        // First verify each ring individually.
        for (std::size_t i = 0; i < polygon.rings.size(); ++i) {
            const Ring& ring = polygon.rings[i];
            if (!ring_is_simple(ring)) {
                return false;
            }

            if (i == 0 && !is_counter_clockwise(ring)) {
                return false;
            }
            if (i > 0 && !is_clockwise(ring)) {
                return false;
            }
        }

        // Then verify that every hole lies inside the exterior ring.
        const Ring& exterior = polygon.rings.front();
        for (std::size_t i = 1; i < polygon.rings.size(); ++i) {
            if (!point_in_ring(exterior, polygon.rings[i].vertices.front())) {
                return false;
            }
        }

        // Finally ensure no two distinct rings intersect each other.
        for (std::size_t i = 0; i < polygon.rings.size(); ++i) {
            for (std::size_t j = i + 1; j < polygon.rings.size(); ++j) {
                if (rings_intersect(polygon.rings[i], polygon.rings[j])) {
                    return false;
                }
            }
        }

        return true;
    }

}  // namespace apsc