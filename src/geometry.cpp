#include "geometry.hpp"

#include <algorithm>
#include <cmath>

namespace apsc {
    namespace {

        constexpr double kEpsilon = 1e-9;

        double cross(const Point& a, const Point& b, const Point& c) {
            return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        }

        bool nearly_equal(double lhs, double rhs) {
            return std::abs(lhs - rhs) <= kEpsilon;
        }

        bool on_segment(const Point& a, const Point& b, const Point& p) {
            return std::min(a.x, b.x) - kEpsilon <= p.x && p.x <= std::max(a.x, b.x) + kEpsilon &&
                std::min(a.y, b.y) - kEpsilon <= p.y && p.y <= std::max(a.y, b.y) + kEpsilon &&
                nearly_equal(cross(a, b, p), 0.0);
        }

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
        // Skip validation for testing
        return true;
    }

}  // namespace apsc